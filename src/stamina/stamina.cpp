#include "stamina.hpp"
#include "stamina_internal.hpp"
#include "sprint_human.hpp"
#include "sprint_wolf.hpp"
#include "sprint_swim.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "d/d_msg_object.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_controller_pad.h"
#include "Z2AudioLib/Z2SeMgr.h"

#define private public
#define protected public
#include "d/d_meter2_draw.h"
#undef private
#undef protected

#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"

#include <cstdint>

bool g_configStaminaEnabled = false;
int  g_configStaminaMax     = 100;
int  g_configStaminaRegen   = 100;

bool g_configStaminaSrcAttacks  = true;
bool g_configStaminaSrcJumpSpin  = true;
bool g_configStaminaSrcRolls     = true;
bool g_configStaminaSrcClimb     = true;
bool g_configStaminaSrcHang      = true;
bool g_configStaminaSrcSwim      = true;
bool g_configStaminaSrcPushPull  = true;
bool g_configStaminaSrcWolfDash  = true;
bool g_configStaminaSrcHiddenSkills = true;

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

DEFINE_HOOK(&dMeter2Draw_c::draw, StaminaMeterDrawHook);

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

static f32 s_stamina    = 100.0f;
static f32 s_display    = 100.0f;
static int s_regenDelay = 0;
static int s_showTimer  = 0;
static f32 s_alpha      = 0.0f;
static f32 s_pulse      = 0.0f;
static f32 s_emptyFlash = 0.0f;
static f32 s_stackShift = 0.0f;
static bool s_blockedThisFrame = false;
static bool s_swungThisFrame = false;
static int  s_suppressSwingCharge = 0;
static int  s_jumpChargeCd = 0;
static f32  s_extraDrain  = 0.0f;

static f32 stamina_max() {
    f32 m = static_cast<f32>(g_configStaminaMax);
    return m < 10.0f ? 10.0f : m;
}

static bool in_gameplay() {
    if (dMeter2Info_getWindowStatus() != 0) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    if (dComIfGp_event_runCheck()) return false;
    if (dMeter2Info_isShopTalkFlag() || dMsgObject_isTalkNowCheck()) return false;
    return true;
}

