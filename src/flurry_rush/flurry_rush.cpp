#include "flurry_rush.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_b_tn.h"
#include "d/actor/d_a_b_gnd.h"
#include "d/actor/d_a_b_gg.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_uty.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "SSystem/SComponent/c_math.h"
#include "m_Do/m_Do_audio.h"
#include "Z2AudioLib/Z2Creature.h"
#include "m_Do/m_Do_controller_pad.h"
#include "mods/svc/hook.hpp"
#include "dusk/config_var.hpp"
#include "dusk/settings.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string_view>

bool g_configFlurryRushEnabled = false;
int g_configFlurryRushPerfectFrames = 30;
int g_configFlurryRushSlowFactor = 30;
int g_configFlurryRushWindowTicks = 150;
int g_configFlurryRushHits = 4;
int g_configFlurryRushIdleFrames = 45;

namespace {

constexpr int kCooldownTicks = 45;
constexpr int kPostRushGraceTicks = 30;
constexpr int kMinRushTicks = 30;
constexpr int kFinishGraceTicks = 15;
constexpr f32 kFlurryApproachRange = 150.0f;
constexpr f32 kFlurryApproachSpeed = 22.0f;
constexpr int kLinkSlowTicks = 5;
constexpr int kMinWindowTicks = 90;

constexpr f32 kFlurryReachXZ = 260.0f;
constexpr f32 kFlurryReachY = 200.0f;

constexpr int kBonusDelayTicks = 2;

constexpr int kSwingResolveTicks = 4;
constexpr int kMaxAtp = 8;

constexpr f32 kMaxRushDistance = 300.0f;
constexpr f32 kDodgeSensorRadiusScale = 3.0f;
constexpr f32 kSwingSnapDistance = 125.0f;
constexpr f32 kSwingMinDistance = 95.0f;
constexpr f32 kSwingMaxDistance = 150.0f;
constexpr f32 kSwingSnapMaxDistance = 450.0f;
constexpr f32 kDodgeSensorExtraHeight = 60.0f;
constexpr u32 kDefaultHitMapInfo = 30;

enum class State {
    IDLE,
    ARMED,
    RUSH,
};

State s_state = State::IDLE;
int s_stateTicks = 0;
int s_cooldown = 0;
int s_postRushGrace = 0;
u16 s_prevProc = 0;
int s_hitCount = 0;
int s_finishTicks = -1;
int s_idleTicks = 0;
fpc_ProcID s_targetId = fpcM_ERROR_PROCESS_ID_e;
bool s_atHitPrev[5] = {};

bool s_reentering = false;
float s_execAccumulator = 0.0f;

const HookService* s_hookSvc = nullptr;
bool s_hookSvcSet = false;
bool s_hooksInstalled = false;
const LogService* s_log = nullptr;

int s_swingCount = 0;
int s_contactCount = 0;
int s_realHitCount = 0;
int s_bonusHitCount = 0;
bool s_swingOpen = false;
bool s_swingLanded = false;
int s_swingAtp = 0;
int s_swingTicks = 0;

int s_observedPower[kMaxAtp] = {};

bool s_targetProven = false;
Z2Creature* s_hitSound = nullptr;
u32 s_hitSeId = 0;
u32 s_hitMapInfo = kDefaultHitMapInfo;
int s_pendingBonus = 0;
int s_pendingBonusTicks = 0;
int s_pendingBonusAtp = 0;

void flog(const char* fmt, ...) {
    if (s_log == nullptr) return;
    char buf[192];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    s_log->info(mod_ctx, buf);
}

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
        if (attacker != nullptr && attacker != link && fopAcM_GetGroup(attacker) != fopAc_PLAYER_e) {
            return attacker;
        }
    }
    return nullptr;
}

