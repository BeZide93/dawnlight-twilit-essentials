#include "fast_forward_cutscenes.hpp"
#include "skip_cutscenes.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_manager.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "d/d_s_play.h"
#include "dusk/settings.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_midna.h"
#include "human_warp.hpp"

#include <cstring>

int g_configGeneralFastForwardCutscenesMode = FF_CUTSCENES_OFF;

bool is_boss_rush_active();
bool boss_rush_is_fighting_here();
bool boss_rush_is_returning_to_chamber();
bool boss_rush_settle_window_active();
bool boss_rush_is_fight_retry_warp();
bool is_in_boss_rush_chamber();
bool boss_bar_boss_defeated_now();
bool boss_bar_current_fight_state(const char** outLabel, bool& outEngaged);

namespace {

using SetTimescaleFn = void (*)(float);
using GetTimescaleFn = float (*)();
using SetSimRateFn = void (*)(float);
using GetSimRateFn = float (*)();
using GetTransientSettingsFn = dusk::TransientSettings& (*)();

constexpr float kBaseSimHz = 30.0f;

constexpr float kFastForwardScale = 8.0f;
constexpr float kVeryFastForwardScale = 12.0f;
constexpr float kTurboScale = 4.0f;
constexpr float kHiddenRunScale = 16.0f;
constexpr int kLeadFrames = 5;
constexpr int kFightStartHoldFrames = 60;
constexpr int kDefeatMinHoldFrames = 120;
constexpr int kTurboReleaseWatchFrames = 6;

SetTimescaleFn s_setTimescale = nullptr;
GetTimescaleFn s_getTimescale = nullptr;
SetTimescaleFn s_setClockScale = nullptr;
GetTimescaleFn s_getClockScale = nullptr;
SetSimRateFn s_setSimRate = nullptr;
GetSimRateFn s_getSimRate = nullptr;
GetTransientSettingsFn s_getTransientSettings = nullptr;

bool s_active = false;
float s_desiredScale = 1.0f;
int s_confirmFrames = 0;
bool s_holdActive = false;
int s_holdSettleFrames = 0;
int s_holdMinFrames = 0;
bool s_wasFightLive = false;
int s_fightStartFrames = -1;
bool s_prevTurbo = false;
int s_turboReleaseFrames = 0;

DEFINE_HOOK_SYMBOL("aurora_get_timescale", float(), AuroraGetTimescaleHook);
DEFINE_HOOK_SYMBOL("aurora_set_timescale", void(float), AuroraSetTimescaleHook);

bool s_hiddenRun = false;
float s_hostScale = 1.0f;

float sane_host_scale(float scale) {
    return scale > 0.0f && scale < kHiddenRunScale ? scale : 1.0f;
}

bool clock_available();

bool turbo_held(bool& known) {
    known = s_getTransientSettings != nullptr;
    return known && s_getTransientSettings().turboMode;
}

void on_aurora_get_timescale_post(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) return;
    if (s_hiddenRun) {
        *static_cast<float*>(retval) = s_hostScale;
        return;
    }
    if (s_active) *static_cast<float*>(retval) = s_desiredScale;
}

