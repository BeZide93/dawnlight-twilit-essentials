#include "sprint_swim.hpp"

#include "stamina.hpp"
#include "stamina_internal.hpp"
#include "../controls/controls.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"

bool g_configStaminaSwimSprint = false;
float g_configStaminaSwimSprintSpeed = 1.15f;

static constexpr int kSprintHoldFrames = 3;
static constexpr f32 kSwimSprintDrainRate = 0.85f;

static bool s_swimSprinting = false;
static int  s_holdFrames = 0;

static bool is_zora_tunic(const daAlink_c*) {
    return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e;
}

static bool sprint_swim_wanted(const daAlink_c* link) {
    if (!g_configStaminaSwimSprint || !stamina_impl::in_gameplay()) return false;
    if (!link || !link->mpHIO) return false;
    if (link->checkWolf()) return false;
    if (is_zora_tunic(link)) return false;
    if (link->mProcID != daAlink_c::PROC_SWIM_MOVE) return false;

    const bool sprintHeld = controls_binding_held(CTRL_BIND_SPRINT);
    if (!sprintHeld) return false;

    if (g_configStaminaEnabled && g_configStaminaSrcSwim && stamina_impl::is_empty()) {
        return false;
    }

    if (s_holdFrames < kSprintHoldFrames && !s_swimSprinting) {
        return false;
    }

    return true;
}

void update_sprint_swim() {
    if (!g_configStaminaSwimSprint || !stamina_impl::in_gameplay()) {
        s_holdFrames = 0;
        s_swimSprinting = false;
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (!link || !link->mpHIO) {
        s_holdFrames = 0;
        s_swimSprinting = false;
        return;
    }

    const bool sprintHeld = controls_binding_held(CTRL_BIND_SPRINT);
    if (sprintHeld) {
        if (s_holdFrames < 0xFF) s_holdFrames++;
    } else {
        s_holdFrames = 0;
    }

    if (!sprint_swim_wanted(link)) {
        if (s_swimSprinting) {
            s_swimSprinting = false;
            link->offNoResetFlg1(daPy_py_c::FLG1_DASH_MODE);
            link->field_0x30d0 = 0;
        }
        return;
    }

    s_swimSprinting = true;

    link->onNoResetFlg1(daPy_py_c::FLG1_DASH_MODE);
    link->field_0x30d0 = link->mpHIO->mSwim.m.field_0x5c;
    link->field_0x30d2 = 0;

    const f32 baseDashSpeed = link->mpHIO->mSwim.m.mDashMaxSpeed;
    const f32 targetSpeed = baseDashSpeed * g_configStaminaSwimSprintSpeed;
    link->mMaxSpeed = targetSpeed;
    if (link->mNormalSpeed < targetSpeed) {
        link->mNormalSpeed += 0.8f;
        if (link->mNormalSpeed > targetSpeed) {
            link->mNormalSpeed = targetSpeed;
        }
    }

    if (g_configStaminaEnabled && g_configStaminaSrcSwim) {
        stamina_impl::report_drain(stamina_impl::cost_scaled(kSwimSprintDrainRate, g_configStaminaCostSwimSprint));
    }
}

DEFINE_HOOK(&daAlink_c::procSwimMove, SprintSwimAnm);

static void swim_move_post(ModContext*, void* args, void*, void*) {
    if (!s_swimSprinting) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link) return;
    daPy_frameCtrl_c& fc = link->mUnderFrameCtrl[0];
    fc.setRate(fc.getRate() *
               stamina_impl::sprint_anim_speed_mul(g_configStaminaSwimSprintSpeed));
}

ModResult init_sprint_swim(const HookService* hook_svc) {
    if (!hook_svc) return MOD_OK;
    mods::hook::add_post<SprintSwimAnm>(hook_svc, swim_move_post);
    return MOD_OK;
}

void shutdown_sprint_swim() {
    s_swimSprinting = false;
    s_holdFrames = 0;
}
