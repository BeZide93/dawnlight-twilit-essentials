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
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

bool g_configGeneralFastForwardCutscenes = false;

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

constexpr float kFastForwardScale = 4.0f;
constexpr int kLeadFrames = 5;
constexpr int kFightStartHoldFrames = 60;
constexpr int kDefeatMinHoldFrames = 120;
constexpr int kStuckWatchFrames = 30;

SetTimescaleFn s_setTimescale = nullptr;
GetTimescaleFn s_getTimescale = nullptr;

bool s_active = false;
float s_restoreScale = 1.0f;
int s_confirmFrames = 0;
bool s_holdActive = false;
int s_holdSettleFrames = 0;
int s_holdMinFrames = 0;
bool s_wasFightLive = false;
int s_fightStartFrames = -1;
int s_stuckFrames = 0;
int s_postBoostFrames = -1;
bool s_extHoldsFastScale = false;

DEFINE_HOOK_SYMBOL("aurora_get_timescale", float(), AuroraGetTimescaleHook);
DEFINE_HOOK_SYMBOL("aurora_set_timescale", void(float), AuroraSetTimescaleHook);

void on_aurora_get_timescale_post(ModContext*, void*, void* retval, void*) {
    if (!s_active || retval == nullptr) return;
    float shown = s_restoreScale;
    if (!(shown > 0.0f) || shown == kFastForwardScale) shown = 1.0f;
    *static_cast<float*>(retval) = shown;
}

HookAction on_aurora_set_timescale_pre(ModContext*, void* args, void*, void*) {
    s_extHoldsFastScale = mods::arg<float>(args, 0) == kFastForwardScale;
    return HOOK_CONTINUE;
}

float live_timescale() {
    if (AuroraGetTimescaleHook::g_orig) return AuroraGetTimescaleHook::g_orig();
    return s_getTimescale ? s_getTimescale() : 1.0f;
}

void own_set_timescale(float scale) {
    if (AuroraSetTimescaleHook::g_orig) AuroraSetTimescaleHook::g_orig(scale);
    else if (s_setTimescale) s_setTimescale(scale);
}

void stop_fast_forward() {
    if (!s_active) return;
    if (!s_extHoldsFastScale) own_set_timescale(s_restoreScale);
    s_active = false;
}

/* Diag: why the boss-rush hold path is currently swallowing the boost (0 = not holding). */
int s_holdReasonDiag = 0;
const char* hold_reason_name(int reason) {
    switch (reason) {
    case 1: return "fighting+defeated";
    case 2: return "fighting+holdActive";
    case 3: return "returning-to-chamber";
    case 4: return "chamber-settle";
    case 5: return "fight-start-hold";
    default: return "none";
    }
}

bool is_boss_rush_defeat_hold() {
    if (!is_boss_rush_active()) {
        s_holdSettleFrames = 0;
        s_holdActive = false;
        s_holdMinFrames = 0;
        s_holdReasonDiag = 0;
        return false;
    }

    if (s_holdMinFrames > 0) --s_holdMinFrames;

    if (boss_rush_is_fighting_here()) {
        s_holdSettleFrames = 0;
        if (boss_bar_boss_defeated_now()) {
            s_holdActive = true;
            s_holdMinFrames = kDefeatMinHoldFrames;
            s_holdReasonDiag = 1;
            return true;
        }

        const char* label = nullptr;
        bool engaged = false;
        if (s_holdMinFrames <= 0 && boss_bar_current_fight_state(&label, engaged) && engaged) {
            s_holdActive = false;
        }
        if (s_holdActive) s_holdReasonDiag = 2;
        return s_holdActive;
    }

    if (boss_rush_is_returning_to_chamber()) {
        s_holdSettleFrames = 0;
        s_holdReasonDiag = 3;
        return true;
    }

    if (is_in_boss_rush_chamber() && !boss_rush_settle_window_active()) {
        s_holdReasonDiag = s_holdActive ? 4 : 0;
        if (++s_holdSettleFrames > 30 && s_holdMinFrames <= 0) {
            s_holdSettleFrames = 0;
            s_holdActive = false;
            s_holdReasonDiag = 0;
        }
        return s_holdActive;
    }

    s_holdSettleFrames = 0;
    if (!s_holdActive) s_holdReasonDiag = 0;
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

bool is_genuine_cutscene(dEvt_control_c* evt) {
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr &&
        (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0)) {
        return false;
    }

    if (dComIfGp_getPlayer(0) == nullptr) return false;

    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;

    if (evt == nullptr || evt->mEventStatus != 1) return false;

    const u8 mode = evt->getMode();
    if (mode != dEvt_mode_DEMO_e && mode != dEvt_mode_COMPULSORY_e) return false;

    if (evt->mEventId < 0) return false;

    static constexpr const char* kNeverBoostEvents[] = {
        "DEFAULT_START",
        "KNOB_START",
    };
    dEvDtEvent_c* data = g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
    if (data != nullptr && data->getName() != nullptr) {
        for (const char* name : kNeverBoostEvents) {
            if (std::strcmp(data->getName(), name) == 0) return false;
        }
    }

    if (dMsgObject_isTalkNowCheck() || dMeter2Info_isShopTalkFlag()) return false;

    if (is_door_event(evt)) return false;

    const int idx = static_cast<int>(evt->mOrderIdx);
    const u16 type = (idx >= 0 && idx < 8)
        ? evt->mOrder[idx].mEventType
        : static_cast<u16>(dEvt_type_OTHER_e);
    return type == dEvt_type_OTHER_e || type == dEvt_type_COMPULSORY_e ||
           type == dEvt_type_POTENTIAL_e;
}

}