HookAction on_aurora_set_timescale_pre(ModContext*, void* args, void*, void*) {
    float& requested = mods::arg_ref<float>(args, 0);
    if (requested >= kHiddenRunScale) requested = 1.0f;
    if (requested > 0.0f) s_hostScale = requested;
    if (s_hiddenRun) return HOOK_SKIP_ORIGINAL;
    if (s_active) {
        s_desiredScale = sane_host_scale(requested);
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

enum ClockBackend { kBackendC = 0, kBackendSimRate, kBackendCpp, kBackendCount };
bool s_backendBad[kBackendCount] = {};
int s_backend = -1;

bool backend_available(int backend) {
    switch (backend) {
    case kBackendC:
        return (AuroraSetTimescaleHook::g_orig != nullptr || s_setTimescale != nullptr) &&
               (AuroraGetTimescaleHook::g_orig != nullptr || s_getTimescale != nullptr);
    case kBackendSimRate:
        return s_setSimRate != nullptr && s_getSimRate != nullptr;
    case kBackendCpp:
        return s_setClockScale != nullptr && s_getClockScale != nullptr;
    default:
        return false;
    }
}

float backend_get(int backend) {
    switch (backend) {
    case kBackendC:
        if (AuroraGetTimescaleHook::g_orig != nullptr) return AuroraGetTimescaleHook::g_orig();
        return s_getTimescale();
    case kBackendSimRate:
        return s_getSimRate() / kBaseSimHz;
    case kBackendCpp:
        return s_getClockScale();
    default:
        return 1.0f;
    }
}

void backend_set(int backend, float scale) {
    switch (backend) {
    case kBackendC:
        if (AuroraSetTimescaleHook::g_orig != nullptr) {
            AuroraSetTimescaleHook::g_orig(scale);
        } else {
            s_setTimescale(scale);
        }
        break;
    case kBackendSimRate:
        s_setSimRate(scale * kBaseSimHz);
        break;
    case kBackendCpp:
        s_setClockScale(scale);
        break;
    default:
        break;
    }
}

int current_backend() {
    if (s_backend >= 0 && !s_backendBad[s_backend]) return s_backend;
    for (int b = 0; b < kBackendCount; b++) {
        if (!s_backendBad[b] && backend_available(b)) {
            s_backend = b;
            return b;
        }
    }
    s_backend = -1;
    return -1;
}

float live_timescale() {
    const int backend = current_backend();
    return backend >= 0 ? backend_get(backend) : 1.0f;
}

void force_set_timescale(float scale) {
    for (int attempt = 0; attempt < kBackendCount; attempt++) {
        const int backend = current_backend();
        if (backend < 0) return;
        backend_set(backend, scale);
        const float readBack = backend_get(backend);
        if (readBack == scale || (scale > 1.0f && readBack > 1.0f) || (scale < 1.0f && readBack < 1.0f)) {
            return;
        }
        s_backendBad[backend] = true;
    }
}

void own_set_timescale(float scale) {
    if (live_timescale() == scale) return;
    force_set_timescale(scale);
}

bool clock_available() {
    return current_backend() >= 0;
}

float resolved_desired_scale() {
    bool known = false;
    const bool turbo = turbo_held(known);
    if (known && !turbo && s_desiredScale == kTurboScale) return 1.0f;
    return sane_host_scale(s_desiredScale);
}

void stop_fast_forward() {
    if (!s_active) return;
    s_active = false;
    own_set_timescale(resolved_desired_scale());
}

void update_turbo_release_watch() {
    bool known = false;
    const bool turbo = turbo_held(known);
    if (!known) return;
    if (s_prevTurbo && !turbo) s_turboReleaseFrames = kTurboReleaseWatchFrames;
    s_prevTurbo = turbo;
    if (s_turboReleaseFrames <= 0) return;
    --s_turboReleaseFrames;
    if (!s_active && !s_hiddenRun && live_timescale() == kTurboScale) {
        own_set_timescale(1.0f);
    }
}

bool is_boss_rush_defeat_hold() {
    if (!is_boss_rush_active()) {
        s_holdSettleFrames = 0;
        s_holdActive = false;
        s_holdMinFrames = 0;
        return false;
    }

    if (s_holdMinFrames > 0) --s_holdMinFrames;

    if (boss_rush_is_fighting_here()) {
        s_holdSettleFrames = 0;
        if (boss_bar_boss_defeated_now()) {
            s_holdActive = true;
            s_holdMinFrames = kDefeatMinHoldFrames;
            return true;
        }

        const char* label = nullptr;
        bool engaged = false;
        if (s_holdMinFrames <= 0 && boss_bar_current_fight_state(&label, engaged) && engaged) {
            s_holdActive = false;
        }
        return s_holdActive;
    }

    if (boss_rush_is_returning_to_chamber()) {
        s_holdSettleFrames = 0;
        return true;
    }

    if (is_in_boss_rush_chamber() && !boss_rush_settle_window_active()) {
        if (++s_holdSettleFrames > 30 && s_holdMinFrames <= 0) {
            s_holdSettleFrames = 0;
            s_holdActive = false;
        }
        return s_holdActive;
    }

    s_holdSettleFrames = 0;
    return s_holdActive;
}

void update_boss_rush_fight_start_hold() {
    const bool fightLive = is_boss_rush_active() &&
                           (boss_rush_is_fighting_here() || boss_rush_is_fight_retry_warp());
    if (fightLive && !s_wasFightLive) {
        s_fightStartFrames = 0;
    }
    s_wasFightLive = fightLive;

    if (s_fightStartFrames >= 0) {
        if (s_fightStartFrames >= kFightStartHoldFrames) {
            s_fightStartFrames = -1;
        } else {
            ++s_fightStartFrames;
        }
    }
}

bool is_boss_rush_fight_start_hold() {
    return s_fightStartFrames >= 0;
}

bool is_door_actor(fopAc_ac_c* actor) {
    if (actor == nullptr) return false;
    static constexpr s16 kDoorProfiles[] = {
        fpcNm_DOOR20_e,       fpcNm_DBDOOR_e,       fpcNm_BOSS_DOOR_e,
        fpcNm_L1BOSS_DOOR_e,  fpcNm_L1MBOSS_DOOR_e, fpcNm_L5BOSS_DOOR_e,
        fpcNm_SPIRAL_DOOR_e,  fpcNm_PushDoor_e,     fpcNm_Obj_PushDoor_e,
        fpcNm_Obj_Cdoor_e,    fpcNm_Obj_TDoor_e,    fpcNm_OBJ_NDOOR_e,
        fpcNm_OBJ_UDOOR_e,    fpcNm_Obj_SM_DOOR_e,  fpcNm_OBJ_SEKIDOOR_e,
    };
    const s16 name = fopAcM_GetProfName(actor);
    for (const s16 door : kDoorProfiles) {
        if (name == door) return true;
    }
    return false;
}

bool is_door_event(dEvt_control_c* evt) {
    return is_door_actor(evt->getPt1()) || is_door_actor(evt->getPt2());
}

bool is_midna_actor(fopAc_ac_c* actor) {
    if (actor == nullptr) return false;
    const s16 name = fopAcM_GetProfName(actor);
    return name == fpcNm_MIDNA_e || name == fpcNm_DMIDNA_e;
}

bool is_midna_event(dEvt_control_c* evt) {
    if (evt == nullptr) return false;
    return is_midna_actor(evt->getPt1()) || is_midna_actor(evt->getPt2());
}

bool is_talk_event(dEvt_control_c* evt) {
    return evt != nullptr && evt->mEventStatus == 1 && evt->getMode() == dEvt_mode_TALK_e;
}

bool is_player_process_running() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    return link != nullptr && link->checkEventRun();
}

bool is_genuine_cutscene(dEvt_control_c* evt, bool allowDoors) {
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr &&
        (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0)) {
        return false;
    }

    if (dComIfGp_getPlayer(0) == nullptr) return false;

    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;

    if (allowDoors && human_warp_cinematic_active()) return true;

    if (is_talk_event(evt) || is_midna_event(evt)) return false;

    if (allowDoors && is_player_process_running()) return true;

    if (evt == nullptr || evt->mEventStatus != 1) return false;

    const u8 mode = evt->getMode();
    if (mode != dEvt_mode_DEMO_e && mode != dEvt_mode_COMPULSORY_e) return false;

    static constexpr const char* kNeverBoostEvents[] = {
        "DEFAULT_START",
        "KNOB_START",
    };
    if (!allowDoors && evt->mEventId >= 0) {
        dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
        if (data != nullptr && data->getName() != nullptr) {
            for (const char* name : kNeverBoostEvents) {
                if (std::strcmp(data->getName(), name) == 0) return false;
            }
        }
    }

    if (dMsgObject_isTalkNowCheck() || dMeter2Info_isShopTalkFlag()) return false;

    if (!allowDoors && is_door_event(evt)) return false;

    const int idx = static_cast<int>(evt->mOrderIdx);
    const u16 type = (idx >= 0 && idx < 8)
        ? evt->mOrder[idx].mEventType
        : static_cast<u16>(dEvt_type_OTHER_e);
    return type == dEvt_type_OTHER_e || type == dEvt_type_COMPULSORY_e ||
           type == dEvt_type_POTENTIAL_e ||
           (allowDoors && type == dEvt_type_DOOR_e);
}

}

