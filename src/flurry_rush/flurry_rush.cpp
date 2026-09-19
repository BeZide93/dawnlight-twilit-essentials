#include "flurry_rush.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "mods/svc/hook.hpp"
#include "dusk/config_var.hpp"
#include "dusk/settings.h"

#include <cstdio>
#include <string_view>

bool g_configFlurryRushEnabled = false;
int g_configFlurryRushPerfectFrames = 30;
int g_configFlurryRushSlowFactor = 30;
int g_configFlurryRushWindowTicks = 150;
int g_configFlurryRushHits = 4;

namespace {

constexpr int kCooldownTicks = 45;
constexpr int kMinRushTicks = 30;
constexpr int kFinishGraceTicks = 15;
constexpr f32 kFlurryApproachRange = 120.0f;
constexpr f32 kFlurryApproachSpeed = 22.0f;
constexpr int kLinkSlowTicks = 5;
constexpr int kMinWindowTicks = 90;

enum class State {
    IDLE,
    ARMED,
    RUSH,
};

State s_state = State::IDLE;
int s_stateTicks = 0;
int s_cooldown = 0;
u16 s_prevProc = 0;
int s_hitCount = 0;
int s_finishTicks = -1;
fpc_ProcID s_targetId = fpcM_ERROR_PROCESS_ID_e;
bool s_atHitPrev[5] = {};

bool s_reentering = false;
float s_execAccumulator = 0.0f;

const HookService* s_hookSvc = nullptr;
bool s_hookSvcSet = false;
bool s_hooksInstalled = false;

using SetSimRateFn = void (*)(float);
using GetSimRateFn = float (*)();
SetSimRateFn s_setSimRate = nullptr;
GetSimRateFn s_getSimRate = nullptr;
using SetAuroraScaleFn = void (*)(float);
SetAuroraScaleFn s_setAuroraScale = nullptr;
float s_baselineRate = 30.0f;
constexpr float kSimPeriod = 1.0f / 30.0f;

using GetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
void* s_frameInterpVar = nullptr;
bool s_interpOverridden = false;

void override_frame_interp(bool enable) {
    if (s_frameInterpVar == nullptr) return;
    auto* var = static_cast<dusk::config::ConfigVar<dusk::FrameInterpMode>*>(s_frameInterpVar);
    if (enable) {
        var->setOverrideValue(dusk::FrameInterpMode::Unlimited);
        s_interpOverridden = true;
    } else if (s_interpOverridden) {
        var->clearOverride();
        s_interpOverridden = false;
    }
}

void slow_time() {
    if (s_getSimRate != nullptr) {
        s_baselineRate = s_getSimRate();
    }
    if (s_setAuroraScale != nullptr) {
        s_setAuroraScale(s_baselineRate *
                         (static_cast<f32>(g_configFlurryRushSlowFactor) / 100.0f) * kSimPeriod);
    } else if (s_setSimRate != nullptr) {
        s_setSimRate(s_baselineRate * (static_cast<f32>(g_configFlurryRushSlowFactor) / 100.0f));
    }
    override_frame_interp(true);
}

void restore_time() {
    if (s_setAuroraScale != nullptr) {
        s_setAuroraScale(s_baselineRate * kSimPeriod);
    } else if (s_setSimRate != nullptr) {
        s_setSimRate(s_baselineRate);
    }
    override_frame_interp(false);
}

void reset_link_hit_flags(daAlink_c* link) {
    for (int i = 0; i < 3; i++) {
        link->mTgCyls[i].ResetTgHit();
    }
    link->mAtSph.ResetTgHit();
    link->mCcStts.ClrTg();
    link->mCcStts.ClrAt();
}

fopAc_ac_c* find_enemy_attacker(daAlink_c* link) {
    for (int i = 0; i < 3; i++) {
        if (!link->mTgCyls[i].ChkTgHit()) continue;
        fopAc_ac_c* attacker = link->mTgCyls[i].GetTgHitAc();
        if (attacker != nullptr && fopAcM_GetGroup(attacker) == fopAc_ENEMY_e) {
            return attacker;
        }
    }
    return nullptr;
}

void clear_rush_state() {
    s_hitCount = 0;
    s_finishTicks = -1;
    s_targetId = fpcM_ERROR_PROCESS_ID_e;
    s_execAccumulator = 0.0f;
    for (int i = 0; i < 5; i++) {
        s_atHitPrev[i] = false;
    }
}

void end_rush() {
    if (s_state == State::RUSH) {
        restore_time();
        s_cooldown = kCooldownTicks;
    }
    s_state = State::IDLE;
    s_stateTicks = 0;
    s_reentering = false;
    clear_rush_state();
}

void start_rush() {
    s_state = State::RUSH;
    s_stateTicks = 0;
    clear_rush_state();
    auto* player = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
    s_targetId = (player != nullptr && player->mTargetedActor != nullptr)
                     ? fopAcM_GetID(player->mTargetedActor)
                     : fpcM_ERROR_PROCESS_ID_e;
    slow_time();
}

bool is_damage_proc(u16 procId) {
    switch (procId) {
        case daAlink_c::PROC_DAMAGE:
        case daAlink_c::PROC_LARGE_DAMAGE:
        case daAlink_c::PROC_LARGE_DAMAGE_UP:
        case daAlink_c::PROC_LARGE_DAMAGE_WALL:
        case daAlink_c::PROC_LAND_DAMAGE:
        case daAlink_c::PROC_SWIM_DAMAGE:
        case daAlink_c::PROC_ELEC_DAMAGE:
        case daAlink_c::PROC_POLY_DAMAGE:
            return true;
        default:
            return false;
    }
}

int poll_enemy_hits(daAlink_c* link) {
    dCcD_GObjInf* atObjs[5] = {
        &link->mAtCps[0], &link->mAtCps[1], &link->mAtCps[2], &link->mAtCyl, &link->mAtSph,
    };
    bool hitNow[5];
    int newHits = 0;
    for (int i = 0; i < 5; i++) {
        hitNow[i] = atObjs[i]->ChkAtHit() != 0;
        if (hitNow[i] && !s_atHitPrev[i]) {
            cCcD_GObjInf* hitGObj = atObjs[i]->GetAtHitGObj();
            fopAc_ac_c* hitAc = hitGObj != nullptr ? hitGObj->GetAc() : nullptr;
            if (hitAc != nullptr && fopAcM_GetGroup(hitAc) == fopAc_ENEMY_e) {
                newHits++;
            }
        }
    }
    for (int i = 0; i < 5; i++) {
        s_atHitPrev[i] = hitNow[i];
    }
    return newHits;
}

bool is_attack_proc(u16 procId) {
    switch (procId) {
        case daAlink_c::PROC_CUT_NORMAL:
        case daAlink_c::PROC_CUT_FINISH:
        case daAlink_c::PROC_CUT_REVERSE:
        case daAlink_c::PROC_CUT_JUMP:
        case daAlink_c::PROC_CUT_TURN:
        case daAlink_c::PROC_CUT_TURN_CHARGE:
        case daAlink_c::PROC_CUT_TURN_MOVE:
        case daAlink_c::PROC_CUT_DOWN:
        case daAlink_c::PROC_CUT_HEAD:
            return true;
        default:
            return false;
    }
}

DEFINE_HOOK(&daAlink_c::procSideStepInit, FlurryRushSideStepInitHook);
DEFINE_HOOK(&daAlink_c::checkDamageAction, FlurryRushDamageActionHook);
DEFINE_HOOK(&daAlink_c::execute, FlurryRushExecuteHook);

bool try_arm(daAlink_c* link) {
    if (s_state == State::RUSH || s_cooldown > 0) return false;
    if (link == nullptr || link->checkWolf()) return false;

    fopAc_ac_c* target = link->mTargetedActor;
    if (target == nullptr || fopAcM_GetGroup(target) != fopAc_ENEMY_e) return false;

    s_state = State::ARMED;
    s_stateTicks = 0;
    return true;
}

static void on_sidestep_init_post(ModContext*, void* args, void* retval, void*) {
    if (!g_configFlurryRushEnabled || s_state == State::RUSH) return;
    if (s_cooldown > 0) return;

    if (retval == nullptr || *static_cast<BOOL*>(retval) == 0) return;

    try_arm(mods::arg<daAlink_c*>(args, 0));
}

static HookAction on_check_damage_action_pre(ModContext*, void* args, void* retval, void*) {
    const bool armed = s_state == State::ARMED;
    const bool rushing = s_state == State::RUSH;
    if (!armed && !rushing) return HOOK_CONTINUE;

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkDeadAction(0)) return HOOK_CONTINUE;

