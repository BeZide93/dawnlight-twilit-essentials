#include "boss_rush_timer.hpp"
#include "boss_rush.hpp"
#include "boss_rush_common.hpp"
#include "../boss_bar/boss_bar.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "Z2AudioLib/Z2AudioMgr.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool g_configBossRushTimer = false;
bool g_configBossRushShowBestTimer = true;

float general_get_aurora_timescale();

namespace {

enum RunState { IDLE, RUNNING, FINISHED };

const ConfigService* s_cfg = nullptr;
ModContext*          s_ctx = nullptr;
ConfigVarHandle      s_var = 0;

RunState s_state = IDLE;
int  s_idx      = -1;
u32  s_finalCs  = 0;
bool s_isRecord = false;

u32  s_best[kMaxBossGalleryEntries] = {};

unsigned long long s_elapsedMs = 0;
long long s_provisionalMs = 0;
std::chrono::steady_clock::time_point s_lastTick;
bool s_haveLastTick = false;

// Chain-run mode (full boss rush): s_elapsedMs keeps accumulating across the
// whole run and only ticks while a fight is actually running. Per-fight times
// for the best-time records are measured relative to s_chainCheckpointMs, the
// value the long timer had when the current fight engaged.
bool s_chainRun = false;
unsigned long long s_chainCheckpointMs = 0;

const ConfigService* s_chainCfg = nullptr;
ModContext*          s_chainCtx = nullptr;
ConfigVarHandle      s_chainVar = 0;
u32                  s_chainBestCs = 0;

inline u32 cs_now() { return static_cast<u32>(s_elapsedMs / 10ull); }

void load_from_string(const char* s) {
    for (u32& b : s_best) b = 0;
    if (s == nullptr || s[0] == '\0') return;
    const char* p = s;
    while (*p) {
        char* end = nullptr;
        long idx = std::strtol(p, &end, 10);
        if (end == p || *end != ':') break;
        p = end + 1;
        long cs = std::strtol(p, &end, 10);
        if (end == p) break;
        p = end;
        if (idx >= 0 && idx < static_cast<long>(kMaxBossGalleryEntries) && cs > 0) {
            s_best[idx] = static_cast<u32>(cs);
        }
        if (*p == ',') ++p;
    }
}

void save_to_string() {
    if (s_cfg == nullptr || s_var == 0) return;
    char buf[512];
    buf[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < kMaxBossGalleryEntries; ++i) {
        if (s_best[i] == 0) continue;
        char frag[24];
        int n = std::snprintf(frag, sizeof(frag), "%s%zu:%u",
                              used ? "," : "", i, s_best[i]);
        if (n <= 0 || used + static_cast<size_t>(n) >= sizeof(buf)) break;
        std::strcat(buf, frag);
        used += static_cast<size_t>(n);
    }
    s_cfg->set_string(s_ctx, s_var, buf);
}

constexpr long long kCutsceneMs = 400;

std::chrono::steady_clock::time_point s_eventStart;
bool s_eventActive = false;

void finalize() {
    if (s_state != RUNNING) return;
    s_state = FINISHED;
    const unsigned long long fightMs = s_chainRun ? (s_elapsedMs - s_chainCheckpointMs)
                                                  : s_elapsedMs;
    s_finalCs = static_cast<u32>(fightMs / 10ull);

    const u32 prev = (s_idx >= 0 && s_idx < static_cast<int>(kMaxBossGalleryEntries))
                         ? s_best[s_idx] : 0;
    s_isRecord = (prev == 0) || (s_finalCs < prev);
    if (s_isRecord && s_idx >= 0 && s_idx < static_cast<int>(kMaxBossGalleryEntries)) {
        s_best[s_idx] = s_finalCs;
        save_to_string();
        if (!s_chainRun) {
            Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_DROP_COMPLETE, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }
}

}

void boss_rush_timer_init(const ConfigService* config_svc, ModContext* mod_ctx,
                          ConfigVarHandle var) {
    s_cfg = config_svc;
    s_ctx = mod_ctx;
    s_var = var;
    s_state = IDLE;
    s_elapsedMs = 0;
    s_provisionalMs = 0;
    s_haveLastTick = false;

    if (s_cfg != nullptr && s_var != 0) {
        char buf[512];
        size_t len = 0;
        if (s_cfg->get_string(s_ctx, s_var, buf, sizeof(buf), &len) == MOD_OK) {
            load_from_string(buf);
        }
    }
}

void boss_rush_timer_init_chain_best(const ConfigService* config_svc, ModContext* mod_ctx,
                                     ConfigVarHandle var) {
    s_chainCfg = config_svc;
    s_chainCtx = mod_ctx;
    s_chainVar = var;
    s_chainBestCs = 0;

    if (s_chainCfg != nullptr && s_chainVar != 0) {
        char buf[32];
        size_t len = 0;
        if (s_chainCfg->get_string(s_chainCtx, s_chainVar, buf, sizeof(buf), &len) == MOD_OK) {
            const long cs = std::strtol(buf, nullptr, 10);
            if (cs > 0) {
                s_chainBestCs = static_cast<u32>(cs);
            }
        }
    }
}

void boss_rush_timer_update() {
    if (!g_configBossRushTimer) {
        s_state = IDLE; s_elapsedMs = 0; s_provisionalMs = 0; s_haveLastTick = false; return;
    }

    const bool fightingHere = boss_rush_is_fighting_here() &&
                              !boss_rush_is_returning_to_chamber() &&
                              daAlink_getAlinkActorClass() != nullptr;

    static int s_notFightingFrames = 0;
    if (!fightingHere) {
        s_notFightingFrames++;
        if (s_state == RUNNING) s_state = IDLE;
        if (s_state == FINISHED && !boss_rush_is_returning_to_chamber() &&
            s_notFightingFrames > 20) {
            s_state = IDLE;
        }
        return;
    }
    s_notFightingFrames = 0;

    const int idx = boss_rush_current_target_index();
    if (idx < 0) return;

    switch (s_state) {
    case IDLE: {
        if (boss_rush_settle_window_active()) {
            break;
        }
        const char* lbl = nullptr;
        bool engaged = true;
        if (boss_bar_current_fight_state(&lbl, engaged) && engaged) {
            s_state = RUNNING;
            if (s_chainRun) {
                s_chainCheckpointMs = s_elapsedMs;
            } else {
                s_elapsedMs = 0;
            }
            s_provisionalMs = 0;
            s_haveLastTick = false;
            s_eventActive = false;
            s_idx = idx;
        }
        break;
    }
    case RUNNING: {
        if (boss_bar_boss_defeated_now()) {
            finalize();
            break;
        }

        const auto now = std::chrono::steady_clock::now();

        const bool eventNow = dComIfGp_event_runCheck() != 0;
        if (eventNow) {
            if (!s_eventActive) { s_eventActive = true; s_eventStart = now; }
        } else {
            s_eventActive = false;
        }
        const long long eventMs = s_eventActive
            ? std::chrono::duration_cast<std::chrono::milliseconds>(now - s_eventStart).count()
            : 0;
        const bool hardPause = dComIfGp_isPauseFlag() != 0;
        const bool cutsceneConfirmed = s_eventActive && eventMs > kCutsceneMs;

        long long d = 0;
        if (s_haveLastTick) {
            d = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastTick).count();
            if (d < 0 || d >= 1000) d = 0;
        }
        s_lastTick = now;
        s_haveLastTick = true;

        float timescale = general_get_aurora_timescale();
        if (!(timescale > 0.0f)) timescale = 1.0f;
        d = static_cast<long long>(static_cast<double>(d) * static_cast<double>(timescale) + 0.5);

        if (hardPause || cutsceneConfirmed) {
            s_provisionalMs = 0;
        } else if (s_eventActive) {
            s_provisionalMs += d;
        } else {
            s_elapsedMs += static_cast<unsigned long long>(s_provisionalMs + d);
            s_provisionalMs = 0;
        }
        break;
    }
    case FINISHED:
        if (s_chainRun && idx != s_idx) {
            s_state = IDLE;
        }
        break;
    }
}

