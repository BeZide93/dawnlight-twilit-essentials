#include "stamina.hpp"
#include "stamina_internal.hpp"
#include "stamina_hud.hpp"
#include "sprint_human.hpp"
#include "sprint_wolf.hpp"
#include "sprint_swim.hpp"
#include "sprint_wind.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "d/d_camera.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_controller_pad.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "mods/svc/save.h"
#include "mods/svc/config.h"

#include <cstdint>

extern const SaveService* svc_save;
extern ModContext* mod_ctx;

bool g_configStaminaEnabled = false;
int  g_configStaminaMax     = 100;
bool g_configStaminaScaleWithHearts = false;
int  g_configStaminaPerHeart = 15;
int  g_configStaminaRegen   = 100;
int  g_configStaminaRegenDelay = 2;
int  g_configStaminaExhaustRecover = 35;
bool g_configStaminaSlowHangRegen = true;
bool g_configStaminaRefillOnStageChange = true;
bool g_configStaminaSwimRestRegen = true;
bool g_configStaminaSwimDrown = true;

bool g_configStaminaSrcAttacks  = true;
bool g_configStaminaSrcJumpSpin  = true;
bool g_configStaminaSrcRolls     = true;
bool g_configStaminaSrcClimb     = true;
bool g_configStaminaSrcHang      = true;
bool g_configStaminaSrcSwim = true;
bool g_configStaminaSrcPushPull  = true;
bool g_configStaminaSrcWolfDash  = true;
bool g_configStaminaSrcHiddenSkills = true;
bool g_configStaminaSrcBulletTime = true;

int g_configStaminaCostAttack     = 100;
int g_configStaminaCostJumpAttack = 100;
int g_configStaminaCostSpin       = 100;
int g_configStaminaCostRoll       = 100;
int g_configStaminaCostSidestep   = 100;
int g_configStaminaCostClimb      = 100;
int g_configStaminaCostHang       = 100;
int g_configStaminaCostCrawl      = 100;
int g_configStaminaCostSwim       = 100;
int g_configStaminaCostPushPull   = 100;
int g_configStaminaCostWolfDash   = 100;
int g_configStaminaCostSprint     = 100;
int g_configStaminaCostWolfSprint = 100;
int g_configStaminaCostSwimSprint = 100;
int g_configStaminaCostHiddenSkills = 100;
int g_configStaminaCostSpinCharge = 10;
int g_configStaminaCostBulletTime = 100;

enum StamCat {
    STAM_ATTACKS = 1,
    STAM_JUMPSPIN,
    STAM_ROLLS,
    STAM_HIDDENSKILLS,
};

static bool stam_cat_enabled(int cat) {
    switch (cat) {
    case STAM_ATTACKS:      return g_configStaminaSrcAttacks;
    case STAM_JUMPSPIN:     return g_configStaminaSrcJumpSpin;
    case STAM_ROLLS:        return g_configStaminaSrcRolls;
    case STAM_HIDDENSKILLS: return g_configStaminaSrcHiddenSkills;
    default:                return true;
    }
}

