#include "skip_cutscenes.hpp"

#include "d/d_event.h"

bool g_configGeneralSkipCutscenes = false;

bool is_boss_rush_active();

DEFINE_HOOK(&dEvt_control_c::skipper, GeneralSkipperHook);

static const int kArmFrames = 12;
static const int kCooldownFrames = 30;

static int s_skipArm = 0;
static int s_skipCooldown = 0;

static HookAction on_skipper_pre(ModContext*, void* args, void*, void*) {
    if ((!g_configGeneralSkipCutscenes && !is_boss_rush_active()) || !args) return HOOK_CONTINUE;

    dEvt_control_c* evt = mods::arg<dEvt_control_c*>(args, 0);
    if (!evt) return HOOK_CONTINUE;

    if (s_skipCooldown > 0) {
        s_skipCooldown--;
        s_skipArm = 0;
        return HOOK_CONTINUE;
    }

    if (evt->mEventStatus == 1 && evt->mSkipFunc != NULL) {
        if (++s_skipArm >= kArmFrames) {
            evt->mSkipTimer = -100;
            s_skipArm = 0;
            s_skipCooldown = kCooldownFrames;
        }
    } else {
        s_skipArm = 0;
    }

    return HOOK_CONTINUE;
}

ModResult init_skip_cutscenes(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<GeneralSkipperHook>(hook_svc, on_skipper_pre);
    return MOD_OK;
}

void shutdown_skip_cutscenes() {
    s_skipArm = 0;
    s_skipCooldown = 0;
}