ModResult init_fast_forward_cutscenes(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    if (hook_svc->resolve) {
        void* addr = nullptr;
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

namespace {

/* Diagnostics: log gate/boost state transitions so cutscene behavior can be followed in the
 * game log. Only fires when the event manager state actually changes, never per frame. */
u32 s_diagKey = 0;

void diag_log(const LogService* log_svc, const char* fmt, ...) {
    if (log_svc == nullptr || log_svc->debug == nullptr) return;
    char msg[320];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log_svc->debug(mod_ctx, msg);
}

void diag_gate(const LogService* log_svc, dEvt_control_c* evt, bool genuine) {
    u32 key = genuine ? 0x80000000u : 0u;
    char why[224] = "";
    if (evt != nullptr) {
        key |= 1u * evt->mEventStatus;
        key |= u32(evt->getMode()) << 4;
        key |= (u32(u16(evt->mEventId)) & 0xFFF) << 8;
        key |= (evt->mSkipFunc != nullptr ? 1u : 0u) << 21;

        const char* stageName = dComIfGp_getStartStageName();
        if (stageName != nullptr &&
            (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0)) {
            key |= 1u << 22;
            std::snprintf(why, sizeof(why), "stage=%s excluded", stageName);
        } else if (dComIfGp_getPlayer(0) == nullptr) {
            key |= 2u << 22;
            std::snprintf(why, sizeof(why), "no player");
        } else if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) {
            key |= 3u << 22;
            std::snprintf(why, sizeof(why), "paused");
        } else if (evt->mEventStatus != 1) {
            key |= 4u << 22;
            std::snprintf(why, sizeof(why), "status=%d", (int)evt->mEventStatus);
        } else {
            const u8 mode = evt->getMode();
            if (mode != dEvt_mode_DEMO_e && mode != dEvt_mode_COMPULSORY_e) {
                key |= 5u << 22;
                std::snprintf(why, sizeof(why), "mode=%d", (int)mode);
            } else if (evt->mEventId < 0) {
                key |= 6u << 22;
                std::snprintf(why, sizeof(why), "eventId<0");
            } else if (dMsgObject_isTalkNowCheck() || dMeter2Info_isShopTalkFlag()) {
                key |= 7u << 22;
                std::snprintf(why, sizeof(why), "talk/shop");
            } else if (is_door_event(const_cast<dEvt_control_c*>(evt))) {
                key |= 8u << 22;
                std::snprintf(why, sizeof(why), "door event");
            } else {
                const int idx = static_cast<int>(evt->mOrderIdx);
                const u16 type = (idx >= 0 && idx < 8)
                    ? evt->mOrder[idx].mEventType
                    : static_cast<u16>(dEvt_type_OTHER_e);
                key |= (u32(type & 0xF) << 22) | (9u << 26);
                std::snprintf(why, sizeof(why), "orderType=%d", (int)type);
            }
            dEvDtEvent_c* data =
                g_dComIfG_gameInfo.play.getEvtManager().getEventData(evt->mEventId);
            if (data != nullptr && data->getName() != nullptr) {
                diag_log(log_svc, "ff-cutscene: evt=%d name='%s' status=%d mode=%d "
                                    "skipFunc=%d genuine=%d %s",
                         (int)evt->mEventId, data->getName(), (int)evt->mEventStatus,
                         (int)evt->getMode(), evt->mSkipFunc != nullptr ? 1 : 0,
                         genuine ? 1 : 0, why);
            }
        }
    }
    if (key != s_diagKey) {
        s_diagKey = key;
        diag_log(log_svc, "ff-cutscene: gate %s (%s)", genuine ? "OPEN" : "closed", why);
    }
}

}  // namespace

void update_fast_forward_cutscenes(const LogService* log_svc, ModContext* mod_ctx) {
    if (!s_setTimescale) return;

    update_boss_rush_fight_start_hold();

    const bool defeatHold = is_boss_rush_defeat_hold();
    const bool startHold = is_boss_rush_fight_start_hold();
    {
        const int reason = defeatHold ? s_holdReasonDiag : (startHold ? 5 : 0);
        static int s_lastHoldLogged = 0;
        if (reason != s_lastHoldLogged) {
            diag_log(log_svc, "ff-cutscene: hold %s -> %s",
                     hold_reason_name(s_lastHoldLogged), hold_reason_name(reason));
            s_lastHoldLogged = reason;
        }
    }
    if (defeatHold || startHold) {
        if (s_active) diag_log(log_svc, "ff-cutscene: boost stopped (boss rush hold)");
        stop_fast_forward();
        s_confirmFrames = 0;
        return;
    }

    dEvt_control_c* evt = dComIfGp_getEvent();
    const bool genuine = is_genuine_cutscene(evt);
    diag_gate(log_svc, evt, genuine);

    if (!genuine) {
        if (s_active) diag_log(log_svc, "ff-cutscene: boost stopped (gate closed)");
        stop_fast_forward();
        s_confirmFrames = 0;
        return;
    }

    if (s_confirmFrames < kLeadFrames) {
        ++s_confirmFrames;
    }

    const bool skipWillHandle = evt->mSkipFunc != nullptr &&
                                (g_configGeneralSkipCutscenes || is_boss_rush_active());

    const bool shouldFastForward = g_configGeneralFastForwardCutscenes &&
                                   !skipWillHandle &&
                                   s_confirmFrames >= kLeadFrames;

    if (shouldFastForward) {
        const float current = live_timescale();
        if (!s_active) {
            s_restoreScale = (current > 0.0f && current != kFastForwardScale)
                                 ? current
                                 : 1.0f;
            own_set_timescale(kFastForwardScale);
            s_active = true;
            diag_log(log_svc, "ff-cutscene: boost ENGAGED, scale 1 -> %g (skipWillHandle=%d)",
                     kFastForwardScale, skipWillHandle ? 1 : 0);
        } else if (current != kFastForwardScale) {
            s_restoreScale = (current > 0.0f && current != kFastForwardScale)
                                 ? current
                                 : 1.0f;
            own_set_timescale(kFastForwardScale);
            diag_log(log_svc, "ff-cutscene: boost scale stomped to %g, reasserting",
                     current);
        }
    } else if (s_active) {
        stop_fast_forward();
    }

    if (s_active) {
        s_postBoostFrames = 0;
        s_stuckFrames = 0;
    } else if (s_postBoostFrames >= 0) {
        if (s_postBoostFrames < 600) {
            ++s_postBoostFrames;
        } else {
            s_postBoostFrames = -1;
        }
        if (!s_extHoldsFastScale && live_timescale() == kFastForwardScale) {
            if (++s_stuckFrames > kStuckWatchFrames) {
                own_set_timescale(s_restoreScale > 0.0f &&
                                          s_restoreScale != kFastForwardScale
                                      ? s_restoreScale
                                      : 1.0f);
                s_stuckFrames = 0;
            }
        } else {
            s_stuckFrames = 0;
        }
    }
}

void shutdown_fast_forward_cutscenes() {
    stop_fast_forward();
    s_confirmFrames = 0;
    s_holdActive = false;
    s_holdSettleFrames = 0;
    s_wasFightLive = false;
    s_fightStartFrames = -1;
    s_stuckFrames = 0;
    s_postBoostFrames = -1;
    s_extHoldsFastScale = false;
    s_holdMinFrames = 0;
}

float general_get_aurora_timescale() {
    if (s_active) return s_restoreScale;
    return s_getTimescale ? s_getTimescale() : 1.0f;
}