ModResult init_fast_forward_cutscenes(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    if (hook_svc->resolve) {
        void* addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "aurora::time::set_scale", &addr, nullptr) == MOD_OK && addr) {
            s_setClockScale = reinterpret_cast<SetTimescaleFn>(addr);
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "aurora::time::scale", &addr, nullptr) == MOD_OK && addr) {
            s_getClockScale = reinterpret_cast<GetTimescaleFn>(addr);
        }
        if (s_setClockScale == nullptr || s_getClockScale == nullptr) {
            s_setClockScale = nullptr;
            s_getClockScale = nullptr;
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "dusk::getTransientSettings", &addr, nullptr) == MOD_OK && addr) {
            s_getTransientSettings = reinterpret_cast<GetTransientSettingsFn>(addr);
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "dusk::game_clock::set_sim_rate", &addr, nullptr) == MOD_OK && addr) {
            s_setSimRate = reinterpret_cast<SetSimRateFn>(addr);
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "dusk::game_clock::get_sim_rate", &addr, nullptr) == MOD_OK && addr) {
            s_getSimRate = reinterpret_cast<GetSimRateFn>(addr);
        }
        if (s_setSimRate == nullptr || s_getSimRate == nullptr) {
            s_setSimRate = nullptr;
            s_getSimRate = nullptr;
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "aurora_set_timescale", &addr, nullptr) == MOD_OK && addr) {
            s_setTimescale = reinterpret_cast<SetTimescaleFn>(addr);
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "aurora_get_timescale", &addr, nullptr) == MOD_OK && addr) {
            s_getTimescale = reinterpret_cast<GetTimescaleFn>(addr);
        }
    }
    mods::hook::add_post<AuroraGetTimescaleHook>(hook_svc, on_aurora_get_timescale_post);
    mods::hook::add_pre<AuroraSetTimescaleHook>(hook_svc, on_aurora_set_timescale_pre);
    return MOD_OK;
}