DEFINE_HOOK(&daAlink_c::swordSwingTrigger, StamSwordSwing);
DEFINE_HOOK(&daAlink_c::procFrontRollInit, StamFrontRoll);
DEFINE_HOOK(&daAlink_c::procSideRollInit,  StamSideRoll);
DEFINE_HOOK(&daAlink_c::procBackJumpInit,  StamBackJump);
DEFINE_HOOK(&daAlink_c::procSideStepInit,  StamSideStep);
DEFINE_HOOK(&daAlink_c::procCutJumpInit,            StamCutJump);
DEFINE_HOOK(&daAlink_c::procCutLargeJumpChargeInit, StamCutLargeJump);
DEFINE_HOOK(&daAlink_c::procCutTurnInit, StamCutSpin);
DEFINE_HOOK(&daAlink_c::procCutFinishInit, StamCutFinish);
DEFINE_HOOK(&daAlink_c::procCutFinishJumpUpInit, StamCutBackSlice);
DEFINE_HOOK(&daAlink_c::procCutDownInit, StamCutDown);
DEFINE_HOOK(&daAlink_c::procCutHeadInit, StamCutHead);
DEFINE_HOOK(&daAlink_c::procGuardAttackInit, StamGuardAttack);
DEFINE_HOOK(&daAlink_c::checkRestHPAnime, StaminaTiredCheck);
DEFINE_HOOK(&daAlink_c::changeHangEndProc, StamHangEnd);
DEFINE_HOOK(&daAlink_c::procHangWallCatch, StamHangWallCatch);
DEFINE_HOOK(&daAlink_c::checkLadderFall, StamLadderFall);
DEFINE_HOOK(&daAlink_c::procCutTurnCharge, StamSpinCharge);
DEFINE_HOOK(&daAlink_c::procCutTurnMove, StamSpinChargeMove);

static f32 s_stamina    = 100.0f;
static int s_regenDelay = 0;
static bool s_blockedThisFrame = false;
static bool s_swungThisFrame = false;
static int  s_suppressSwingCharge = 0;
static int  s_jumpChargeCd = 0;
static f32  s_extraDrain  = 0.0f;
static bool s_exhausted   = false;
static f32  s_regenRamp   = 0.0f;

static constexpr int kExhaustMinDelayFrames = 30;
static constexpr f32 kRegenRampStep = 1.0f / 20.0f;
static constexpr f32 kGuardRegenFactor = 0.5f;
static constexpr int kDenyCooldownFrames = 24;

static constexpr f32 kHangRestRegenFactor = 0.15f;

static constexpr int kDrownGraceFrames = 30;
static constexpr int kDrownDamage = 4;
static constexpr u32 kDrownRestartMode = 5;

static int  s_drownTimer = 0;
static bool s_drowning = false;

static f32 stamina_scaled_max_for_hearts() {
    using namespace stamina_impl;
    f32 hearts = static_cast<f32>(dComIfGs_getMaxLife() / 5);
    if (hearts < kScaleMinHearts) hearts = kScaleMinHearts;
    if (hearts > kScaleMaxHearts) hearts = kScaleMaxHearts;
    return kScaleBaseValue + (hearts - kScaleMinHearts) * static_cast<f32>(g_configStaminaPerHeart);
}

static f32 stamina_max() {
    f32 m = g_configStaminaScaleWithHearts ? stamina_scaled_max_for_hearts()
                                            : static_cast<f32>(g_configStaminaMax);
    return m < 10.0f ? 10.0f : m;
}

int stamina_effective_max() {
    return static_cast<int>(stamina_max());
}

static constexpr u8 kPlaceNameFukiKind = 12;

static bool is_place_name_message() {
    dMsgObject_c* msg = dMsgObject_getMsgObjectClass();
    return msg != nullptr && msg->getFukiKind() == kPlaceNameFukiKind;
}

static bool in_gameplay() {
    if (dMeter2Info_getWindowStatus() != 0) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    if (dComIfGp_event_runCheck()) return false;
    if (dMeter2Info_isShopTalkFlag()) return false;
    if (dMsgObject_isTalkNowCheck() && !is_place_name_message()) return false;
    return true;
}

static bool zora_armor_worn() {
    return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e;
}

static bool swim_costs_stamina(const daAlink_c* link) {
    return g_configStaminaSrcSwim && link != nullptr && !link->checkWolf() && !zora_armor_worn();
}

static bool is_swim_drain_proc(u16 proc) {
    return proc == daAlink_c::PROC_SWIM_MOVE || proc == daAlink_c::PROC_SWIM_DIVE;
}

