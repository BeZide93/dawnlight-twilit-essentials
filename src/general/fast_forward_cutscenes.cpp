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
#include "f_op/f_op_overlap_mng.h"
#include "f_pc/f_pc_name.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_midna.h"
#include "dusk/config_var.hpp"
#include "human_warp.hpp"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>
#include <cstring>
#include <string_view>

int g_configGeneralFastForwardCutscenesMode = FF_CUTSCENES_OFF;

float g_configGeneralFastForwardSpeed = 8.0f;

bool g_configGeneralFastDoorAnimations = false;

#define ENABLE_FF_LOG 0

float clamp_fast_forward_speed(float speed) {
    if (speed < 2.0f) speed = 2.0f;
    if (speed > 15.0f) speed = 15.0f;
    return speed;
}

bool is_boss_rush_active();
bool boss_rush_is_fighting_here();
bool boss_rush_is_returning_to_chamber();
bool boss_rush_settle_window_active();
bool boss_rush_is_fight_retry_warp();
bool is_in_boss_rush_chamber();
bool boss_bar_boss_defeated_now();
bool boss_bar_current_fight_state(const char** outLabel, bool& outEngaged);
bool boss_rush_game_mode_is_active();
bool boss_rush_game_mode_entering();
bool boss_rush_stallord_fade_active();

namespace {

using SetTimescaleFn = void (*)(float);
using GetTimescaleFn = float (*)();
using SetSimRateFn = void (*)(float);
using GetSimRateFn = float (*)();
using GetTransientSettingsFn = dusk::TransientSettings& (*)();
using GetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
using GameClockResetFn = void (*)();

constexpr float kBaseSimHz = 30.0f;

constexpr float kTurboScale = 4.0f;
constexpr float kHiddenRunScale = 16.0f;
constexpr int kLeadFrames = 5;
constexpr int kFightStartHoldFrames = 60;
constexpr int kDefeatMinHoldFrames = 120;
constexpr int kTurboReleaseWatchFrames = 6;
constexpr int kGameModeEntryHoldFrames = 90;

SetTimescaleFn s_setTimescale = nullptr;
GetTimescaleFn s_getTimescale = nullptr;
SetTimescaleFn s_setClockScale = nullptr;
GetTimescaleFn s_getClockScale = nullptr;
SetSimRateFn s_setSimRate = nullptr;
GetSimRateFn s_getSimRate = nullptr;
GetTransientSettingsFn s_getTransientSettings = nullptr;
GameClockResetFn s_gameClockReset = nullptr;
dusk::config::ConfigVar<bool>* s_instantTextVar = nullptr;
bool s_instantTextOverridden = false;

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
int s_gameModeHoldFrames = 0;

bool update_game_mode_entry_hold() {
    if (boss_rush_game_mode_is_active() && boss_rush_game_mode_entering()) {
        s_gameModeHoldFrames = kGameModeEntryHoldFrames;
    } else if (s_gameModeHoldFrames > 0) {
        --s_gameModeHoldFrames;
    }
    return s_gameModeHoldFrames > 0;
}

DEFINE_HOOK_SYMBOL("aurora_get_timescale", float(), AuroraGetTimescaleHook);
DEFINE_HOOK_SYMBOL("aurora_set_timescale", void(float), AuroraSetTimescaleHook);
DEFINE_HOOK(&mDoCPd_c::read, FfDialoguePadReadHook);

bool s_hiddenRun = false;
float s_hostScale = 1.0f;

bool s_slowActive = false;
float s_slowScale = 1.0f;
float s_slowRestoreScale = 1.0f;

float sane_host_scale(float scale) {
    return scale > 0.0f && scale < kHiddenRunScale ? scale : 1.0f;
}

float unslowed(float scale) {
    return s_slowActive ? s_slowRestoreScale : scale;
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
    if (s_slowActive) {
        s_slowRestoreScale = sane_host_scale(requested);
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

void drop_sim_backlog() {
    if (s_gameClockReset != nullptr) {
        s_gameClockReset();
    } else if (s_setSimRate != nullptr && s_getSimRate != nullptr) {
        s_setSimRate(s_getSimRate());
    }
}

void stop_fast_forward() {
    if (!s_active) return;
    s_active = false;
    if (s_slowActive) {
        s_slowRestoreScale = resolved_desired_scale();
        if (!s_hiddenRun) force_set_timescale(s_slowScale);
        drop_sim_backlog();
        return;
    }
    own_set_timescale(resolved_desired_scale());
    drop_sim_backlog();
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

bool is_item_get_proc() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr) return false;
    switch (link->mProcID) {
    case daAlink_c::PROC_GET_ITEM:
    case daAlink_c::PROC_OPEN_TREASURE:
    case daAlink_c::PROC_BOTTLE_GET:
    case daAlink_c::PROC_GRASS_WHISTLE_GET:
    case daAlink_c::PROC_LOOK_UP_TO_GET_ITEM:
    case daAlink_c::PROC_HORSE_GET_KEY:
    case daAlink_c::PROC_CANOE_FISHING_GET:
        return true;
    default:
        return false;
    }
}

bool is_gameplay_event_proc() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    return link != nullptr && link->mProcID == daAlink_c::PROC_MONKEY_MOVE;
}

bool is_item_get_event(dEvt_control_c* evt) {
    if (evt == nullptr || evt->mEventStatus != 1) return false;
    const int idx = static_cast<int>(evt->mOrderIdx);
    if (idx >= 0 && idx < 8 && evt->mOrder[idx].mEventType == dEvt_type_TREASURE_e) return true;
    if (evt->mEventId < 0) return false;
    dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
    return data != nullptr && data->getName() != nullptr &&
           std::strcmp(data->getName(), "DEFAULT_GETITEM") == 0;
}

bool is_warp_place_title() {
    dMsgObject_c* msg = dMsgObject_getMsgObjectClass();
    if (msg == nullptr || msg->getFukiKind() != 12) return false;
    if (human_warp_cinematic_active()) return true;
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    return link != nullptr && link->mProcID == daAlink_c::PROC_WARP;
}

bool is_talk_message_active() {
    return dMsgObject_isTalkNowCheck() && !is_warp_place_title();
}

bool is_dialogue_active(dEvt_control_c* evt) {
    if (is_talk_message_active() || dMeter2Info_isShopTalkFlag()) return true;
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link != nullptr && link->mProcID == daAlink_c::PROC_TALK) return true;
    if (evt == nullptr || evt->mEventStatus != 1 || evt->mEventId < 0) return false;
    dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
    return data != nullptr && data->getName() != nullptr && std::strstr(data->getName(), "TALK") != nullptr;
}

