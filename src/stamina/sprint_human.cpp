#include "sprint_human.hpp"

#include "stamina.hpp"
#include "stamina_internal.hpp"
#include "../controls/controls.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_math.h"

#include <cmath>

bool g_configStaminaSprint     = false;
bool g_configStaminaSrcSprint  = true;

static constexpr int kSprintHoldFrames = 4;
static constexpr f32 kSprintDrainRate  = 0.90f;

static constexpr int kRollRepeatGuardFrames = 10;

static constexpr f32 kHouseAnimSpeedMul = 0.80f;

float g_configStaminaSprintSpeed = 1.1f;
bool g_configStaminaSprintStartRoll = false;

static bool s_sprintLatched = false;
static bool s_sprintBoost   = false;
static bool s_sprintEngage  = false;
static int  s_holdFrames    = 0;

static bool s_sprintRollPending  = false;
static int  s_frameCounter       = 0;
static int  s_lastFrontRollFrame = -100;

static bool in_front_roll(const daAlink_c* link) {
    const u16 proc = static_cast<u16>(link->mProcID);
    return proc == daAlink_c::PROC_FRONT_ROLL || proc == daAlink_c::PROC_FRONT_ROLL_CRASH ||
           proc == daAlink_c::PROC_FRONT_ROLL_SUCCESS;
}

DEFINE_HOOK(&daAlink_c::setDoubleAnime, SprintHumanRunAnm);

static HookAction sprint_run_pre(ModContext*, void* args, void*, void*) {
    if (!stamina_impl::in_gameplay() || !args) {
        s_sprintLatched = false;
        return HOOK_CONTINUE;
    }
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);

    if (link != nullptr && link->checkHorseRide()) {
        s_sprintLatched = false;
        return HOOK_CONTINUE;
    }

    if (g_configStaminaEnabled && stamina_impl::is_empty()) {
        s_sprintLatched = false;
        return HOOK_CONTINUE;
    }

    if (!g_configStaminaSprint) {
        s_sprintLatched = false;
        return HOOK_CONTINUE;
    }

    const bool sprintHeld = controls_binding_held(CTRL_BIND_SPRINT);
    const bool drains   = g_configStaminaSrcSprint && g_configStaminaEnabled;
    const bool wasLatched = s_sprintLatched;

    if (!sprintHeld || (drains && stamina_impl::is_empty())) {
        s_sprintLatched = false;
        return HOOK_CONTINUE;
    }
    if (s_holdFrames < kSprintHoldFrames) return HOOK_CONTINUE;
    if (drains && !wasLatched &&
        stamina_impl::current_value() < 0.25f * stamina_impl::max_value()) {
        return HOOK_CONTINUE;
    }
    s_sprintLatched = true;

    if (!link->checkEquipAnime()) {
        if (link->mEquipItem == 0x103) {
            link->swordUnequip();
        } else if (link->mEquipItem != dItemNo_NONE_e &&
                   link->mEquipItem != dItemNo_KANTERA_e &&
                   link->mEquipItem != dItemNo_KANTERA2_e) {
            link->deleteEquipItem(FALSE, TRUE);
        }
    }

    int& anmA = mods::arg_ref<int>(args, 4);
    int& anmB = mods::arg_ref<int>(args, 5);
    if (anmA != daAlink_c::ANM_RUN && anmB != daAlink_c::ANM_RUN) return HOOK_CONTINUE;

    f32 animSpeedMul = stamina_impl::sprint_anim_speed_mul(g_configStaminaSprintSpeed);
    if (daAlink_c::checkRoom()) {
        animSpeedMul *= kHouseAnimSpeedMul;
    }
    f32& speedA = mods::arg_ref<f32>(args, 2);
    f32& speedB = mods::arg_ref<f32>(args, 3);
    if (anmA == daAlink_c::ANM_RUN) {
        anmA = daAlink_c::ANM_RUN_B;
        speedA *= animSpeedMul;
    }
    if (anmB == daAlink_c::ANM_RUN) {
        anmB = daAlink_c::ANM_RUN_B;
        speedB *= animSpeedMul;
    }
    if (drains) stamina_impl::report_drain(stamina_impl::cost_scaled(kSprintDrainRate, g_configStaminaCostSprint));
    s_sprintBoost = true;
    if (!wasLatched) {
        s_sprintEngage = true;
        if (g_configStaminaSprintStartRoll) s_sprintRollPending = true;
    }
    return HOOK_CONTINUE;
}

static void sprint_run_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link || !link->mpHIO) return;

    if (s_sprintBoost) {
        s_sprintBoost = false;
        const f32 base = link->mpHIO->mMove.m.mMaxSpeed;
        link->mMaxSpeed = base * g_configStaminaSprintSpeed;

        if (s_sprintEngage) {
            s_sprintEngage = false;
            if (link->mNormalSpeed >= 0.9f * base) {
                link->mNormalSpeed = link->mMaxSpeed;
            }
        }
        return;
    }

    if (stamina_impl::is_empty() && link->mProcID == daAlink_c::PROC_MOVE) {
        link->mMaxSpeed = link->mpHIO->mMove.m.mMaxSpeed * stamina_impl::kExhaustedSpeedMul;
    }
}

static u32 s_sprintWindEmitter = 0;

static void stop_sprint_wind_effect() {
    if (s_sprintWindEmitter == 0) return;
    JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(s_sprintWindEmitter);
    if (emitter != nullptr) {
        emitter->stopDrawParticle();
    }
    s_sprintWindEmitter = 0;
}