static f32 drain_rate(const daAlink_c* link) {
    const u16 proc = link->mProcID;
    switch (proc) {
    case daAlink_c::PROC_CLIMB_MOVE_UPDOWN:
    case daAlink_c::PROC_CLIMB_MOVE_SIDE:
    case daAlink_c::PROC_CLIMB_TO_ROOF:
        return g_configStaminaSrcClimb
                   ? stamina_impl::cost_scaled(0.55f, g_configStaminaCostClimb)
                   : 0.0f;
    case daAlink_c::PROC_HANG_MOVE:
    case daAlink_c::PROC_HANG_CLIMB:
    case daAlink_c::PROC_HANG_UP:
    case daAlink_c::PROC_HANG_WALL_CATCH:
        return g_configStaminaSrcHang
                   ? stamina_impl::cost_scaled(0.45f, g_configStaminaCostHang)
                   : 0.0f;
    case daAlink_c::PROC_CRAWL_MOVE:
    case daAlink_c::PROC_CRAWL_AUTO_MOVE:
        return g_configStaminaSrcClimb
                   ? stamina_impl::cost_scaled(0.22f, g_configStaminaCostCrawl)
                   : 0.0f;
    case daAlink_c::PROC_SWIM_MOVE:
    case daAlink_c::PROC_SWIM_DIVE:
        return swim_costs_stamina(link)
                   ? stamina_impl::cost_scaled(0.40f, g_configStaminaCostSwim)
                   : 0.0f;
    case daAlink_c::PROC_PUSH_MOVE:
    case daAlink_c::PROC_PULL_MOVE:
        return g_configStaminaSrcPushPull
                   ? stamina_impl::cost_scaled(0.55f, g_configStaminaCostPushPull)
                   : 0.0f;
    case daAlink_c::PROC_WOLF_DASH:
    case daAlink_c::PROC_WOLF_DASH_REVERSE:
        return g_configStaminaSrcWolfDash
                   ? stamina_impl::cost_scaled(0.90f, g_configStaminaCostWolfDash)
                   : 0.0f;
    default:
        return 0.0f;
    }
}

static constexpr f32 kSpinChargeDrain = 0.5f;

static bool is_spin_charge(const daAlink_c* link) {
    if (link->mProcID != daAlink_c::PROC_CUT_TURN_CHARGE && link->mProcID != daAlink_c::PROC_CUT_TURN_MOVE) return false;
    return link->mProcVar2.field_0x300c == 0;
}

static f32 spin_charge_drain(const daAlink_c* link) {
    if (!g_configStaminaSrcJumpSpin || !is_spin_charge(link)) return 0.0f;
    return stamina_impl::cost_scaled(kSpinChargeDrain, g_configStaminaCostSpinCharge);
}

static bool empty() { return s_exhausted; }
static bool drained() { return s_stamina <= 0.5f; }

static void refill_stamina() {
    s_stamina = stamina_max();
    s_exhausted = false;
    s_regenDelay = 0;
    s_regenRamp = 0.0f;
    stamina_hud_refill(s_stamina);
}

static void check_exhaust() {
    if (s_stamina > 0.0f) return;
    s_stamina = 0.0f;
    if (s_exhausted) return;
    s_exhausted = true;
    s_regenRamp = 0.0f;
    if (s_regenDelay < kExhaustMinDelayFrames) s_regenDelay = kExhaustMinDelayFrames;
    stamina_hud_notify_exhaust();
}

namespace stamina_impl {
f32 max_value() { return stamina_max(); }
f32 current_value() { return s_stamina; }
bool is_empty() { return empty(); }
bool in_gameplay() { return ::in_gameplay(); }
void report_drain(f32 amount) { s_extraDrain += amount; }
f32 cost_scaled(f32 base_cost, int pct) {
    f32 p = static_cast<f32>(pct);
    if (p < 5.0f) p = 5.0f;
    return base_cost * p / 100.0f;
}
f32 main_ring_capacity(f32 maxValue) {
    if (g_configStaminaScaleWithHearts) return kScaleBaseValue;
    f32 main = maxValue < kRingCapacity ? maxValue : kRingCapacity;
    if (maxValue > main * kRingMax) main = maxValue / kRingMax;
    return main;
}
}