void clear_rush_state() {
    s_hitCount = 0;
    s_swingCount = 0;
    s_contactCount = 0;
    s_realHitCount = 0;
    s_bonusHitCount = 0;
    s_swingOpen = false;
    s_swingLanded = false;
    s_swingAtp = 0;
    s_swingTicks = 0;
    for (int& power : s_observedPower) power = 0;
    s_targetProven = false;
    s_hitSound = nullptr;
    s_hitSeId = 0;
    s_hitMapInfo = kDefaultHitMapInfo;
    s_pendingBonus = 0;
    s_pendingBonusTicks = 0;
    s_pendingBonusAtp = 0;
    s_finishTicks = -1;
    s_idleTicks = 0;
    s_targetId = fpcM_ERROR_PROCESS_ID_e;
    s_execAccumulator = 0.0f;
    for (int i = 0; i < 5; i++) {
        s_atHitPrev[i] = false;
    }
}

void close_swing();
void apply_bonus_hit(int atp);

void end_rush(const char* reason = "disabled") {
    if (s_state == State::RUSH) {
        close_swing();
        for (; s_pendingBonus > 0; --s_pendingBonus) {
            apply_bonus_hit(s_pendingBonusAtp);
        }
        flog("flurry: rush end (%s): swings=%d contacts=%d real hits=%d bonus hits=%d", reason,
             s_swingCount, s_contactCount, s_realHitCount, s_bonusHitCount);
        restore_time();
        s_cooldown = kCooldownTicks;
        s_postRushGrace = kPostRushGraceTicks;
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
    fopAc_ac_c* target = player != nullptr ? player->mTargetedActor : nullptr;
    flog("flurry: rush start, target actor %d (id %u) hp %d",
         target ? fopAcM_GetName(target) : -1, target ? fopAcM_GetID(target) : 0u,
         target ? target->health : -1);
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
DEFINE_HOOK(&cc_at_check, FlurryRushAtCheckHook);

void clear_hit_stop() {
    dScnPly_c::nextPauseTimer = 0;
    dScnPly_c::pauseTimer = 0;
}

void clear_cut_recovery(daAlink_c* link) {
    if (is_attack_proc(static_cast<u16>(link->mProcID)) && link->mProcVar0.field_0x3008 > 0) {
        link->mProcVar0.field_0x3008 = 0;
    }
}

fopAc_ac_c* rush_target(daAlink_c* link) {
    if (s_targetId != fpcM_ERROR_PROCESS_ID_e) return fopAcM_SearchByID(s_targetId);
    return link != nullptr ? link->mTargetedActor : nullptr;
}

int sword_hit_power(daAlink_c* link, int atp) {
    if (atp >= 0 && atp < kMaxAtp && s_observedPower[atp] > 0) return s_observedPower[atp];
    int power = atp <= 1 ? 1 : atp == 2 ? 10 : atp == 3 ? 30 : atp == 6 ? 80 : 200;
    if (link->checkMasterSwordEquip()) power *= 2;
    if (daPy_py_c::checkWoodSwordEquip()) power /= 2;
    return power < 1 ? 1 : power;
}

void apply_bonus_hit(int atp) {
    auto* link = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
    fopAc_ac_c* target = rush_target(link);
    if (link == nullptr || target == nullptr || target->health <= 0) return;
    if (fopAcM_CheckStatus(target, fopAcStts_BOSS_e)) {
        flog("flurry: swing without damage, bosses only take real hits");
        return;
    }
    if (!s_targetProven) {
        flog("flurry: swing without damage, target has not taken a real sword hit yet");
        return;
    }

    const cXyz diff = target->current.pos - link->current.pos;
    if (diff.absXZ() > kFlurryReachXZ || std::fabs(diff.y) > kFlurryReachY) {
        flog("flurry: swing missed, target %.0f away", diff.absXZ());
        return;
    }

    int damage = sword_hit_power(link, atp);

    const s16 before = target->health;
    const int after = before - damage < 1 ? 1 : before - damage;
    target->health = static_cast<s16>(after);

    cXyz pos = target->eyePos;
    dComIfGp_setHitMark(1, target, &pos, nullptr, nullptr, 0);
    if (s_hitSound != nullptr) {
        s_hitSound->startCollisionSE(s_hitSeId, s_hitMapInfo);
    }

    ++s_bonusHitCount;
    ++s_hitCount;
    flog("flurry: bonus hit #%d on actor %d (id %u): hp %d -> %d (no damage registered)",
         s_bonusHitCount, fopAcM_GetName(target), fopAcM_GetID(target), before, after);
}

void close_swing() {
    if (!s_swingOpen) return;
    s_swingOpen = false;
    if (s_swingLanded) return;
    ++s_pendingBonus;
    s_pendingBonusTicks = kBonusDelayTicks;
    s_pendingBonusAtp = s_swingAtp;
}

void snap_to_target(daAlink_c* link) {
    fopAc_ac_c* target = rush_target(link);
    if (target == nullptr || target->health <= 0) return;
    const s16 angle = fopAcM_searchActorAngleY(link, target);
    link->current.angle.y = angle;
    link->shape_angle.y = angle;
    cXyz diff = target->current.pos - link->current.pos;
    const f32 dist = diff.absXZ();
    if (dist > kSwingSnapMaxDistance) return;
    if (dist < 1.0f) {
        link->current.pos.x -= cM_ssin(link->shape_angle.y) * kSwingSnapDistance;
        link->current.pos.z -= cM_scos(link->shape_angle.y) * kSwingSnapDistance;
        return;
    }
    if (dist >= kSwingMinDistance && dist <= kSwingMaxDistance) return;
    const f32 k = (dist - kSwingSnapDistance) / dist;
    link->current.pos.x += diff.x * k;
    link->current.pos.z += diff.z * k;
}

void on_swing_start(daAlink_c* link, const char* kind) {
    if (s_state != State::RUSH || link == nullptr) return;
    snap_to_target(link);
    close_swing();
    s_swingOpen = true;
    s_swingLanded = false;
    s_swingTicks = 0;
    s_swingAtp = link->mAtCps[0].GetAtAtp();
    ++s_swingCount;
    flog("flurry: swing #%d (%s, atp %d)", s_swingCount, kind, s_swingAtp);
}

DEFINE_HOOK(&daAlink_c::procCutNormalInit, FlurrySwingNormalHook);
DEFINE_HOOK(&daAlink_c::procCutFinishInit, FlurrySwingFinishHook);
DEFINE_HOOK(&daAlink_c::procCutReverseInit, FlurrySwingReverseHook);
DEFINE_HOOK(&daAlink_c::procCutJumpInit, FlurrySwingJumpHook);
DEFINE_HOOK(&daAlink_c::procCutTurnInit, FlurrySwingTurnHook);
DEFINE_HOOK(&daAlink_c::procCutHeadInit, FlurrySwingHeadHook);
DEFINE_HOOK(&daAlink_c::procCutDownInit, FlurrySwingDownHook);

template <const char* Kind>
void on_swing_init_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || *static_cast<int*>(retval) == 0) return;
    on_swing_start(mods::arg<daAlink_c*>(args, 0), Kind);
}