    if (armed) {
        const u16 proc = static_cast<u16>(link->mProcID);
        if (proc != daAlink_c::PROC_SIDESTEP && proc != daAlink_c::PROC_BACK_JUMP &&
            proc != daAlink_c::PROC_SIDESTEP_LAND && proc != daAlink_c::PROC_BACK_JUMP_LAND)
        {
            return HOOK_CONTINUE;
        }
    }

    if (find_enemy_attacker(link) == nullptr) return HOOK_CONTINUE;

    reset_link_hit_flags(link);

    if (armed) {
        start_rush();
    }

    if (retval != nullptr) {
        *static_cast<BOOL*>(retval) = 0;
    }
    return HOOK_SKIP_ORIGINAL;
}

static HookAction on_link_execute_pre(ModContext*, void* args, void*, void*) {
    if (s_state != State::RUSH || s_reentering) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || s_hitCount != 0) return HOOK_CONTINUE;
    if (!link->mLinkAcch.ChkGroundHit()) return HOOK_CONTINUE;

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const bool attacking = is_attack_proc(static_cast<u16>(link->mProcID)) ||
                           (pad.mPressedButtonFlags & PAD_BUTTON_B) != 0;
    if (!attacking) return HOOK_CONTINUE;

    fopAc_ac_c* target = link->mTargetedActor;
    if (target == nullptr) return HOOK_CONTINUE;

    const cXyz toTarget = target->current.pos - link->current.pos;
    if (toTarget.absXZ() <= kFlurryApproachRange) return HOOK_CONTINUE;

    const s16 angle = fopAcM_searchActorAngleY(link, target);
    link->current.angle.y = angle;
    link->shape_angle.y = angle;
    if (link->mNormalSpeed < kFlurryApproachSpeed) {
        link->mNormalSpeed = kFlurryApproachSpeed;
    }
    return HOOK_CONTINUE;
}