void update_hidden_run_watchdog();

void update_fast_forward_cutscenes(const LogService*, ModContext*) {
    if (!clock_available()) return;

    update_hidden_run_watchdog();
    update_turbo_release_watch();

    update_boss_rush_fight_start_hold();

    if (s_hiddenRun) {
        s_active = false;
        s_confirmFrames = 0;
        return;
    }

    const bool defeatHold = is_boss_rush_defeat_hold();
    const bool startHold = is_boss_rush_fight_start_hold();
    if (defeatHold || startHold) {
        stop_fast_forward();
        s_confirmFrames = 0;
        return;
    }

    const bool veryFast = g_configGeneralFastForwardCutscenesMode == FF_CUTSCENES_VERY_FAST;
    dEvt_control_c* evt = dComIfGp_getEvent();
    const bool genuine = is_genuine_cutscene(evt, veryFast);

    if (!genuine) {
        stop_fast_forward();
        s_confirmFrames = 0;
        return;
    }

    if (s_confirmFrames < kLeadFrames) {
        ++s_confirmFrames;
    }

    const bool skipWillHandle = evt->mSkipFunc != nullptr &&
                                (g_configGeneralSkipCutscenes || is_boss_rush_active());

    const bool shouldFastForward = g_configGeneralFastForwardCutscenesMode != FF_CUTSCENES_OFF &&
                                   !skipWillHandle &&
                                   s_confirmFrames >= kLeadFrames;

    const float targetScale = veryFast ? kVeryFastForwardScale : kFastForwardScale;

    if (shouldFastForward) {
        const float current = live_timescale();
        if (!s_active) {
            s_desiredScale = sane_host_scale(current);
            s_active = true;
            own_set_timescale(targetScale);
        } else if (current != targetScale) {
            s_desiredScale = sane_host_scale(current);
            own_set_timescale(targetScale);
        }
    } else if (s_active) {
        stop_fast_forward();
    }
}

void fast_forward_set_hidden_run(bool on);

void shutdown_fast_forward_cutscenes() {
    fast_forward_set_hidden_run(false);
    stop_fast_forward();
    s_confirmFrames = 0;
    s_holdActive = false;
    s_holdSettleFrames = 0;
    s_wasFightLive = false;
    s_fightStartFrames = -1;
    s_holdMinFrames = 0;
    s_turboReleaseFrames = 0;
}

void update_hidden_run_watchdog() {
    if (s_hiddenRun || s_active) return;
    const float current = live_timescale();
    if (current >= kHiddenRunScale) {
        own_set_timescale(sane_host_scale(s_hostScale));
    } else if (current > 0.0f) {
        s_hostScale = current;
    }
}

void fast_forward_set_hidden_run(bool on) {
    if (!clock_available()) return;
    if (on == s_hiddenRun) {
        if (on && live_timescale() != kHiddenRunScale) force_set_timescale(kHiddenRunScale);
        return;
    }
    if (on) {
        s_hostScale = s_active ? sane_host_scale(s_desiredScale) : sane_host_scale(live_timescale());
        s_active = false;
        s_confirmFrames = 0;
        s_hiddenRun = true;
        force_set_timescale(kHiddenRunScale);
    } else {
        s_hiddenRun = false;
        bool known = false;
        const bool turbo = turbo_held(known);
        if (known && !turbo && s_hostScale == kTurboScale) s_hostScale = 1.0f;
        own_set_timescale(sane_host_scale(s_hostScale));
    }
}

float general_get_aurora_timescale() {
    if (s_active) return s_desiredScale;
    if (!clock_available()) return 1.0f;
    return live_timescale();
}