constexpr char kSwingNormal[] = "normal";
constexpr char kSwingFinish[] = "finish";
constexpr char kSwingReverse[] = "reverse";
constexpr char kSwingJump[] = "jump";
constexpr char kSwingTurn[] = "spin";
constexpr char kSwingHead[] = "helm split";
constexpr char kSwingDown[] = "ending blow";

bool link_protected() {
    return s_state == State::RUSH || s_postRushGrace > 0;
}

void make_link_untouchable(daAlink_c* link) {
    for (int i = 0; i < 3; i++) {
        link->mTgCyls[i].OffTgSetBit();
        link->mTgCyls[i].ResetTgHit();
    }
}

bool is_dodge_proc(u16 proc) {
    return proc == daAlink_c::PROC_SIDESTEP || proc == daAlink_c::PROC_BACK_JUMP ||
           proc == daAlink_c::PROC_SIDESTEP_LAND || proc == daAlink_c::PROC_BACK_JUMP_LAND;
}

bool target_in_rush_range(daAlink_c* link) {
    fopAc_ac_c* target = link->mTargetedActor;
    return target != nullptr &&
           (target->current.pos - link->current.pos).absXZ() <= kMaxRushDistance;
}

bool try_arm(daAlink_c* link) {
    if (!g_configFlurryRushEnabled || s_state == State::RUSH || s_cooldown > 0) return false;
    if (link == nullptr || link->checkWolf()) return false;

    fopAc_ac_c* target = link->mTargetedActor;
    if (target == nullptr || fopAcM_GetGroup(target) != fopAc_ENEMY_e) return false;
    if (!target_in_rush_range(link)) return false;

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
    const bool shielded = link_protected();
    if (!armed && !shielded) return HOOK_CONTINUE;

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link->checkDeadAction(0)) return HOOK_CONTINUE;

    bool anyHit = false;
    for (int i = 0; i < 3; i++) {
        if (link->mTgCyls[i].ChkTgHit()) anyHit = true;
    }
    if (!anyHit) return HOOK_CONTINUE;
    if (armed && find_enemy_attacker(link) == nullptr) return HOOK_CONTINUE;

    reset_link_hit_flags(link);

    if (armed) {
        start_rush();
        make_link_untouchable(link);
    }

    if (retval != nullptr) {
        *static_cast<BOOL*>(retval) = 0;
    }
    return HOOK_SKIP_ORIGINAL;
}