void stamina_add_drain(float amount) {
    if (!g_configStaminaEnabled || amount <= 0.0f) return;
    s_extraDrain += amount;
}

bool stamina_is_exhausted() {
    return g_configStaminaEnabled && s_exhausted;
}

static void tired_check_post(ModContext*, void* args, void* retval, void*) {
    if (!g_configStaminaEnabled || !empty() || !args || !retval) return;
    BOOL* out = static_cast<BOOL*>(retval);
    if (*out) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link) return;
    if (!link->checkPlayerGuard()
        && (link->checkNoUpperAnime() || link->checkHorseTiredAnime())
        && link->mTargetedActor == NULL
        && !link->checkWindSpeedOnAngle()
        && !link->checkPlayerDemoMode())
    {
        *out = 1;
    }
}

static HookAction hang_drop_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaSrcHang) return HOOK_CONTINUE;
    if (!in_gameplay() || !drained()) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link || link->checkWolf()) return HOOK_CONTINUE;
    link->speed.y = 0.0f;
    const int result = link->procFallInit(1, link->mpHIO->mAutoJump.m.mFallInterpolation);
    if (retval) *static_cast<int*>(retval) = result;
    return HOOK_SKIP_ORIGINAL;
}

static int s_denyCooldown = 0;

static void deny() {
    stamina_hud_notify_deny();
    if (!s_blockedThisFrame && s_denyCooldown <= 0) {
        s_blockedThisFrame = true;
        s_denyCooldown = kDenyCooldownFrames;
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

static HookAction spin_charge_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaSrcJumpSpin) return HOOK_CONTINUE;
    if (!empty() || !in_gameplay()) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link || !is_spin_charge(link)) return HOOK_CONTINUE;
    link->mComboCutCount = 0;
    link->mRunCutComboCount = 0;
    link->offNoResetFlg0(daPy_py_c::FLG0_UNK_8000);
    link->checkNextAction(0);
    if (is_spin_charge(link)) link->procWaitInit();
    deny();
    if (retval) *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

static f32 s_otherSpend = 0.0f;

static int  s_hiddenSkillLock = 0;

static constexpr int kHiddenSkillLockFrames = 20;
static constexpr f32 kRecentSpendDecay       = 0.25f;

static int regen_delay_frames() {
    int frames = g_configStaminaRegenDelay * 30;
    return frames < 0 ? 0 : frames;
}

static void spend_raw(f32 cost) {
    s_stamina -= cost;
    if (s_stamina < 0.0f) s_stamina = 0.0f;
    s_regenDelay = regen_delay_frames();
    s_regenRamp = 0.0f;
    stamina_hud_notify_spend();
    check_exhaust();
}

static void spend(f32 cost) {
    spend_raw(cost);
    s_otherSpend += cost;
}


static f32 cost_for_id(int id);

static constexpr f32 kSwingCost      = 10.0f;
static constexpr f32 kJumpAttackCost = 14.0f;
static constexpr f32 kHiddenSkillCost = 14.0f;

enum StamCostId {
    STAMC_ROLL = 1,
    STAMC_SIDESTEP,
    STAMC_SPIN,
    STAMC_HIDDENSKILL,
};

static f32 cost_for_id(int id) {
    switch (id) {
    case STAMC_ROLL:        return stamina_impl::cost_scaled(14.0f, g_configStaminaCostRoll);
    case STAMC_SIDESTEP:    return stamina_impl::cost_scaled(12.0f, g_configStaminaCostSidestep);
    case STAMC_SPIN:        return stamina_impl::cost_scaled(10.0f, g_configStaminaCostSpin);
    case STAMC_HIDDENSKILL: return stamina_impl::cost_scaled(kHiddenSkillCost, g_configStaminaCostHiddenSkills);
    default:                return 0.0f;
    }
}