void boss_rush_timer_notify_defeat() {
    if (!g_configBossRushTimer) return;
    finalize();
}

void boss_rush_timer_reset_run() {
    if (s_state == RUNNING) s_state = IDLE;
    if (!s_chainRun) {
        s_elapsedMs = 0;
    }
    s_provisionalMs = 0;
    s_haveLastTick = false;
}

bool boss_rush_timer_active_cs(unsigned int* outCs) {
    if (!g_configBossRushTimer) return false;
    if (s_state == RUNNING)  { if (outCs) *outCs = cs_now(); return true; }
    if (s_state == FINISHED) {
        if (outCs) *outCs = s_chainRun ? cs_now() : s_finalCs;
        return true;
    }
    if (s_chainRun && s_elapsedMs > 0) {
        if (outCs) *outCs = cs_now();
        return true;
    }
    return false;
}

bool boss_rush_timer_best_cs(int tableIndex, unsigned int* outCs) {
    if (tableIndex < 0 || tableIndex >= static_cast<int>(kMaxBossGalleryEntries)) return false;
    if (s_best[tableIndex] == 0) return false;
    if (outCs) *outCs = s_best[tableIndex];
    return true;
}

bool boss_rush_timer_result(unsigned int* outCs, bool* outIsRecord) {
    if (s_chainRun) return false;
    if (s_state != FINISHED) return false;
    if (outCs) *outCs = s_finalCs;
    if (outIsRecord) *outIsRecord = s_isRecord;
    return true;
}