bool is_chest_open_event(dEvt_control_c* evt) {
    if (evt == nullptr || evt->mEventStatus != 1) return false;
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr || link->mProcID == daAlink_c::PROC_GET_ITEM) return false;
    const int idx = static_cast<int>(evt->mOrderIdx);
    if (idx >= 0 && idx < 8 && evt->mOrder[idx].mEventType == dEvt_type_TREASURE_e) return true;
    if (evt->mEventId < 0) return false;
    dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
    return data != nullptr && data->getName() != nullptr &&
           std::strncmp(data->getName(), "DEFAULT_TREASURE", 16) == 0;
}

bool is_gameplay_scene() {
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr &&
        (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0)) {
        return false;
    }

    if (dComIfGp_getPlayer(0) == nullptr) return false;

    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;

    return dMeter2Info_getWindowStatus() == 0;
}

bool is_door_start_event(const char* name) {
    static constexpr const char* kDoorStartEvents[] = {
        "SHUTTER_START",
        "SHUTTER_START_STOP",
        "BS_SHUTTER_START",
        "BS_SHUTTER_START_B",
        "KNOB_START",
        "KNOB_START_B",
    };
    for (const char* doorEvent : kDoorStartEvents) {
        if (std::strcmp(name, doorEvent) == 0) return true;
    }
    return false;
}

bool is_door_animation(dEvt_control_c* evt) {
    if (evt == nullptr || evt->mEventStatus != 1) return false;

    if (!is_gameplay_scene() || is_dialogue_active(evt)) return false;

    if (dComIfGp_isEnableNextStage() || fopOvlpM_IsPeek()) return false;

    const int idx = static_cast<int>(evt->mOrderIdx);
    if (idx >= 0 && idx < 8 && evt->mOrder[idx].mEventType == dEvt_type_DOOR_e) return true;

    if (is_door_event(evt)) return true;

    if (evt->mEventId < 0) return false;
    dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
    return data != nullptr && data->getName() != nullptr && is_door_start_event(data->getName());
}

bool is_dialogue_fast_forward_wanted() {
    if (g_configGeneralFastForwardCutscenesMode != FF_CUTSCENES_VERY_FAST) return false;

    if (!is_gameplay_scene() || !is_talk_message_active()) return false;

    return is_player_process_running();
}

void set_instant_text_override(bool on) {
    if (s_instantTextVar == nullptr) return;
    if (on) {
        if (!s_instantTextOverridden && !s_instantTextVar->getValue()) {
            s_instantTextVar->setOverrideValue(true);
            s_instantTextOverridden = true;
        }
    } else if (s_instantTextOverridden) {
        s_instantTextVar->clearOverride();
        s_instantTextOverridden = false;
    }
}