static void on_at_check_post(ModContext*, void* args, void*, void*) {
    if (s_state != State::RUSH) return;
    clear_hit_stop();

    fopAc_ac_c* enemy = mods::arg<fopAc_ac_c*>(args, 0);
    dCcU_AtInfo* info = mods::arg<dCcU_AtInfo*>(args, 1);
    if (enemy == nullptr || info == nullptr || info->mpActor != dComIfGp_getPlayer(0) ||
        info->mAttackPower == 0)
    {
        return;
    }
    ++s_realHitCount;
    ++s_hitCount;
    const char* credited = "other enemy";
    if (fopAcM_GetID(enemy) == s_targetId || s_targetId == fpcM_ERROR_PROCESS_ID_e) {
        s_targetProven = true;
        if (info->mpSound != nullptr && info->mpCollider != nullptr) {
            s_hitSound = info->mpSound;
            s_hitSeId = dCcD_GObjInf::getHitSeID(
                static_cast<dCcD_GObjInf*>(info->mpCollider)->GetAtSe(), 0);
            s_hitMapInfo = info->field_0x18 != 0 ? info->field_0x18 : kDefaultHitMapInfo;
        }

        if (s_pendingBonus > 0) {
            --s_pendingBonus;
            credited = "earlier swing";
        } else if (s_swingOpen) {
            s_swingLanded = true;
            credited = "current swing";
            if (s_swingAtp >= 0 && s_swingAtp < kMaxAtp) {
                s_observedPower[s_swingAtp] = info->mAttackPower;
            }
        } else {
            credited = "no open swing";
        }
    }
    flog("flurry: real hit #%d on actor %d (id %u), power %d, hp now %d (%s)", s_realHitCount,
         fopAcM_GetName(enemy), fopAcM_GetID(enemy), info->mAttackPower, enemy->health,
         credited);
}

static HookAction on_link_execute_pre(ModContext*, void* args, void*, void*) {
    if (s_state != State::RUSH) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr) clear_cut_recovery(link);
    if (s_reentering) return HOOK_CONTINUE;
    if (link == nullptr) return HOOK_CONTINUE;
    if (!link->mLinkAcch.ChkGroundHit()) return HOOK_CONTINUE;

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const bool attacking = is_attack_proc(static_cast<u16>(link->mProcID)) ||
                           (pad.mPressedButtonFlags & PAD_BUTTON_B) != 0;
    if (!attacking) return HOOK_CONTINUE;

    fopAc_ac_c* target = rush_target(link);
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

bool s_sensorActive = false;
f32 s_sensorBaseR[3] = {};

void restore_dodge_sensor(daAlink_c* link) {
    if (!s_sensorActive) return;
    s_sensorActive = false;
    if (link == nullptr) return;
    for (int i = 0; i < 3; i++) {
        link->mTgCyls[i].SetR(s_sensorBaseR[i]);
    }
}

void apply_dodge_sensor(daAlink_c* link) {
    if (!s_sensorActive) {
        for (int i = 0; i < 3; i++) {
            s_sensorBaseR[i] = link->mTgCyls[i].GetR();
        }
        s_sensorActive = true;
    }
    for (int i = 0; i < 3; i++) {
        link->mTgCyls[i].SetR(s_sensorBaseR[i] * kDodgeSensorRadiusScale);
        link->mTgCyls[i].SetH(link->mTgCyls[i].GetH() + kDodgeSensorExtraHeight);
    }
}