static f32 drain_rate(u16 proc) {
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
        return g_configStaminaSrcSwim
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

static bool empty() { return s_stamina <= 0.5f; }

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

static int s_denyCooldown = 0;

static void deny() {
    s_showTimer = 50;
    s_emptyFlash = 1.0f;
    if (!s_blockedThisFrame && s_denyCooldown <= 0) {
        s_blockedThisFrame = true;
        s_denyCooldown = 24;
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

// A hidden skill move is made up of rolls, jumps and swings that each carry
// their own charge. To keep the whole move at its single hidden-skill price,
// recently spent stamina is refunded when the move starts and all other
// charges are suppressed while the move runs. The lock is short - it only
// bridges transitions - and stays alive through the hidden skill procs.
static f32 s_otherSpend = 0.0f;

static int  s_hiddenSkillLock = 0;

static constexpr int kHiddenSkillLockFrames = 20;
static constexpr f32 kRecentSpendDecay       = 0.25f;

static void spend_raw(f32 cost) {
    s_stamina -= cost;
    if (s_stamina < 0.0f) s_stamina = 0.0f;
    s_regenDelay = 50;
    s_showTimer = 50;
    s_pulse = 1.0f;
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
        // A proc init marks the start of a new move, so it always charges - the
        // lock must never suppress it (spamming re-enters the same proc while
        // the previous move's lock is still alive).
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

// daAlink_CutFinishParamType from d_a_alink_cut.inc; Mortal Draw is the
// procCutFinishInit variant of the hidden skills.
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

static HookAction sword_swing_pre(ModContext*, void*, void* retval, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaSrcAttacks) return HOOK_CONTINUE;
    if (!in_gameplay() || s_hiddenSkillLock > 0 || !empty()) return HOOK_CONTINUE;
    if (retval) *static_cast<int*>(retval) = 0;
    deny();
    return HOOK_SKIP_ORIGINAL;
}

static void sword_swing_post(ModContext*, void*, void* retval, void*) {
    if (!g_configStaminaEnabled || !in_gameplay()) return;
    if (!g_configStaminaSrcAttacks || s_hiddenSkillLock > 0) return;
    if (s_suppressSwingCharge > 0) { s_swungThisFrame = true; return; }
    if (s_swungThisFrame) return;
    if (retval && *static_cast<int*>(retval) != 0) {
        s_swungThisFrame = true;
        spend(stamina_impl::cost_scaled(kSwingCost, g_configStaminaCostAttack));
    }
}

static HookAction jump_attack_pre(ModContext*, void*, void* retval, void*) {
    if (!g_configStaminaEnabled || !g_configStaminaSrcJumpSpin) return HOOK_CONTINUE;
    if (!in_gameplay() || s_hiddenSkillLock > 0) return HOOK_CONTINUE;
    if (empty()) {
        if (retval) *static_cast<int*>(retval) = 0;
        deny();
        return HOOK_SKIP_ORIGINAL;
    }
    if (s_jumpChargeCd <= 0) {
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

void update_stamina(const LogService*, ModContext*) {
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
        s_stamina = s_display = stamina_max();
        s_regenDelay = s_showTimer = 0;
        s_alpha = s_pulse = s_emptyFlash = 0.0f;
        s_hiddenSkillLock = 0;
        s_otherSpend = 0.0f;
        s_extraDrain = 0.0f;
        return;
    }

    const f32 kMax = stamina_max();
    if (s_stamina > kMax) s_stamina = kMax;

    if (!in_gameplay()) {
        s_extraDrain = 0.0f;
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());

    // Keep the charge lock alive while a hidden skill move is still running,
    // so its follow-up swings and landings stay free.
    if (s_hiddenSkillLock > 0 && link != nullptr && is_hidden_skill_proc_state(link)) {
        s_hiddenSkillLock = kHiddenSkillLockFrames;
    }

    const f32 extra = s_extraDrain;
    s_extraDrain = 0.0f;
    f32 rate = (link ? drain_rate(link->mProcID) : 0.0f) + extra;

    if (rate > 0.0f) {
        s_stamina -= rate;
        s_regenDelay = 45;
        s_showTimer = 45;
        s_pulse = 1.0f;
    } else {
        if (s_regenDelay > 0) {
            s_regenDelay--;
        } else {
            f32 pct = static_cast<f32>(g_configStaminaRegen);
            if (pct < 10.0f) pct = 10.0f;
            s_stamina += 1.3f * pct / 100.0f;
        }
        if (s_showTimer > 0) s_showTimer--;
    }
    if (s_stamina < 0.0f) s_stamina = 0.0f;
    if (s_stamina > kMax) s_stamina = kMax;

    s_display += (s_stamina - s_display) * 0.28f;
    if (s_display < 0.0f) s_display = 0.0f;
    if (s_display > kMax) s_display = kMax;
    s_pulse *= 0.82f;       if (s_pulse < 0.003f)      s_pulse = 0.0f;
    s_emptyFlash *= 0.90f;  if (s_emptyFlash < 0.003f) s_emptyFlash = 0.0f;

    const bool visible = (s_showTimer > 0) || (s_stamina < kMax - 0.5f);
    const f32 target = visible ? 1.0f : 0.0f;
    s_alpha += (target - s_alpha) * (target > s_alpha ? 0.22f : 0.12f);
    if (s_alpha < 0.001f) s_alpha = 0.0f;
    if (s_alpha > 1.0f) s_alpha = 1.0f;
}

static JUtility::TColor lerp(JUtility::TColor a, JUtility::TColor b, f32 t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return JUtility::TColor(
        static_cast<u8>(a.r + (b.r - a.r) * t),
        static_cast<u8>(a.g + (b.g - a.g) * t),
        static_cast<u8>(a.b + (b.b - a.b) * t),
        static_cast<u8>(a.a + (b.a - a.a) * t));
}

static void on_stamina_meter_draw_post(ModContext*, void* args, void*, void*) {
    if (!g_configStaminaEnabled || s_alpha < 0.01f || !args) return;
    if (!in_gameplay()) return;

    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (!draw || !draw->mpKanteraScreen) return;

    CPaneMgr* meter  = draw->mpMagicMeter;
    CPaneMgr* base   = draw->mpMagicBase;
    CPaneMgr* frameL = draw->mpMagicFrameL;
    CPaneMgr* frameR = draw->mpMagicFrameR;
    CPaneMgr* parent = draw->mpMagicParent;
    if (!meter || !base || !frameL || !frameR || !parent) return;

    f32 a = s_alpha;
    if (a > 1.0f) a = 1.0f;

    const f32 fill01 = s_display / stamina_max();
    const f32 span = frameR->getInitPosX() - frameL->getInitPosX();

    JUtility::TColor hi(170, 255, 150, 255);
    JUtility::TColor lo(28, 158, 54, 255);
    hi = lerp(hi, JUtility::TColor(224, 255, 214, 255), s_pulse * 0.6f);
    lo = lerp(lo, JUtility::TColor(120, 224, 128, 255), s_pulse * 0.6f);
    hi = lerp(hi, JUtility::TColor(255, 170, 120, 255), s_emptyFlash);
    lo = lerp(lo, JUtility::TColor(206, 40, 30, 255), s_emptyFlash);

    meter->setBlackWhite(hi, lo);
    meter->resize(fill01 * meter->getInitSizeX(), meter->getInitSizeY());
    frameR->move(span + frameL->getInitPosX(), frameL->getInitPosY());
    base->resize(base->getInitSizeX(), base->getInitSizeY());

    const f32 stackTarget = (draw->getMeterGaugeAlphaRate(1) > 0.02f) ? 16.0f : 0.0f;
    s_stackShift += (stackTarget - s_stackShift) * 0.15f;

    parent->setAlphaRate(a);
    meter->setAlphaRate(a * g_drawHIO.mLanternMeterAlpha);
    frameL->setAlphaRate(a * g_drawHIO.mLanternMeterFrameAlpha);
    frameR->setAlphaRate(a * g_drawHIO.mLanternMeterFrameAlpha);

    const f32 origTX = parent->getTranslateX();
    const f32 origTY = parent->getTranslateY();
    parent->translate(origTX, origTY + s_stackShift);

    J2DGrafContext* graf = dComIfGp_getCurrentGrafPort();
    if (graf) graf->setup2D();
    draw->mpKanteraScreen->draw(0.0f, 0.0f, graf);

    parent->translate(origTX, origTY);
}

template <class Entry>
static void hook_cost(const HookService* h, int cat, int cost_id) {
    HookOptions opt = HOOK_OPTIONS_INIT;
    opt.userdata = reinterpret_cast<void*>(
        static_cast<std::intptr_t>((cat << 16) | (cost_id & 0xFFFF)));
    mods::hook::add_pre<Entry>(h, action_cost_pre, &opt);
}

ModResult init_stamina(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_OK;

    init_sprint_human(hook_svc);
    init_sprint_wolf(hook_svc);
    init_sprint_swim(hook_svc);

    mods::hook::add_post<StaminaMeterDrawHook>(hook_svc, on_stamina_meter_draw_post);

    mods::hook::add_post<StaminaTiredCheck>(hook_svc, tired_check_post);

    mods::hook::add_pre<StamSwordSwing>(hook_svc, sword_swing_pre);
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
    s_stamina = s_display = stamina_max();
    s_regenDelay = s_showTimer = 0;
    s_alpha = s_pulse = s_emptyFlash = s_stackShift = 0.0f;
    s_blockedThisFrame = s_swungThisFrame = false;
    s_suppressSwingCharge = 0;
    s_jumpChargeCd = 0;
    s_denyCooldown = 0;
    s_extraDrain = 0.0f;
    s_hiddenSkillLock = 0;
    s_otherSpend = 0.0f;
}