void on_ff_dialogue_pad_read_post(ModContext*, void*, void*, void*) {
    const bool wanted = is_dialogue_fast_forward_wanted();
    set_instant_text_override(wanted);
    if (!wanted || s_instantTextVar == nullptr || !s_instantTextVar->getValue()) return;
    mDoCPd_c::getCpadInfo(PAD_1).mButtonFlags |= PAD_BUTTON_B;
}

bool is_genuine_cutscene(dEvt_control_c* evt, bool allowDoors) {
    if (!is_gameplay_scene()) return false;

    if (is_dialogue_active(evt)) return false;

    if (allowDoors && is_chest_open_event(evt)) return true;

    if (is_item_get_proc() || is_item_get_event(evt)) return false;

    if (is_gameplay_event_proc()) return false;

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

    if (is_talk_message_active() || dMeter2Info_isShopTalkFlag()) return false;

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
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "dusk::game_clock::reset", &addr, nullptr) == MOD_OK && addr) {
            s_gameClockReset = reinterpret_cast<GameClockResetFn>(addr);
        }
        addr = nullptr;
        if (hook_svc->resolve(mod_ctx, "dusk::config::GetConfigVar", &addr, nullptr) == MOD_OK && addr) {
            s_instantTextVar = static_cast<dusk::config::ConfigVar<bool>*>(
                reinterpret_cast<GetConfigVarFn>(addr)("game.instantText"));
        }
    }
    mods::hook::add_post<AuroraGetTimescaleHook>(hook_svc, on_aurora_get_timescale_post);
    mods::hook::add_pre<AuroraSetTimescaleHook>(hook_svc, on_aurora_set_timescale_pre);
    mods::hook::add_post<FfDialoguePadReadHook>(hook_svc, on_ff_dialogue_pad_read_post);
    return MOD_OK;
}

void update_hidden_run_watchdog();

namespace {

struct FfLogState {
    s16 eventId = -2;
    u8 status = 0xFF;
    u8 mode = 0xFF;
    u16 type = 0xFFFF;
    int proc = -1;
    s16 pt1 = -2;
    s16 pt2 = -2;
    bool genuine = false;
    bool boosted = false;
};

FfLogState s_ffLog;

void log_fast_forward_event(const LogService* log_svc, ModContext* ctx, dEvt_control_c* evt,
                            bool genuine, bool boosted) {
    if (!ENABLE_FF_LOG || log_svc == nullptr || ctx == nullptr) return;

    FfLogState now;
    now.genuine = genuine;
    now.boosted = boosted;
    if (evt != nullptr) {
        now.eventId = evt->mEventId;
        now.status = evt->mEventStatus;
        now.mode = evt->getMode();
        const int idx = static_cast<int>(evt->mOrderIdx);
        now.type = (idx >= 0 && idx < 8) ? evt->mOrder[idx].mEventType : 0xFFFF;
        now.pt1 = evt->getPt1() != nullptr ? fopAcM_GetProfName(evt->getPt1()) : -1;
        now.pt2 = evt->getPt2() != nullptr ? fopAcM_GetProfName(evt->getPt2()) : -1;
    }
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    now.proc = link != nullptr ? static_cast<int>(link->mProcID) : -1;

    const bool linkEvent = link != nullptr && link->checkEventRun();
    const bool changed = now.eventId != s_ffLog.eventId || now.status != s_ffLog.status ||
                         now.mode != s_ffLog.mode || now.type != s_ffLog.type ||
                         now.pt1 != s_ffLog.pt1 || now.pt2 != s_ffLog.pt2 ||
                         now.genuine != s_ffLog.genuine || now.boosted != s_ffLog.boosted ||
                         (now.proc != s_ffLog.proc && (linkEvent || now.status == 1));
    if (!changed) return;
    s_ffLog = now;
    if (now.status != 1 && !linkEvent && !boosted) return;

    const char* name = "-";
    if (evt != nullptr && evt->mEventId >= 0) {
        dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
        if (data != nullptr && data->getName() != nullptr) name = data->getName();
    }
    const char* stage = dComIfGp_getStartStageName();
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "[ff] %s | event '%s' id=%d status=%d mode=%d type=%d | pt1=%d pt2=%d | "
                  "link proc=%d eventRun=%d | stage=%s",
                  boosted ? "FAST" : (genuine ? "cutscene, normal speed" : "normal"), name,
                  static_cast<int>(now.eventId), static_cast<int>(now.status),
                  static_cast<int>(now.mode), static_cast<int>(now.type),
                  static_cast<int>(now.pt1), static_cast<int>(now.pt2), now.proc,
                  linkEvent ? 1 : 0, stage != nullptr ? stage : "-");
    log_svc->info(ctx, msg);
}

}