static void hidden_skill_spend() {
    const f32 cost = cost_for_id(STAMC_HIDDENSKILL);
    const f32 refund = s_otherSpend < cost ? s_otherSpend : cost;
    if (refund > 0.0f) {
        s_stamina += refund;
        s_otherSpend -= refund;
        if (s_stamina > stamina_max()) s_stamina = stamina_max();
    }
    s_hiddenSkillLock = kHiddenSkillLockFrames;
    spend_raw(cost);
}

static HookAction action_cost_pre(ModContext*, void*, void* retval, void* userdata) {
    if (!g_configStaminaEnabled || !in_gameplay()) return HOOK_CONTINUE;
    const std::intptr_t packed = reinterpret_cast<std::intptr_t>(userdata);
    const int cat = static_cast<int>(packed >> 16);
    if (!stam_cat_enabled(cat)) return HOOK_CONTINUE;
    if (cat == STAM_HIDDENSKILLS) {

        if (empty()) {
            if (retval) *static_cast<int*>(retval) = 0;
            deny();
            return HOOK_SKIP_ORIGINAL;
        }
        hidden_skill_spend();
        return HOOK_CONTINUE;
    }
    if (s_hiddenSkillLock > 0) return HOOK_CONTINUE;
    const f32 cost = cost_for_id(static_cast<int>(packed & 0xFFFF));
    if (empty()) {
        if (retval) *static_cast<int*>(retval) = 0;
        deny();
        return HOOK_SKIP_ORIGINAL;
    }
    spend(cost);
    return HOOK_CONTINUE;
}

static constexpr int kCutFinishMortalDrawA = 3;
static constexpr int kCutFinishMortalDrawB = 4;

static HookAction cut_finish_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_configStaminaEnabled || !in_gameplay()) return HOOK_CONTINUE;
    if (!g_configStaminaSrcHiddenSkills) return HOOK_CONTINUE;
    const int type = mods::arg<int>(args, 1);
    if (type != kCutFinishMortalDrawA && type != kCutFinishMortalDrawB) return HOOK_CONTINUE;
    if (empty()) {
        if (retval) *static_cast<int*>(retval) = 0;
        deny();
        return HOOK_SKIP_ORIGINAL;
    }
    hidden_skill_spend();
    return HOOK_CONTINUE;
}

static void sword_swing_post(ModContext*, void*, void* retval, void*) {
    if (!g_configStaminaEnabled || !in_gameplay()) return;
    if (!g_configStaminaSrcAttacks || s_hiddenSkillLock > 0) return;
    if (retval == nullptr || *static_cast<int*>(retval) == 0) return;
    if (s_suppressSwingCharge > 0) { s_swungThisFrame = true; return; }
    if (s_swungThisFrame) return;
    if (empty()) {
        *static_cast<int*>(retval) = 0;
        deny();
        return;
    }
    s_swungThisFrame = true;
    spend(stamina_impl::cost_scaled(kSwingCost, g_configStaminaCostAttack));
}

static HookAction jump_attack_pre(ModContext*, void*, void* retval, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaSrcJumpSpin) return HOOK_CONTINUE;
    if (!in_gameplay() || s_hiddenSkillLock > 0) return HOOK_CONTINUE;
    if (s_jumpChargeCd <= 0) {
        if (empty()) {
            if (retval) *static_cast<int*>(retval) = 0;
            deny();
            return HOOK_SKIP_ORIGINAL;
        }
        const f32 already = s_swungThisFrame ? stamina_impl::cost_scaled(kSwingCost, g_configStaminaCostAttack) : 0.0f;
        const f32 extra = stamina_impl::cost_scaled(kJumpAttackCost, g_configStaminaCostJumpAttack) - already;
        if (extra > 0.0f) spend(extra);
        s_jumpChargeCd = 30;
    }
    s_swungThisFrame = true;
    s_suppressSwingCharge = 6;
    return HOOK_CONTINUE;
}