bool boss_rush_timer_last_was_record() {
    return s_state == FINISHED && s_isRecord;
}

void boss_rush_timer_clear_best() {
    for (u32& b : s_best) b = 0;
    save_to_string();
}

void boss_rush_timer_begin_chain_run() {
    s_chainRun = true;
    s_chainCheckpointMs = 0;
    s_state = IDLE;
    s_elapsedMs = 0;
    s_provisionalMs = 0;
    s_haveLastTick = false;
    s_idx = -1;
    s_finalCs = 0;
    s_isRecord = false;
}

void boss_rush_timer_end_chain_run() {
    s_chainRun = false;
    s_chainCheckpointMs = 0;
}

bool boss_rush_timer_chain_active() {
    return s_chainRun;
}

bool boss_rush_timer_chain_best_cs(unsigned int* outCs) {
    if (s_chainBestCs == 0) return false;
    if (outCs) *outCs = s_chainBestCs;
    return true;
}

void boss_rush_timer_commit_chain_total() {
    if (!s_chainRun) return;

    const u32 totalCs = cs_now();
    if (totalCs == 0) return;

    const bool record = (s_chainBestCs == 0) || (totalCs < s_chainBestCs);
    if (record) {
        s_chainBestCs = totalCs;
        if (s_chainCfg != nullptr && s_chainCtx != nullptr && s_chainVar != 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%u", totalCs);
            s_chainCfg->set_string(s_chainCtx, s_chainVar, buf);
        }
        Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_DROP_COMPLETE, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }

    // Present the full-run total as the end-of-run result (golden panel on the
    // chamber return when it was a record).
    s_state = FINISHED;
    s_finalCs = totalCs;
    s_isRecord = record;
    s_idx = -1;
}

void boss_rush_timer_format(unsigned int cs, char* buf, size_t bufLen) {
    const u32 m = cs / 6000u;
    const u32 s = (cs / 100u) % 60u;
    const u32 c = cs % 100u;
    std::snprintf(buf, bufLen, "%u:%02u.%02u", m, s, c);
}