void update_fast_forward_cutscenes(const LogService* log_svc, ModContext* ff_ctx) {
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
    const bool gameModeHold = update_game_mode_entry_hold();
    if (defeatHold || startHold || gameModeHold || boss_rush_stallord_fade_active()) {
        stop_fast_forward();
        s_confirmFrames = 0;
        return;
    }

    const bool veryFast = g_configGeneralFastForwardCutscenesMode == FF_CUTSCENES_VERY_FAST;
    dEvt_control_c* evt = dComIfGp_getEvent();
    const bool fastDoor = g_configGeneralFastDoorAnimations && !veryFast && is_door_animation(evt);
    const bool genuine = fastDoor || is_genuine_cutscene(evt, veryFast);

    if (!genuine) {
        stop_fast_forward();
        s_confirmFrames = 0;
        log_fast_forward_event(log_svc, ff_ctx, evt, false, false);
        return;
    }

    if (s_confirmFrames < kLeadFrames) {
        ++s_confirmFrames;
    }

    const bool skipWillHandle = evt->mSkipFunc != nullptr && skip_cutscenes_enabled();

    const bool shouldFastForward = (fastDoor || g_configGeneralFastForwardCutscenesMode != FF_CUTSCENES_OFF) &&
                                   !skipWillHandle &&
                                   s_confirmFrames >= kLeadFrames;

    const float targetScale = g_configGeneralFastForwardSpeed;
    log_fast_forward_event(log_svc, ff_ctx, evt, true, shouldFastForward);

    if (shouldFastForward) {
        const float current = live_timescale();
        if (!s_active) {
            s_desiredScale = sane_host_scale(unslowed(current));
            s_active = true;
            own_set_timescale(targetScale);
        } else if (current != targetScale) {
            s_desiredScale = sane_host_scale(unslowed(current));
            own_set_timescale(targetScale);
        }
    } else if (s_active) {
        stop_fast_forward();
    }
}

void fast_forward_set_hidden_run(bool on);

void shutdown_fast_forward_cutscenes() {
    set_instant_text_override(false);
    s_instantTextVar = nullptr;
    fast_forward_set_hidden_run(false);
    stop_fast_forward();
    s_confirmFrames = 0;
    s_holdActive = false;
    s_holdSettleFrames = 0;
    s_wasFightLive = false;
    s_fightStartFrames = -1;
    s_holdMinFrames = 0;
    s_turboReleaseFrames = 0;
    s_gameModeHoldFrames = 0;
}

void update_hidden_run_watchdog() {
    if (s_hiddenRun || s_active) return;
    const float current = live_timescale();
    if (current >= kHiddenRunScale) {
        own_set_timescale(sane_host_scale(s_hostScale));
    } else if (current > 0.0f) {
        s_hostScale = unslowed(current);
    }
}

void fast_forward_set_hidden_run(bool on) {
    if (!clock_available()) return;
    if (on == s_hiddenRun) {
        if (on && live_timescale() != kHiddenRunScale) force_set_timescale(kHiddenRunScale);
        return;
    }
    if (on) {
        s_hostScale = s_active ? sane_host_scale(s_desiredScale)
                               : sane_host_scale(unslowed(live_timescale()));
        s_active = false;
        s_confirmFrames = 0;
        s_hiddenRun = true;
        force_set_timescale(kHiddenRunScale);
    } else {
        s_hiddenRun = false;
        bool known = false;
        const bool turbo = turbo_held(known);
        if (known && !turbo && s_hostScale == kTurboScale) s_hostScale = 1.0f;
        if (s_slowActive) {
            s_slowRestoreScale = sane_host_scale(s_hostScale);
            force_set_timescale(s_slowScale);
            return;
        }
        own_set_timescale(sane_host_scale(s_hostScale));
    }
}

bool general_timescale_available() {
    return clock_available();
}

void general_set_slow_motion(float scale) {
    if (!clock_available()) return;
    if (scale > 0.0f && scale < 1.0f) {
        if (!s_slowActive) {
            s_slowRestoreScale = s_active ? sane_host_scale(s_desiredScale)
                                          : sane_host_scale(live_timescale());
            s_slowActive = true;
        }
        s_slowScale = scale;
        if (!s_active && !s_hiddenRun) force_set_timescale(scale);
        return;
    }
    if (!s_slowActive) return;
    s_slowActive = false;
    if (s_active || s_hiddenRun) return;
    bool known = false;
    const bool turbo = turbo_held(known);
    if (known && !turbo && s_slowRestoreScale == kTurboScale) s_slowRestoreScale = 1.0f;
    own_set_timescale(sane_host_scale(s_slowRestoreScale));
}

float general_get_aurora_timescale() {
    if (s_active) return s_desiredScale;
    if (!clock_available()) return 1.0f;
    return live_timescale();
}
