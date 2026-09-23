#include "sprint_wolf.hpp"

#include "stamina.hpp"
#include "stamina_internal.hpp"
#include "../controls/controls.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"
#include "SSystem/SComponent/c_math.h"

#include <cmath>

bool g_configStaminaWolfSprint = false;
float g_configStaminaWolfSprintSpeed = 1.1f;

DEFINE_HOOK(&daAlink_c::procWolfMove, SprintWolfRedash);
DEFINE_HOOK(&daAlink_c::procWolfDashInit, SprintWolfDashInit);
DEFINE_HOOK(&daAlink_c::setFaceBasicTexture, SprintWolfTongueFace);
DEFINE_HOOK(&daAlink_c::setDoubleAnimeWolf, SprintWolfRunAnm);

static constexpr int kBurstIntervalFrames = 90;
static constexpr f32 kWolfSprintDrainRate = 0.90f;
static constexpr int kMinSprintRunFrames = 6;

static int  s_burstTimer     = 0;
static bool s_wasSprinting   = false;
static int  s_sprintRunFrames = 0;
static bool s_tongueOut      = false;

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

static HookAction tongue_face_pre(ModContext*, void* args, void*, void*) {
    if (!g_configStaminaEnabled || !stamina_impl::is_empty() || !args) return HOOK_CONTINUE;
    int& ftanm = mods::arg_ref<int>(args, 1);
    if (ftanm == daAlink_c::FTANM_WL_MABA01) ftanm = daAlink_c::FTANM_WL_MABA02;
    return HOOK_CONTINUE;
}

static void refresh_wolf_tongue(daAlink_c* link) {
    if (!link || !g_configStaminaEnabled) return;
    if (stamina_impl::is_empty()) {
        if (!s_tongueOut) {
            s_tongueOut = true;
            link->setFaceBasicTexture(daAlink_c::FTANM_WL_MABA02);
        }
    } else if (s_tongueOut) {
        s_tongueOut = false;
        link->setFaceBasicTexture(daAlink_c::FTANM_WL_MABA01);
    }
}

static bool sprint_wanted(const daAlink_c* link) {
    if (!g_configStaminaWolfSprint || !stamina_impl::in_gameplay()) {
        return false;
    }
    if (!link || !link->mpHIO) return false;
    if (!controls_binding_held(CTRL_BIND_SPRINT)) return false;
    if (g_configStaminaEnabled && g_configStaminaSrcWolfDash && stamina_impl::is_empty()) return false;
    return true;
}

static void apply_dash_speed(daAlink_c* link) {
    const daAlinkHIO_wlMove_c1& wl = link->mpHIO->mWolf.mWlMove.m;
    f32 dashMax;
    if (link->checkWolfSlowDash()) {
        dashMax = wl.mADashMaxSpeedSlow;
    } else if (link->field_0x2fc7 == 2) {
        dashMax = wl.mADashMaxSpeedSlow2;
    } else {
        dashMax = wl.mADashMaxSpeed;
    }
    link->mMaxSpeed = dashMax * g_configStaminaWolfSprintSpeed;
}

static void top_up_dash_duration(daAlink_c* link) {
    const daAlinkHIO_wlMove_c1& wl = link->mpHIO->mWolf.mWlMove.m;
    if (link->checkWolfSlowDash()) {
        link->field_0x30d0 = wl.mADashDurationSlow;
    } else if (link->field_0x2fc7 == 2) {
        link->field_0x30d0 = wl.mADashDurationSlow2;
    } else {
        link->field_0x30d0 = wl.mADashDuration;
    }
}

static void wolf_dash_init_post(ModContext*, void* args, void*, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaWolfSprint || !stamina_impl::in_gameplay()) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link || (g_configStaminaSrcWolfDash && stamina_impl::is_empty())) return;
    apply_dash_speed(link);
    s_burstTimer = 0;
}

static HookAction wolf_move_pre(ModContext*, void* args, void* retval, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    refresh_wolf_tongue(link);
    if (!sprint_wanted(link)) {
        s_burstTimer = 0;
        if (s_wasSprinting && s_sprintRunFrames >= kMinSprintRunFrames && link) {
            link->field_0x30d0 = 0;
            link->offNoResetFlg1(daPy_py_c::FLG1_DASH_MODE);
            const f32 runMax = link->mpHIO->mWolf.mWlMoveNoP.m.mMaxSpeed;
            if (link->mNormalSpeed > runMax) link->mNormalSpeed = runMax;
        }
        s_wasSprinting = false;
        s_sprintRunFrames = 0;
        stop_sprint_wind_effect();
        if (link && link->mpHIO && g_configStaminaEnabled && stamina_impl::is_empty()) {
            link->mMaxSpeed = link->mpHIO->mWolf.mWlMoveNoP.m.mMaxSpeed * stamina_impl::kExhaustedSpeedMul;
        }
        return HOOK_CONTINUE;
    }
    const bool wasSprinting = s_wasSprinting;
    s_wasSprinting = true;
    s_sprintRunFrames++;
    update_sprint_wind_effect(link);
    link->onNoResetFlg1(daPy_py_c::FLG1_DASH_MODE);
    top_up_dash_duration(link);
    apply_dash_speed(link);
    if (!wasSprinting) {
        link->mNormalSpeed = link->mMaxSpeed;
    }
    if (g_configStaminaEnabled && g_configStaminaSrcWolfDash) {
        stamina_impl::report_drain(stamina_impl::cost_scaled(kWolfSprintDrainRate, g_configStaminaCostWolfSprint));
    }

    if (++s_burstTimer < kBurstIntervalFrames) return HOOK_CONTINUE;
    s_burstTimer = 0;

    link->procWolfDashInit();
    apply_dash_speed(link);
    if (retval) *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

static HookAction wolf_run_anm_pre(ModContext*, void* args, void*, void*) {
    if (!s_wasSprinting) return HOOK_CONTINUE;
    int& anmA = mods::arg_ref<int>(args, 4);
    int& anmB = mods::arg_ref<int>(args, 5);
    if (anmA != daAlink_c::WANM_DASH_B && anmB != daAlink_c::WANM_DASH_B) return HOOK_CONTINUE;
    const f32 mul = stamina_impl::sprint_anim_speed_mul(g_configStaminaWolfSprintSpeed);
    if (anmA == daAlink_c::WANM_DASH_B) mods::arg_ref<f32>(args, 2) *= mul;
    if (anmB == daAlink_c::WANM_DASH_B) mods::arg_ref<f32>(args, 3) *= mul;
    return HOOK_CONTINUE;
}

ModResult init_sprint_wolf(const HookService* hook_svc) {
    if (!hook_svc) return MOD_OK;
    mods::hook::add_pre<SprintWolfRedash>(hook_svc, wolf_move_pre);
    mods::hook::add_post<SprintWolfDashInit>(hook_svc, wolf_dash_init_post);
    mods::hook::add_pre<SprintWolfTongueFace>(hook_svc, tongue_face_pre);
    mods::hook::add_pre<SprintWolfRunAnm>(hook_svc, wolf_run_anm_pre);
    return MOD_OK;
}

void shutdown_sprint_wolf() {
    s_burstTimer = 0;
    s_wasSprinting = false;
    s_sprintRunFrames = 0;
    s_tongueOut = false;
    stop_sprint_wind_effect();
}
