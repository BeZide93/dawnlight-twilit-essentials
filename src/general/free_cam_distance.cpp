#include "free_cam_distance.hpp"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "f_op/f_op_camera_mng.h"
#include "dusk/config_var.hpp"

#include <cmath>
#include <string_view>

extern "C" ModContext* mod_ctx;

int g_configFreeCamDistance = 100;
bool g_configFreeCamDistanceZTarget = false;

DEFINE_HOOK(&dCamera_c::Run, FreeCamDistanceRunHook);
DEFINE_HOOK(&dCamera_c::bumpCheck, FreeCamDistanceBumpCheckHook);

namespace {

using GetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);

constexpr int kChaseAlgorithm = 1;
constexpr int kLockonAlgorithm = 2;
constexpr int kRideAlgorithm = 8;
constexpr int kMinPercent = 25;
constexpr int kMaxPercent = 500;
constexpr f32 kScaleEase = 0.12f;
constexpr f32 kScaleSnap = 0.001f;

const dusk::config::ConfigVar<bool>* s_freeCameraVar = nullptr;
const dusk::config::ConfigVar<bool>* s_mouseCameraVar = nullptr;

f32 s_scale = 1.0f;
bool s_applied = false;
bool s_followCamFree = false;
cXyz s_eyeOffset(0.0f, 0.0f, 0.0f);
f32 s_radiusOffset = 0.0f;

f32 s_finalScale = 1.0f;
bool s_finalUnscaled = false;
cXyz s_finalScaledEye(0.0f, 0.0f, 0.0f);
f32 s_finalScaledRadius = 0.0f;
cXyz s_finalUnscaledEye(0.0f, 0.0f, 0.0f);
f32 s_finalUnscaledRadius = 0.0f;

bool setting_on(const dusk::config::ConfigVar<bool>* var) {
    return var != nullptr && var->getValue();
}

bool is_player_camera(const dCamera_c* cam) {
    camera_process_class* playerCam =
        dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
    return playerCam != nullptr && &playerCam->mCamera == cam;
}

bool event_camera_running() {
    return dComIfGp_getEvent()->runCheck();
}

bool same_point(const cXyz& a, const cXyz& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool free_camera_active(dCamera_c* cam) {
    const bool freeSetting = setting_on(s_freeCameraVar) || setting_on(s_mouseCameraVar);
    const int algorithm = cam->mCamParam.Algorythmn();
    if (algorithm == kChaseAlgorithm || algorithm == kRideAlgorithm) {
        s_followCamFree = freeSetting || cam->mCamParam.mManualMode != 0;
        return s_followCamFree;
    }
    if (algorithm == kLockonAlgorithm) {
        return g_configFreeCamDistanceZTarget && (freeSetting || s_followCamFree);
    }
    return false;
}

f32 target_scale() {
    int percent = g_configFreeCamDistance;
    if (percent < kMinPercent) percent = kMinPercent;
    if (percent > kMaxPercent) percent = kMaxPercent;
    return static_cast<f32>(percent) / 100.0f;
}

HookAction on_camera_run_pre(ModContext*, void* args, void*, void*) {
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr || !is_player_camera(cam)) return HOOK_CONTINUE;

    s_finalUnscaled = false;
    if (s_finalScale == 1.0f) return HOOK_CONTINUE;

    const f32 shrink = 1.0f / s_finalScale;
    s_finalScale = 1.0f;
    s_finalScaledEye = cam->mEye;
    s_finalScaledRadius = cam->mDirection.R();
    cam->mEye.x = cam->mCenter.x + (cam->mEye.x - cam->mCenter.x) * shrink;
    cam->mEye.y = cam->mCenter.y + (cam->mEye.y - cam->mCenter.y) * shrink;
    cam->mEye.z = cam->mCenter.z + (cam->mEye.z - cam->mCenter.z) * shrink;
    cam->mDirection.R(s_finalScaledRadius * shrink);
    s_finalUnscaledEye = cam->mEye;
    s_finalUnscaledRadius = cam->mDirection.R();
    s_finalUnscaled = true;
    return HOOK_CONTINUE;
}

void restore_final_camera(dCamera_c* cam) {
    if (!s_finalUnscaled) return;
    s_finalUnscaled = false;
    if (!same_point(cam->mEye, s_finalUnscaledEye) ||
        cam->mDirection.R() != s_finalUnscaledRadius) {
        return;
    }
    cam->mEye = s_finalScaledEye;
    cam->mDirection.R(s_finalScaledRadius);
}

HookAction on_bump_check_pre(ModContext*, void* args, void*, void*) {
    s_applied = false;
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr || !is_player_camera(cam)) return HOOK_CONTINUE;

    if (event_camera_running()) {
        s_scale = 1.0f;
        s_finalUnscaled = false;
        return HOOK_CONTINUE;
    }

    const f32 target = free_camera_active(cam) ? target_scale() : 1.0f;
    s_scale += (target - s_scale) * kScaleEase;
    if (std::fabs(target - s_scale) < kScaleSnap) s_scale = target;
    if (s_scale == 1.0f) {
        s_finalUnscaled = false;
        return HOOK_CONTINUE;
    }

    restore_final_camera(cam);

    auto& view = cam->mViewCache;
    const f32 grow = s_scale - 1.0f;
    s_eyeOffset.x = (view.mEye.x - view.mCenter.x) * grow;
    s_eyeOffset.y = (view.mEye.y - view.mCenter.y) * grow;
    s_eyeOffset.z = (view.mEye.z - view.mCenter.z) * grow;
    s_radiusOffset = view.mDirection.R() * grow;
    view.mEye.x += s_eyeOffset.x;
    view.mEye.y += s_eyeOffset.y;
    view.mEye.z += s_eyeOffset.z;
    view.mDirection.R(view.mDirection.R() + s_radiusOffset);
    s_applied = true;
    return HOOK_CONTINUE;
}

void on_bump_check_post(ModContext*, void* args, void*, void*) {
    if (!s_applied) return;
    s_applied = false;
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr) return;
    auto& view = cam->mViewCache;
    view.mEye.x -= s_eyeOffset.x;
    view.mEye.y -= s_eyeOffset.y;
    view.mEye.z -= s_eyeOffset.z;
    view.mDirection.R(view.mDirection.R() - s_radiusOffset);
    s_finalScale = s_scale;
}

}

ModResult init_free_cam_distance(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;

    void* addr = nullptr;
    if (hook_svc->resolve(mod_ctx, "dusk::config::GetConfigVar", &addr, nullptr) == MOD_OK &&
        addr != nullptr) {
        const auto getVar = reinterpret_cast<GetConfigVarFn>(addr);
        s_freeCameraVar =
            static_cast<const dusk::config::ConfigVar<bool>*>(getVar("game.freeCamera"));
        s_mouseCameraVar =
            static_cast<const dusk::config::ConfigVar<bool>*>(getVar("game.enableMouseCamera"));
    }

    ModResult result = mods::hook::add_pre<FreeCamDistanceRunHook>(hook_svc, on_camera_run_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<FreeCamDistanceBumpCheckHook>(hook_svc, on_bump_check_pre);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<FreeCamDistanceBumpCheckHook>(hook_svc, on_bump_check_post);
}

void shutdown_free_cam_distance() {
    s_scale = 1.0f;
    s_applied = false;
    s_followCamFree = false;
    s_finalScale = 1.0f;
    s_finalUnscaled = false;
}