DEFINE_HOOK(&daAlink_c::setDamagePoint, FlurryRushSetDamagePointHook);

static HookAction on_set_damage_point_pre(ModContext*, void*, void* retval, void*) {
    if (!link_protected()) return HOOK_CONTINUE;
    if (retval != nullptr) *static_cast<int*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&daAlink_c::setThrowDamage, FlurryRushThrowDamageHook);

static HookAction on_throw_damage_pre(ModContext*, void*, void* retval, void*) {
    if (!link_protected()) return HOOK_CONTINUE;
    if (retval != nullptr) *static_cast<bool*>(retval) = false;
    return HOOK_SKIP_ORIGINAL;
}

void magnet_sword(daAlink_c* link) {
    if (s_state != State::RUSH || !s_swingOpen || s_swingLanded) return;
    if (!link->mAtCps[0].ChkAtSet()) return;
    fopAc_ac_c* target = rush_target(link);
    if (target == nullptr || target->health <= 0) return;
    cXyz center = (target->current.pos + target->eyePos) * 0.5f;
    if ((center - link->current.pos).absXZ() > kSwingSnapMaxDistance) return;
    cXyz start = link->current.pos;
    start.y = center.y;
    link->mAtCps[0].SetStartEnd(start, center);
}

static void on_link_execute_post(ModContext*, void* args, void*, void*) {
    if (link_protected()) {
        daAlink_c* rushLink = mods::arg<daAlink_c*>(args, 0);
        if (rushLink != nullptr) {
            make_link_untouchable(rushLink);
            magnet_sword(rushLink);
        }
    }
    if (!s_reentering) {
        daAlink_c* sensorLink = mods::arg<daAlink_c*>(args, 0);
        if (sensorLink != nullptr && s_state == State::ARMED) {
            apply_dodge_sensor(sensorLink);
        } else {
            restore_dodge_sensor(sensorLink);
        }
    }
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
    if (link_protected()) {
        make_link_untouchable(link);
        magnet_sword(link);
    }
}

DEFINE_HOOK(&daB_TN_c::damage_check, FlurryDarknutDamageCheckHook);
DEFINE_HOOK(&daB_TN_c::setBodyShield, FlurryDarknutBodyShieldHook);

u8 s_savedCutType = 0;
bool s_cutTypeSwapped = false;

bool darknut_rush_target(daB_TN_c* tn) {
    if (s_state != State::RUSH || tn == nullptr) return false;
    return s_targetId == fpcM_ERROR_PROCESS_ID_e || fopAcM_GetID(tn) == s_targetId;
}

void darknut_drop_guard(daB_TN_c* tn) {
    tn->field_0xa91 = false;
    tn->mTimer10 = 0;
    if (tn->mActionMode1 > 1 && tn->mActionMode1 < 8 && tn->mTimer12 < 2) {
        tn->mTimer12 = 2;
    }
}

static HookAction on_darknut_damage_check_pre(ModContext*, void* args, void*, void*) {
    daB_TN_c* tn = mods::arg<daB_TN_c*>(args, 0);
    if (!darknut_rush_target(tn)) return HOOK_CONTINUE;
    darknut_drop_guard(tn);
    if (tn->mInvincibilityTimer > 2) tn->mInvincibilityTimer = 2;
    auto* player = static_cast<daPy_py_c*>(dComIfGp_getPlayer(0));
    if (player != nullptr) {
        const u8 cut = player->mCutType;
        if (cut == daPy_py_c::CUT_TYPE_DASH_LEFT || cut == daPy_py_c::CUT_TYPE_DASH_RIGHT) {
            s_savedCutType = cut;
            s_cutTypeSwapped = true;
            player->mCutType = cut == daPy_py_c::CUT_TYPE_DASH_LEFT ? daPy_py_c::CUT_TYPE_NM_LEFT
                                                                    : daPy_py_c::CUT_TYPE_NM_RIGHT;
        }
    }
    return HOOK_CONTINUE;
}

static void on_darknut_damage_check_post(ModContext*, void*, void*, void*) {
    if (!s_cutTypeSwapped) return;
    s_cutTypeSwapped = false;
    auto* player = static_cast<daPy_py_c*>(dComIfGp_getPlayer(0));
    if (player != nullptr) player->mCutType = s_savedCutType;
}