static bool is_hidden_skill_proc_state(daAlink_c* link) {
    switch (link->mProcID) {
    case daAlink_c::PROC_GUARD_ATTACK:
    case daAlink_c::PROC_CUT_FINISH_JUMP_UP:
    case daAlink_c::PROC_CUT_FINISH_JUMP_UP_LAND:
    case daAlink_c::PROC_CUT_DOWN:
    case daAlink_c::PROC_CUT_DOWN_LAND:
    case daAlink_c::PROC_CUT_HEAD:
    case daAlink_c::PROC_CUT_HEAD_LAND:
        return true;
    case daAlink_c::PROC_CUT_FINISH:
        return link->getCutType() == daAlink_c::CUT_TYPE_MORTAL_DRAW_A ||
               link->getCutType() == daAlink_c::CUT_TYPE_MORTAL_DRAW_B;
    default:
        return false;
    }
}

static bool is_hang_rest_proc(daAlink_c* link) {
    if (!link) return false;
    switch (link->mProcID) {
    case daAlink_c::PROC_HANG_WAIT:
    case daAlink_c::PROC_HANG_READY:
    case daAlink_c::PROC_HANG_WALL_CATCH:
    case daAlink_c::PROC_CLIMB_WAIT:
        return true;
    default:
        return false;
    }
}

static bool is_swim_rest_proc(const daAlink_c* link) {
    return link != nullptr && link->mProcID == daAlink_c::PROC_SWIM_WAIT && swim_costs_stamina(link);
}

static void reset_drown() {
    s_drownTimer = 0;
    s_drowning = false;
}

static bool start_drown(daAlink_c* link) {
    if (!dComIfGp_event_compulsory(link, NULL, 0xFFFF)) return false;
    link->mDemo.setSpecialDemoType();
    if (!link->commonProcInitNotSameProc(daAlink_c::PROC_LAVA_RETURN)) return true;
    link->onNoResetFlg0(daAlink_c::FLG0_SWIM_UP);
    link->setSingleAnimeBase(daAlink_c::ANM_SWIM_DROWN);
    if (link->mEquipItem == 0x103) link->mLeftHandIndex = 100;
    link->voiceStart(Z2SE_AL_V_FALL_QUICKSAND);
    link->seStartOnlyReverb(Z2SE_AL_WATER_STROKE_L);
    link->field_0x32cc = kDrownRestartMode;
    link->field_0x3198 = kDrownDamage;
    link->field_0x3080 = 0;
    link->mDamageTimer = 0;
    link->mNormalSpeed = 0.0f;
    link->field_0x3194 = 1;
    dCam_getBody()->StartEventCamera(9, fopAcM_GetID(link), "Type", 1, &link->field_0x3194, nullptr);
    return true;
}

static void update_drown(daAlink_c* link) {
    if (!g_configStaminaSwimDrown || link == nullptr || !swim_costs_stamina(link)
        || !link->checkModeFlg(daAlink_c::MODE_SWIMMING)) {
        reset_drown();
        return;
    }
    if (s_drowning || link->mProcID == daAlink_c::PROC_LAVA_RETURN) return;
    if (!drained() || !is_swim_drain_proc(link->mProcID)) {
        s_drownTimer = 0;
        return;
    }
    if (s_drownTimer == 0) deny();
    if (++s_drownTimer < kDrownGraceFrames) return;
    if (start_drown(link)) s_drowning = true;
}

