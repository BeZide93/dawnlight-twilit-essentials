#include "sprint_fov_kick.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_camera.h"
#include "d/d_event.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"

#include <cmath>

bool g_configSprintFovKickEnabled = false;

static constexpr f32 kMaxKickDeg = 8.0f;
static constexpr f32 kSprintSpeedFactor = 1.06f;

static f32 s_kick = 0.0f;
static f32 s_lastWritten = 0.0f;
static f32 s_lastApplied = 0.0f;
static bool s_wroteFovy = false;

static bool in_gameplay() {
    if (dComIfGp_getPlayer(0) == nullptr) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    if (dComIfGp_getEvent()->runCheck()) return false;
    return true;
}

DEFINE_HOOK(&dCamera_c::Run, SprintFovKickCameraRunHook);

static void on_camera_run_post(ModContext*, void*, void*, void*) {
    if (!g_configSprintFovKickEnabled) return;
    if (s_kick <= 0.001f && (!s_wroteFovy || s_lastApplied <= 0.0f)) return;

    camera_process_class* cam = dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
    if (cam == nullptr) return;

    const f32 cur = cam->mCamera.mFovy;
    const f32 baseline =
        (!s_wroteFovy || cur != s_lastWritten) ? cur : cur - s_lastApplied;
    if (baseline <= 0.0f || std::isnan(baseline)) {
        s_wroteFovy = false;
        return;
    }

    const f32 kickDeg = s_kick * kMaxKickDeg;
    cam->mCamera.mFovy = baseline + kickDeg;
    s_lastWritten = baseline + kickDeg;
    s_lastApplied = kickDeg;
    s_wroteFovy = true;
}

void update_sprint_fov_kick() {
    if (!g_configSprintFovKickEnabled || !in_gameplay()) {
        s_kick = 0.0f;
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    bool sprinting = false;
    if (link != nullptr && link->mpHIO != nullptr && !link->checkHorseRide()) {
        const u16 proc = link->mProcID;
        const bool inSprintProc = proc == daAlink_c::PROC_MOVE ||
                                  proc == daAlink_c::PROC_WOLF_DASH ||
                                  proc == daAlink_c::PROC_WOLF_DASH_REVERSE;
        if (inSprintProc) {
            const f32 hspeed =
                std::sqrt(link->speed.x * link->speed.x + link->speed.z * link->speed.z);
            const f32 runBase = link->mpHIO->mMove.m.mMaxSpeed;
            sprinting = runBase > 0.0f && hspeed > runBase * kSprintSpeedFactor;
        }
    }

    s_kick += ((sprinting ? 1.0f : 0.0f) - s_kick) * (sprinting ? 0.10f : 0.13f);
    if (s_kick < 0.001f) s_kick = 0.0f;
    if (s_kick > 1.0f) s_kick = 1.0f;
}

ModResult init_sprint_fov_kick(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    return mods::hook::add_post<SprintFovKickCameraRunHook>(hook_svc, on_camera_run_post);
}

void shutdown_sprint_fov_kick() {
    s_kick = 0.0f;
    s_lastApplied = 0.0f;
    s_wroteFovy = false;
}