static HookAction on_darknut_body_shield_pre(ModContext*, void* args, void*, void*) {
    daB_TN_c* tn = mods::arg<daB_TN_c*>(args, 0);
    if (darknut_rush_target(tn)) darknut_drop_guard(tn);
    return HOOK_CONTINUE;
}

DEFINE_HOOK_SYMBOL("src/d/actor/d_a_b_gnd.cpp#damage_check", void(b_gnd_class*), FlurryGanondorfDamageCheckHook);

constexpr int kGndHitGapFrames = 3;
int s_gndHitGap = 0;

bool ganondorf_duel_action(s16 mode) {
    return mode == 10 || mode == 11 || mode == 12 || mode == 13 || mode == 14 || mode == 20;
}

static HookAction on_ganondorf_damage_check_pre(ModContext*, void* args, void*, void*) {
    if (s_gndHitGap > 0) s_gndHitGap--;
    b_gnd_class* gnd = mods::arg<b_gnd_class*>(args, 0);
    if (gnd == nullptr || s_state != State::RUSH) return HOOK_CONTINUE;
    if (s_targetId != fpcM_ERROR_PROCESS_ID_e && fopAcM_GetID(gnd) != s_targetId) return HOOK_CONTINUE;
    if (gnd->mDrawHorse || !ganondorf_duel_action(gnd->mActionMode)) return HOOK_CONTINUE;

    gnd->field_0xc79 = 0;
    if (s_gndHitGap > 0) {
        if (gnd->mDamageInvulnerabilityTimer < 1) gnd->mDamageInvulnerabilityTimer = 1;
        return HOOK_CONTINUE;
    }
    gnd->mDamageInvulnerabilityTimer = 0;
    for (int i = 0; i < 3; i++) {
        if (gnd->mGndSph[i].ChkTgHit()) {
            s_gndHitGap = kGndHitGapFrames;
            break;
        }
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&daB_GG_c::Execute, FlurryAeralfosExecuteHook);
DEFINE_HOOK(&daB_GG_c::CutChk, FlurryAeralfosCutChkHook);

bool is_rush_target_actor(fopAc_ac_c* actor) {
    if (s_state != State::RUSH || actor == nullptr) return false;
    return s_targetId == fpcM_ERROR_PROCESS_ID_e || fopAcM_GetID(actor) == s_targetId;
}

static void on_aeralfos_execute_post(ModContext*, void* args, void*, void*) {
    daB_GG_c* gg = mods::arg<daB_GG_c*>(args, 0);
    if (!is_rush_target_actor(gg)) return;
    gg->mCcCyl.OffTgShield();
    gg->mCcShieldSph.OffTgShield();
    gg->mCcShieldSph.OffTgSetBit();
    if (gg->field_0x5cc > 3) gg->field_0x5cc = 3;
}

static HookAction on_aeralfos_cut_chk_pre(ModContext*, void* args, void*, void*) {
    return is_rush_target_actor(mods::arg<daB_GG_c*>(args, 0)) ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
}

void install_hooks() {
    if (s_hooksInstalled || s_hookSvc == nullptr) return;
    mods::hook::add_post<FlurryRushSideStepInitHook>(s_hookSvc, on_sidestep_init_post);
    mods::hook::add_pre<FlurryRushDamageActionHook>(s_hookSvc, on_check_damage_action_pre);
    mods::hook::add_pre<FlurryRushSetDamagePointHook>(s_hookSvc, on_set_damage_point_pre);
    mods::hook::add_pre<FlurryRushThrowDamageHook>(s_hookSvc, on_throw_damage_pre);
    mods::hook::add_pre<FlurryRushExecuteHook>(s_hookSvc, on_link_execute_pre);
    mods::hook::add_post<FlurryRushExecuteHook>(s_hookSvc, on_link_execute_post);
    mods::hook::add_post<FlurrySwingNormalHook>(s_hookSvc, on_swing_init_post<kSwingNormal>);
    mods::hook::add_post<FlurrySwingFinishHook>(s_hookSvc, on_swing_init_post<kSwingFinish>);
    mods::hook::add_post<FlurrySwingReverseHook>(s_hookSvc, on_swing_init_post<kSwingReverse>);
    mods::hook::add_post<FlurrySwingJumpHook>(s_hookSvc, on_swing_init_post<kSwingJump>);
    mods::hook::add_post<FlurrySwingTurnHook>(s_hookSvc, on_swing_init_post<kSwingTurn>);
    mods::hook::add_post<FlurrySwingHeadHook>(s_hookSvc, on_swing_init_post<kSwingHead>);
    mods::hook::add_post<FlurrySwingDownHook>(s_hookSvc, on_swing_init_post<kSwingDown>);
    mods::hook::add_pre<FlurryDarknutDamageCheckHook>(s_hookSvc, on_darknut_damage_check_pre);
    mods::hook::add_post<FlurryDarknutDamageCheckHook>(s_hookSvc, on_darknut_damage_check_post);
    mods::hook::add_pre<FlurryDarknutBodyShieldHook>(s_hookSvc, on_darknut_body_shield_pre);
    mods::hook::add_pre<FlurryGanondorfDamageCheckHook>(s_hookSvc, on_ganondorf_damage_check_pre);
    mods::hook::add_post<FlurryAeralfosExecuteHook>(s_hookSvc, on_aeralfos_execute_post);
    mods::hook::add_pre<FlurryAeralfosCutChkHook>(s_hookSvc, on_aeralfos_cut_chk_pre);
    s_hooksInstalled = true;
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
    }
}