void update_stamina(const LogService*, ModContext*) {
    stamina_hud_begin_tick();
    s_blockedThisFrame = false;
    s_swungThisFrame = false;
    update_sprint_human();
    update_sprint_swim();
    if (s_suppressSwingCharge > 0) s_suppressSwingCharge--;
    if (s_jumpChargeCd > 0) s_jumpChargeCd--;
    if (s_denyCooldown > 0) s_denyCooldown--;
    if (s_hiddenSkillLock > 0) s_hiddenSkillLock--;
    if (s_otherSpend > 0.0f) {
        s_otherSpend -= kRecentSpendDecay;
        if (s_otherSpend < 0.0f) s_otherSpend = 0.0f;
    }

    if (!g_configStaminaEnabled) {
        s_stamina = stamina_max();
        s_regenDelay = 0;
        s_exhausted = false;
        s_regenRamp = 0.0f;
        stamina_hud_reset(s_stamina);
        s_hiddenSkillLock = 0;
        s_otherSpend = 0.0f;
        s_extraDrain = 0.0f;
        reset_drown();
        return;
    }

    const f32 kMax = stamina_max();
    if (s_stamina > kMax) s_stamina = kMax;

    if (g_configStaminaRefillOnStageChange && dComIfGp_isEnableNextStage()) refill_stamina();

    if (!in_gameplay()) {
        s_extraDrain = 0.0f;
        s_drownTimer = 0;
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());

    if (s_hiddenSkillLock > 0 && link != nullptr && is_hidden_skill_proc_state(link)) {
        s_hiddenSkillLock = kHiddenSkillLockFrames;
    }
    if (link != nullptr && link->mProcID == daAlink_c::PROC_CUT_LARGE_JUMP_CHARGE && s_jumpChargeCd < 2) {
        s_jumpChargeCd = 2;
    }

    const f32 extra = s_extraDrain;
    s_extraDrain = 0.0f;
    f32 rate = (link ? drain_rate(link) + spin_charge_drain(link) : 0.0f) + extra;

    if (rate > 0.0f) {
        s_stamina -= rate;
        if (s_regenDelay < regen_delay_frames()) s_regenDelay = regen_delay_frames();
        s_regenRamp = 0.0f;
        stamina_hud_notify_drain();
        check_exhaust();
    } else {
        if (s_regenDelay > 0) {
            s_regenDelay--;
            s_regenRamp = 0.0f;
        } else {
            s_regenRamp += kRegenRampStep;
            if (s_regenRamp > 1.0f) s_regenRamp = 1.0f;
            f32 pct = static_cast<f32>(g_configStaminaRegen);
            if (pct < 10.0f) pct = 10.0f;
            f32 base = 1.3f;
            if (is_hang_rest_proc(link)) {
                base = g_configStaminaSlowHangRegen ? 1.3f * kHangRestRegenFactor : 0.0f;
            } else if (is_swim_rest_proc(link)) {
                base = g_configStaminaSwimRestRegen ? 1.3f * kHangRestRegenFactor : 0.0f;
            } else if (link != nullptr && !link->checkWolf() && link->checkPlayerGuard()) {
                base = 1.3f * kGuardRegenFactor;
            }
            s_stamina += base * pct / 100.0f * s_regenRamp;
        }
        stamina_hud_idle_tick();
    }
    if (s_stamina < 0.0f) s_stamina = 0.0f;
    if (s_stamina > kMax) s_stamina = kMax;
    update_drown(link);
    f32 recoverFrac = static_cast<f32>(g_configStaminaExhaustRecover) / 100.0f;
    if (recoverFrac < 0.05f) recoverFrac = 0.05f;
    if (recoverFrac > 1.0f) recoverFrac = 1.0f;
    const f32 recoverBase = stamina_hud_radial_style() ? stamina_impl::main_ring_capacity(kMax) : kMax;
    f32 recoverAt = recoverBase * recoverFrac;
    if (recoverAt > kMax) recoverAt = kMax;
    if (s_exhausted && s_stamina >= recoverAt - 0.01f) {
        s_exhausted = false;
        stamina_hud_notify_recover();
    }
    stamina_hud_update(s_stamina, kMax, s_exhausted);
}