static void on_link_execute_post(ModContext*, void* args, void*, void*) {
    if (s_state != State::RUSH || s_reentering) return;

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkEventRun()) return;
    if (s_stateTicks < kLinkSlowTicks) return;

    s_execAccumulator += 100.0f / static_cast<f32>(g_configFlurryRushSlowFactor);
    int total = static_cast<int>(s_execAccumulator);
    if (total < 1) total = 1;
    s_execAccumulator -= static_cast<f32>(total);
    const int extra = total - 1;
    if (extra <= 0) return;

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const u16 pressed = pad.mPressedButtonFlags;
    s_reentering = true;
    pad.mPressedButtonFlags = 0;
    for (int i = 0; i < extra; i++) {
        FlurryRushExecuteHook::g_orig(link);
    }
    pad.mPressedButtonFlags = pressed;
    s_reentering = false;
}

void install_hooks() {
    if (s_hooksInstalled || s_hookSvc == nullptr) return;
    mods::hook::add_post<FlurryRushSideStepInitHook>(s_hookSvc, on_sidestep_init_post);
    mods::hook::add_pre<FlurryRushDamageActionHook>(s_hookSvc, on_check_damage_action_pre);
    mods::hook::add_pre<FlurryRushExecuteHook>(s_hookSvc, on_link_execute_pre);
    mods::hook::add_post<FlurryRushExecuteHook>(s_hookSvc, on_link_execute_post);
    s_hooksInstalled = true;
}

void uninstall_hooks() {
    if (!s_hooksInstalled || s_hookSvc == nullptr) return;
    mods::hook::uninstall<FlurryRushSideStepInitHook>(s_hookSvc);
    mods::hook::uninstall<FlurryRushDamageActionHook>(s_hookSvc);
    mods::hook::uninstall<FlurryRushExecuteHook>(s_hookSvc);
    s_hooksInstalled = false;
}

}