static void update_sprint_wind_effect(daAlink_c* link) {
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    if (camera == nullptr) {
        stop_sprint_wind_effect();
        return;
    }
    cXyz* eye_p = fopCamM_GetEye_p(camera);

    const s16 rel = link->shape_angle.y - fopCamM_GetAngleY(camera);
    const f32 t1 = (200.0f * std::abs(cM_ssin(rel))) + (700.0f * std::abs(cM_scos(rel)));
    const f32 t0 = t1 * cM_scos(fopCamM_GetAngleX(camera));

    cXyz pos;
    pos.x = eye_p->x + (t0 * cM_ssin(fopCamM_GetAngleY(camera)));
    pos.y = eye_p->y + (t1 * cM_ssin(-fopCamM_GetAngleX(camera)));
    pos.z = eye_p->z + (t0 * cM_scos(fopCamM_GetAngleY(camera)));

    csXyz angle(0, link->shape_angle.y, 0);

    if (s_sprintWindEmitter != 0) {
        JPABaseEmitter* existing = dComIfGp_particle_getEmitter(s_sprintWindEmitter);
        if (existing == nullptr || existing->isEnableDeleteEmitter()) {
            s_sprintWindEmitter = 0;
        }
    }

    f32 speedRatio = 2.0f * (link->speedF / link->mMaxSpeed);
    if (speedRatio > 1.0f) speedRatio = 1.0f;
    const u8 alpha = static_cast<u8>(255.0f * speedRatio);

    s_sprintWindEmitter = dComIfGp_particle_set(s_sprintWindEmitter, 0x8657, &pos, &link->tevStr,
                                                &angle, nullptr, alpha, nullptr, -1,
                                                nullptr, nullptr, nullptr);
}

void update_sprint_human() {
    ++s_frameCounter;

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link != nullptr && in_front_roll(link)) {
        s_lastFrontRollFrame = s_frameCounter;
    }

    if (s_sprintRollPending) {
        s_sprintRollPending = false;
        if (g_configStaminaSprintStartRoll && stamina_impl::in_gameplay() && s_sprintLatched &&
            s_frameCounter - s_lastFrontRollFrame > kRollRepeatGuardFrames) {
            link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
            if (link != nullptr && !link->checkWolf() &&
                link->mProcID == daAlink_c::PROC_MOVE) {
                link->procFrontRollInit();
            }
        }
    }

    if (!g_configStaminaSprint || !stamina_impl::in_gameplay()) {
        s_holdFrames = 0;
        stop_sprint_wind_effect();
        return;
    }
    if (controls_binding_held(CTRL_BIND_SPRINT)) {
        if (s_holdFrames < 0xFF) s_holdFrames++;
    } else {
        s_holdFrames = 0;
    }

    if (s_sprintLatched && link != nullptr && !link->checkHorseRide()) {
        update_sprint_wind_effect(link);
    } else {
        stop_sprint_wind_effect();
    }
}

DEFINE_HOOK(&mDoCPd_c::read, SprintHumanPadRead);

static void sprint_pad_read_post(ModContext*, void*, void*, void*) {
    if (!g_configStaminaSprint) return;

    {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link != nullptr && link->checkHorseRide()) {
            return;
        }
    }

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const bool sprintHold = controls_binding_held(CTRL_BIND_SPRINT) &&
                            s_holdFrames >= kSprintHoldFrames;
    if (!s_sprintLatched && !sprintHold) return;

    if (s_sprintLatched) {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link && link->mEquipItem != 0x103 && !link->checkEquipAnime()) return;
    }

    pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
    pad.mButtonFlags &= ~PAD_BUTTON_B;
}

DEFINE_HOOK(&daAlink_c::checkItemAction, SprintHumanJumpAttack);

static HookAction sprint_jump_attack_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_configStaminaSprint || !s_sprintLatched || !args || !retval) {
        return HOOK_CONTINUE;
    }

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->mEquipItem == 0x103 || link->checkEquipAnime()) {
        return HOOK_CONTINUE;
    }

    if (daAlink_c::checkNotBattleStage() || dComIfGs_getSelectEquipSword() == dItemNo_NONE_e) {
        return HOOK_CONTINUE;
    }

    if (!link->swordSwingTrigger()) return HOOK_CONTINUE;

    link->deleteEquipItem(FALSE, TRUE);
    link->swordEquip(TRUE);
    link->setSwordModel();
    s_sprintLatched = false;

    BOOL result = link->procCutJumpInit(FALSE);
    *static_cast<BOOL*>(retval) = result;
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&daAlink_c::itemEquip, SprintHumanItemEquip);

static HookAction sprint_item_equip_pre(ModContext*, void* args, void*, void*) {
    if (!s_sprintLatched || args == nullptr) {
        return HOOK_CONTINUE;
    }
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const u16 itemId = mods::arg<u16>(args, 1);
    if (link == nullptr || itemId != dItemNo_KANTERA_e) {
        return HOOK_CONTINUE;
    }
    if (link->mEquipItem == dItemNo_NONE_e && link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1)) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

ModResult init_sprint_human(const HookService* hook_svc) {
    if (!hook_svc) return MOD_OK;
    mods::hook::add_pre<SprintHumanRunAnm>(hook_svc, sprint_run_pre);
    mods::hook::add_post<SprintHumanRunAnm>(hook_svc, sprint_run_post);
    mods::hook::add_post<SprintHumanPadRead>(hook_svc, sprint_pad_read_post);
    mods::hook::add_pre<SprintHumanJumpAttack>(hook_svc, sprint_jump_attack_pre);
    mods::hook::add_pre<SprintHumanItemEquip>(hook_svc, sprint_item_equip_pre);
    return MOD_OK;
}

void shutdown_sprint_human() {
    s_sprintLatched = s_sprintBoost = s_sprintEngage = false;
    s_sprintRollPending = false;
    s_holdFrames = 0;
    stop_sprint_wind_effect();
}