template <class Entry>
static void hook_cost(const HookService* h, int cat, int cost_id) {
    HookOptions opt = HOOK_OPTIONS_INIT;
    opt.userdata = reinterpret_cast<void*>(
        static_cast<std::intptr_t>((cat << 16) | (cost_id & 0xFFFF)));
    mods::hook::add_pre<Entry>(h, action_cost_pre, &opt);
}

static void on_stamina_save_activated(ModContext*, uint32_t, void*) {
    refill_stamina();
}

ModResult init_stamina(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_OK;

    s_stamina = stamina_max();

    if (svc_save != nullptr && mod_ctx != nullptr) {
        static SaveObserverHandle s_staminaSaveObserver = 0;
        svc_save->observe_saves(mod_ctx, on_stamina_save_activated, on_stamina_save_activated,
                                nullptr, nullptr, &s_staminaSaveObserver);
    }

    init_sprint_human(hook_svc);
    init_sprint_wolf(hook_svc);
    init_sprint_swim(hook_svc);
    init_sprint_wind(hook_svc);

    init_stamina_hud(hook_svc, s_stamina);

    mods::hook::add_post<StaminaTiredCheck>(hook_svc, tired_check_post);
    mods::hook::add_pre<StamHangEnd>(hook_svc, hang_drop_pre);
    mods::hook::add_pre<StamHangWallCatch>(hook_svc, hang_drop_pre);
    mods::hook::add_pre<StamLadderFall>(hook_svc, hang_drop_pre);
    mods::hook::add_pre<StamSpinCharge>(hook_svc, spin_charge_pre);
    mods::hook::add_pre<StamSpinChargeMove>(hook_svc, spin_charge_pre);

    mods::hook::add_post<StamSwordSwing>(hook_svc, sword_swing_post);

    hook_cost<StamFrontRoll>(hook_svc, STAM_ROLLS, STAMC_ROLL);
    hook_cost<StamSideRoll>(hook_svc, STAM_ROLLS, STAMC_ROLL);
    hook_cost<StamBackJump>(hook_svc, STAM_ROLLS, STAMC_ROLL);
    hook_cost<StamSideStep>(hook_svc, STAM_ROLLS, STAMC_SIDESTEP);

    mods::hook::add_pre<StamCutJump>(hook_svc, jump_attack_pre);
    mods::hook::add_pre<StamCutLargeJump>(hook_svc, jump_attack_pre);
    hook_cost<StamCutSpin>(hook_svc, STAM_JUMPSPIN, STAMC_SPIN);

    mods::hook::add_pre<StamCutFinish>(hook_svc, cut_finish_pre);
    hook_cost<StamCutBackSlice>(hook_svc, STAM_HIDDENSKILLS, STAMC_HIDDENSKILL);
    hook_cost<StamCutDown>(hook_svc, STAM_HIDDENSKILLS, STAMC_HIDDENSKILL);
    hook_cost<StamCutHead>(hook_svc, STAM_HIDDENSKILLS, STAMC_HIDDENSKILL);
    hook_cost<StamGuardAttack>(hook_svc, STAM_HIDDENSKILLS, STAMC_HIDDENSKILL);
    return MOD_OK;
}

void shutdown_stamina() {
    shutdown_sprint_human();
    shutdown_sprint_wolf();
    shutdown_sprint_swim();
    shutdown_sprint_wind();
    s_stamina = stamina_max();
    shutdown_stamina_hud(s_stamina);
    s_regenDelay = 0;
    s_blockedThisFrame = s_swungThisFrame = false;
    s_suppressSwingCharge = 0;
    s_jumpChargeCd = 0;
    s_denyCooldown = 0;
    s_extraDrain = 0.0f;
    s_exhausted = false;
    s_regenRamp = 0.0f;
    s_hiddenSkillLock = 0;
    s_otherSpend = 0.0f;
    reset_drown();
}