ModResult init_flurry_rush(const HookService* hook_svc, const LogService* log_svc, ModError*) {
    if (hook_svc == nullptr) return MOD_ERROR;
    s_log = log_svc;
    s_hookSvc = hook_svc;
    s_hookSvcSet = true;

    mods::hook::add_post<FlurryRushAtCheckHook>(hook_svc, on_at_check_post);

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
    if (s_postRushGrace > 0) s_postRushGrace--;

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
        ++s_stateTicks;
        const bool dodging = player != nullptr && is_dodge_proc(static_cast<u16>(player->mProcID));
        if (s_stateTicks > g_configFlurryRushPerfectFrames && !dodging) {
            s_state = State::IDLE;
        }
        return;
    }

    if (s_state != State::RUSH) return;

    s_stateTicks++;
    auto* link = player;
    clear_hit_stop();

    if (s_targetId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* target = fopAcM_SearchByID(s_targetId);
        if (target == nullptr || target->health <= 0) {
            end_rush("target gone");
            return;
        }
    }

    if (link != nullptr) {
        const int newContacts = poll_enemy_hits(link);
        if (newContacts > 0) {
            s_contactCount += newContacts;
            flog("flurry: sword contact (%d so far)", s_contactCount);
        }

        if (s_swingOpen && (++s_swingTicks >= kSwingResolveTicks ||
                            !is_attack_proc(static_cast<u16>(link->mProcID))))
        {
            close_swing();
        }
        if (s_pendingBonus > 0 && --s_pendingBonusTicks <= 0) {
            for (; s_pendingBonus > 0; --s_pendingBonus) {
                apply_bonus_hit(s_pendingBonusAtp);
            }
        }

        if (s_finishTicks < 0 && s_hitCount >= g_configFlurryRushHits) {
            s_finishTicks = kFinishGraceTicks;
        }

        if (s_finishTicks >= 0) {
            s_finishTicks--;
            if (s_finishTicks < 0) {
                end_rush("flurry complete");
            }
            return;
        }
    }

    if (link != nullptr && !is_attack_proc(static_cast<u16>(link->mProcID)) && !s_swingOpen) {
        if (++s_idleTicks >= g_configFlurryRushIdleFrames) {
            end_rush("no attack");
            return;
        }
    } else {
        s_idleTicks = 0;
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
        end_rush("window over or interrupted");
    }
}

void shutdown_flurry_rush() {
    restore_dodge_sensor(static_cast<daAlink_c*>(dComIfGp_getPlayer(0)));
    end_rush();
    s_postRushGrace = 0;
    s_hookSvcSet = false;
    s_hookSvc = nullptr;
    s_frameInterpVar = nullptr;
}