bool flurry_rush_is_rush_active() {
    return s_state == State::RUSH;
}

void flurry_rush_apply_enabled() {
    if (!s_hookSvcSet) return;

    if (g_configFlurryRushEnabled && s_setSimRate != nullptr) {
        install_hooks();
    } else {
        end_rush();
        uninstall_hooks();
    }
}

ModResult init_flurry_rush(const HookService* hook_svc, const LogService* log_svc, ModError*) {
    if (hook_svc == nullptr) return MOD_ERROR;
    s_hookSvc = hook_svc;
    s_hookSvcSet = true;

    if (hook_svc->resolve(mod_ctx, "dusk::game_clock::set_sim_rate",
                          reinterpret_cast<void**>(&s_setSimRate), nullptr) != MOD_OK ||
        hook_svc->resolve(mod_ctx, "dusk::game_clock::get_sim_rate",
                          reinterpret_cast<void**>(&s_getSimRate), nullptr) != MOD_OK)
    {
        s_setSimRate = nullptr;
        s_getSimRate = nullptr;
        return MOD_OK;
    }

    s_baselineRate = s_getSimRate();

    hook_svc->resolve(mod_ctx, "aurora::time::set_scale",
                      reinterpret_cast<void**>(&s_setAuroraScale), nullptr);

    GetConfigVarFn getConfigVar = nullptr;
    if (hook_svc->resolve(mod_ctx, "dusk::config::GetConfigVar",
                          reinterpret_cast<void**>(&getConfigVar), nullptr) == MOD_OK &&
        getConfigVar != nullptr)
    {
        s_frameInterpVar = getConfigVar("game.enableFrameInterpolation");
    }

    flurry_rush_apply_enabled();
    return MOD_OK;
}

void update_flurry_rush(const LogService*, ModContext*) {
    if (s_hookSvc == nullptr || s_setSimRate == nullptr) return;

    if (s_cooldown > 0) s_cooldown--;

    auto* player = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
    if (s_state == State::IDLE && player != nullptr && s_cooldown == 0) {
        const u16 proc = static_cast<u16>(player->mProcID);
        if (s_prevProc != proc &&
            (proc == daAlink_c::PROC_SIDESTEP || proc == daAlink_c::PROC_BACK_JUMP))
        {
            try_arm(player);
        }
        s_prevProc = proc;
    }

    if (s_state == State::ARMED) {
        if (++s_stateTicks > g_configFlurryRushPerfectFrames) {
            s_state = State::IDLE;
        }
        return;
    }

    if (s_state != State::RUSH) return;

    s_stateTicks++;
    auto* link = player;

    if (s_targetId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* target = fopAcM_SearchByID(s_targetId);
        if (target == nullptr || target->health <= 0) {
            end_rush();
            return;
        }
    }

    if (link != nullptr) {
        const int newHits = poll_enemy_hits(link);
        if (newHits > 0) {
            s_hitCount += newHits;
        }

        if (s_finishTicks < 0 && s_hitCount >= g_configFlurryRushHits) {
            s_finishTicks = kFinishGraceTicks;
        }

        if (s_finishTicks >= 0) {
            s_finishTicks--;
            if (s_finishTicks < 0) {
                end_rush();
            }
            return;
        }
    }

    bool shouldEnd = !g_configFlurryRushEnabled;
    if (!shouldEnd) {
        int capTicks = g_configFlurryRushWindowTicks;
        if (capTicks < kMinWindowTicks) {
            capTicks = kMinWindowTicks;
        }
        shouldEnd = link == nullptr || link->checkEventRun() ||
                dComIfGp_isPauseFlag() != 0 ||
                is_damage_proc(static_cast<u16>(link->mProcID)) ||
                s_stateTicks >= capTicks;
    }

    if (shouldEnd) {
        end_rush();
    }
}

void shutdown_flurry_rush() {
    end_rush();
    uninstall_hooks();
    s_hookSvcSet = false;
    s_hookSvc = nullptr;
    s_frameInterpVar = nullptr;
}
