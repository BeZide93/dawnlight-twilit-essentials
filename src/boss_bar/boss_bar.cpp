#include "boss_bar.hpp"
#include "boss_internals.hpp"
#include "../stamina/stamina.hpp"
#include "../compat/twilight_hd.hpp"

void qa_hud_scale_begin(f32 anchorX, f32 anchorY);
void qa_hud_scale_end();

#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/config.h"
#include "mods/svc/resource.h"

#define private public
#define protected public
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#undef private
#undef protected
#include "d/d_pane_class.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_s_play.h"
#include "d/d_attention.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_ext.h"

#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "JSystem/JKernel/JKRExpHeap.h"

#include "../boss_rush/boss_rush.hpp"

bool g_configBossBarEnabled = false;

float g_configBossBarX = 0.0f;
float g_configBossBarY = 0.0f;

static constexpr int kBossBarPreviewFrames = 120;
static int s_bossBarPreviewFrames = 0;

void boss_bar_preview_request() { s_bossBarPreviewFrames = kBossBarPreviewFrames; }
void boss_bar_preview_cancel() { s_bossBarPreviewFrames = 0; }

ConfigVarHandle g_bossBarVars[2] = {};
ConfigVarHandle g_bossBarStyleVar = 0;
int g_configBossBarStyle = 0;

enum BossBarStyle {
    kBossBarStyleDefault = 0,
    kBossBarStyleEldenRing,
};

const char* const kBossBarStyleLabels[] = {"Default", "Elden Ring"};
const size_t kBossBarStyleCount = sizeof(kBossBarStyleLabels) / sizeof(kBossBarStyleLabels[0]);

static void on_boss_bar_style_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                      const ConfigVarValue*, void*) {
    if (value == nullptr) return;
    g_configBossBarStyle = static_cast<int>(value->int_value);
    boss_bar_preview_request();
    stamina_bar_preview_cancel();
}

static void on_boss_bar_pos_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                    const ConfigVarValue*, void* user_data) {
    if (value == nullptr) return;
    const f32 v = static_cast<f32>(value->int_value);
    if (user_data != nullptr) {
        g_configBossBarX = v;
    } else {
        g_configBossBarY = v;
    }
    boss_bar_preview_request();
    stamina_bar_preview_cancel();
}

ModResult init_boss_bar_config(const ConfigService* cfg, ModContext* ctx) {
    if (cfg == nullptr) return MOD_OK;

    const struct { const char* name; bool isX; } vars[] = {
        { "bossBarX", true },
        { "bossBarY", false },
    };
    for (int i = 0; i < 2; i++) {
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = vars[i].name;
        d.type = CONFIG_VAR_INT;
        d.default_int = 0;
        if (cfg->register_var(ctx, &d, &g_bossBarVars[i]) == MOD_OK) {
            int64_t val = 0;
            cfg->get_int(ctx, g_bossBarVars[i], &val);
            if (vars[i].isX) {
                g_configBossBarX = static_cast<f32>(val);
            } else {
                g_configBossBarY = static_cast<f32>(val);
            }
            cfg->subscribe(ctx, g_bossBarVars[i], on_boss_bar_pos_changed,
                           reinterpret_cast<void*>(static_cast<intptr_t>(vars[i].isX)), nullptr);
        }
    }

    ConfigVarDesc styleDesc = CONFIG_VAR_DESC_INIT;
    styleDesc.name = "bossBarStyle";
    styleDesc.type = CONFIG_VAR_INT;
    styleDesc.default_int = kBossBarStyleDefault;
    if (cfg->register_var(ctx, &styleDesc, &g_bossBarStyleVar) == MOD_OK) {
        int64_t style = 0;
        cfg->get_int(ctx, g_bossBarStyleVar, &style);
        g_configBossBarStyle = static_cast<int>(style);
        cfg->subscribe(ctx, g_bossBarStyleVar, on_boss_bar_style_changed, nullptr, nullptr);
    }
    return MOD_OK;
}

constexpr int USE_STATIC_COLOR = 1;

DEFINE_HOOK(&dMeter2Draw_c::draw, BossBarMeter2DrawHook);

typedef f32 (*BossHpRatioFn)(fopAc_ac_c* actor);
typedef bool (*BossEngagedFn)(fopAc_ac_c* actor);

struct BossDef {
    s16 name;
    const char* label;
    bool miniboss;
    bool ownBar;
    BossHpRatioFn hpRatio;
    bool aggregate;
    BossEngagedFn engagedHint;
    BossEngagedFn ignoreFn;
};

static int s_gmDoneFrames = 0;
static bool armogohma_ignore(fopAc_ac_c* a) {
    int hits = 0, act = 0, demo = 0;
    bbi::armogohma_dbg(a, hits, act, demo);
    if (hits < 3) { s_gmDoneFrames = 0; return false; }
    return (++s_gmDoneFrames > 45);
}

static bool ook_engaged_hint(fopAc_ac_c*) { return true; }

static const BossDef kBossTable[] = {
    { fpcNm_B_BQ_e,     "Diababa",          false, true,  nullptr,                true  },
    { fpcNm_B_BH_e,     "Diababa",          false, false, nullptr,                false },
    { fpcNm_E_MB_e,     "Diababa",          false, false, nullptr,                false },
    { fpcNm_E_FM_e,     "Fyrus",            false, true,  bbi::fyrus_ratio,       false, bbi::fyrus_engaged, bbi::fyrus_defeated },
    { fpcNm_B_OB_e,     "Morpheel",         false, true,  bbi::morpheel_ratio },
    { fpcNm_B_OH_e,     "Morpheel",         false, false, nullptr },
    { fpcNm_B_OH2_e,    "Morpheel",         false, false, nullptr },
    { fpcNm_B_DS_e,     "Stallord",         false, true,  bbi::stallord_ratio,    false, bbi::stallord_engaged },
    { fpcNm_B_YO_e,     "Blizzeta",         false, true,  bbi::blizzeta_ratio },
    { fpcNm_B_GM_e,     "Armogohma",        false, true,  bbi::armogohma_ratio,   false, bbi::armogohma_engaged, armogohma_ignore },
    { fpcNm_B_DR_e,     "Argorok",          false, true,  bbi::argorok_ratio },
    { fpcNm_B_DRE_e,    "Argorok",          false, false, nullptr },
    { fpcNm_B_MGN_e,    "Dark Beast Ganon", false, true,  bbi::beastganon_ratio,  false, bbi::beastganon_engaged, bbi::beastganon_defeated },
    { fpcNm_B_GND_e,    "Ganondorf",        false, true,  bbi::ganondorf_ratio },
    { fpcNm_E_HZELDA_e, "Puppet Zelda",     false, true,  nullptr },
    { fpcNm_B_ZANT_e,   "Zant",             false, true,  bbi::zant_ratio },
    { fpcNm_B_ZANTZ_e,  "Zant",             false, false, nullptr },
    { fpcNm_B_ZANTM_e,  "Zant",             false, false, nullptr },
    { fpcNm_B_ZANTS_e,  "Zant",             false, false, nullptr },
    { fpcNm_B_TN_e,     "Darknut",          true,  true,  bbi::darknut_ratio,     false, nullptr, nullptr },
    { fpcNm_B_GG_e,     "Aeralfos",         true,  true,  bbi::aeralfos_ratio },
    { fpcNm_E_VT_e,     "Death Sword",      true,  true,  bbi::deathsword_ratio,  false, bbi::deathsword_engaged, nullptr },
    { fpcNm_E_RDB_e,    "King Bulblin",     true,  true,  nullptr },
    { fpcNm_E_TH_e,     "Darkhammer",       true,  true,  nullptr },
    { fpcNm_E_MK_e,     "Ook",              true,  true,  nullptr,                false, ook_engaged_hint, nullptr },
    { fpcNm_E_DT_e,     "Deku Toad",        true,  true,  nullptr },
    { fpcNm_E_GOB_e,    "Dangoro",          true,  true,  bbi::dangoro_ratio,     false, nullptr, bbi::dangoro_defeated },
    { fpcNm_E_PZ_e,     "Phantom Zant",     true,  true,  nullptr,                false, bbi::phantomzant_engaged, bbi::phantomzant_ignore },
};

static const BossDef* classifyBoss(s16 name) {
    for (const auto& def : kBossTable) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

bool boss_bar_is_boss_name(int16_t name) {
    return classifyBoss(static_cast<s16>(name)) != nullptr;
}

struct BossState {
    bool valid;
    bool engaged;
    bool gone;
    bool custom;
    bool sawHealth;
    fpc_ProcID id;
    s16 name;
    const char* label;
    bool miniboss;
    s16 maxHp;
    s16 curHp;
    s16 lastHealth;
    int lockFrames;
    f32 live01;
    f32 shownRatio;
    f32 displayRatio;
    f32 alpha;
    int deadTimer;
    int missingFrames;
    bool aggHide;
};

static BossState s_boss = {};
static std::unordered_set<uint32_t> s_defeated;

static bool s_justDefeated = false;

static const char* s_recentEngagedLabel = nullptr;
static int s_recentEngagedFrames = 0;

static char s_dbgExtra[80] = {};

bool boss_bar_consume_defeat_event() {
    bool r = s_justDefeated;
    s_justDefeated = false;
    return r;
}

void boss_bar_force_defeat_event() {
    s_justDefeated = true;
}

void boss_bar_rearm_defeat(unsigned int actorId) {
    s_defeated.erase(actorId);
}

bool boss_bar_debug_snapshot(char* buf, size_t bufSize) {
    if (buf == nullptr || bufSize == 0 || !s_boss.valid) {
        return false;
    }
    std::snprintf(buf, bufSize, "%s: engaged=%d alpha=%.2f live=%.2f lock=%d gone=%d hp=%d/%d%s",
                  s_boss.label ? s_boss.label : "?", static_cast<int>(s_boss.engaged),
                  s_boss.alpha, s_boss.live01, s_boss.lockFrames, static_cast<int>(s_boss.gone),
                  static_cast<int>(s_boss.curHp), static_cast<int>(s_boss.maxHp), s_dbgExtra);
    return true;
}

bool boss_bar_current_fight_state(const char** outLabel, bool& outEngaged) {
    if (!s_boss.valid) {
        return false;
    }
    if (outLabel != nullptr) {
        *outLabel = s_boss.label ? s_boss.label : "?";
    }
    outEngaged = s_boss.engaged;
    return true;
}

bool boss_bar_boss_defeated_now() {
    return s_boss.valid && s_boss.engaged && (s_boss.deadTimer > 3 || s_boss.gone);
}

bool boss_bar_hidden_for_transition() {
    return s_boss.valid && s_boss.aggHide && s_boss.alpha < 0.15f;
}

struct Candidate {
    fpc_ProcID id;
    s16 name;
    s16 health;
    bool hasRatio;
    f32 ratio;
    fopAc_ac_c* actor;
    const BossDef* def;
};

static std::vector<Candidate> s_cands;

static int s_diaDbgPhase = 0, s_diaDbgHeads = 0, s_diaDbgRounds = 0;
static bool s_diaSawIntro = false;
static int  s_diaIntroFrames = 0;
static int  s_diaReadyFrames = 0;
static bool s_diaSawHeads = false;
static int  s_diaAllDeadFrames = 0;
static int  s_diaCutFrames = 0;
static int  s_diaP2Rounds = 0;
static bool s_diaP2Started = false;
static s16  s_diaBqLastHp = 0;

static void diababa_reset() {
    s_diaSawIntro = false;
    s_diaIntroFrames = 0;
    s_diaReadyFrames = 0;
    s_diaSawHeads = false;
    s_diaAllDeadFrames = 0;
    s_diaCutFrames = 0;
    s_diaP2Rounds = 0;
    s_diaP2Started = false;
    s_diaBqLastHp = 0;
    s_diaDbgPhase = s_diaDbgHeads = s_diaDbgRounds = 0;
}

static f32 diababa_live01(bool& engageOk, bool& hideNow) {
    fopAc_ac_c* bq = nullptr;
    int headCount = 0, headsAlive = 0;
    for (const auto& c : s_cands) {
        if (!c.actor) continue;
        if (c.name == fpcNm_B_BQ_e) {
            bq = c.actor;
        } else if (c.name == fpcNm_B_BH_e) {
            headCount++;
            if (c.actor->health > 0) headsAlive++;
        }
    }

    const bool headRisen = bq && bbi::diababa_head_risen(bq);

    const bool cutscene = dComIfGp_event_runCheck() != 0;
    if (cutscene && ++s_diaIntroFrames > 20) s_diaSawIntro = true;
    if (boss_rush_is_fighting_here()) s_diaSawIntro = true;

    if (s_diaSawIntro && !cutscene && headsAlive > 0) {
        if (++s_diaReadyFrames > 10) s_diaSawHeads = true;
    } else if (!s_diaSawHeads) {
        s_diaReadyFrames = 0;
    }

    if (!s_diaSawHeads) {
        engageOk = false;
        hideNow = true;
        return 1.0f;
    }

    engageOk = true;

    if (cutscene) s_diaCutFrames++;
    else s_diaCutFrames = 0;

    if (headsAlive == 0) s_diaAllDeadFrames++;
    else s_diaAllDeadFrames = 0;

    if (!s_diaP2Started && (headRisen || (s_diaAllDeadFrames > 90 && !cutscene))) {
        s_diaP2Started = true;
        s_diaP2Rounds = 0;
        s_diaBqLastHp = bq ? bq->health : 0;
    }

    if (!s_diaP2Started) {
        s_diaDbgPhase = 1;
        s_diaDbgHeads = headsAlive;

        if (headsAlive > 0) {
            hideNow = false;
            f32 v = static_cast<f32>(headsAlive) / 2.0f;
            return v > 1.0f ? 1.0f : v;
        }
        hideNow = true;
        return 0.05f;
    }

    const f32 kP2Cycles = 2.0f;
    s16 bqHp = bq ? bq->health : 0;
    hideNow = (s_diaCutFrames > 12);
    const bool roundEnded =
        (s_diaBqLastHp > 0 && bqHp <= 0) ||
        (s_diaBqLastHp > 0 && s_diaBqLastHp < 45 && bqHp >= 45);
    if (roundEnded && s_diaP2Rounds < 2) s_diaP2Rounds++;
    s_diaBqLastHp = bqHp;

    f32 hf = (bqHp <= 0) ? 0.0f : (bqHp >= 50 ? 1.0f : static_cast<f32>(bqHp) / 50.0f);
    f32 partial = (bqHp > 0) ? (1.0f - hf) : 0.0f;
    f32 used = (static_cast<f32>(s_diaP2Rounds) + partial) / kP2Cycles;
    s_diaDbgPhase = 2;
    s_diaDbgRounds = s_diaP2Rounds;
    s_diaDbgHeads = bqHp;
    f32 v = 1.0f - used;
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static int s_obPhase = 0;
static int s_obEmptyFrames = 0;
static bool s_obSawHp = false;
static int s_obDbgAct = 0, s_obDbgFish = 0, s_obDbgFin = 0, s_obDbgHit = 0, s_obDbgDemo = 0, s_obDbgHp = 0;
static void morpheel_reset() { s_obPhase = 0; s_obEmptyFrames = 0; s_obSawHp = false;
    s_obDbgAct = s_obDbgFish = s_obDbgFin = s_obDbgHit = s_obDbgDemo = s_obDbgHp = 0; }

static f32 morpheel_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int act, fish, fin, hit, demo, hp, coreMode;
    bbi::morpheel_dbg(actor, act, fish, fin, hit, demo, hp, coreMode);
    s_obDbgAct = act; s_obDbgFish = fish; s_obDbgFin = fin; s_obDbgHit = hit;
    s_obDbgDemo = demo; s_obDbgHp = hp;

    const bool fishPhase = (act >= 100) || (fish != 0);
    if (!fishPhase && hp >= 20) s_obSawHp = true;

    if (s_obPhase == 0) {
        if (fishPhase && (fin > 0 || hit > 0 || demo == 0)) {
            s_obPhase = 3;
        } else if (!fishPhase && demo == 0 && act >= 1 && act <= 5) {
            s_obPhase = 1;
        } else {
            hideNow = true;
            return 1.0f;
        }
    }

    if (s_obPhase == 1) {
        engageOk = true;
        if (fishPhase || act == 5) { s_obPhase = 2; s_obEmptyFrames = 0; }
        else if (s_obSawHp && hp <= 0) {
            if (++s_obEmptyFrames > 60) { s_obPhase = 2; s_obEmptyFrames = 0; }
                                                                                            return 0.0f;
        } else {
            s_obEmptyFrames = 0;
            f32 v = (s_obSawHp && hp >= 0) ? static_cast<f32>(hp) / 30.0f : 1.0f;
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
    }

    if (s_obPhase == 2) {
        engageOk = true;
        hideNow  = true;
        if (fishPhase && demo == 0) s_obPhase = 3;
        return 1.0f;
    }

    if (s_obPhase == 3) {
        engageOk = true;
        hideNow  = (demo != 0);
        int effHit = (hit >= 4) ? 0 : (hit < 0 ? 0 : hit);
        int effFin = fin < 0 ? 0 : (fin > 3 ? 3 : fin);
        f32 used = static_cast<f32>(effFin * 4 + effHit) / 12.0f;
        f32 v = 1.0f - used;
        if (v <= 0.0f) { s_obPhase = 4; return 0.0f; }
        return v > 1.0f ? 1.0f : v;
    }

    return 0.0f;
}

static int s_dsPhase = 0;
static int s_dsEmptyFrames = 0;
static bool s_dsSawDemo = false;
static int s_dsDamageBone = -1;
static int s_dsDbgPhase = 0, s_dsDbgBone = 0, s_dsDbgHp = 0, s_dsDbgAct = 0;
static void stallord_reset() { s_dsPhase = 0; s_dsEmptyFrames = 0; s_dsSawDemo = false;
    s_dsDamageBone = -1;
    s_dsDbgPhase = s_dsDbgBone = s_dsDbgHp = s_dsDbgAct = 0; }

static f32 stallord_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int phase, bone, hp, act;
    bool dead, demo;
    bbi::stallord_read(actor, phase, bone, hp, act, dead, demo);
    s_dsDbgPhase = phase; s_dsDbgBone = bone; s_dsDbgHp = hp; s_dsDbgAct = act;
    if (bone < 0) bone = 0; if (bone > 3) bone = 3;

    const bool cutscene = demo || dComIfGp_event_runCheck() != 0;
    if (cutscene) s_dsSawDemo = true;

    if (s_dsPhase == 0) {
        if (phase != 0) {
            s_dsPhase = 3;
        } else if (!cutscene && (s_dsSawDemo || bone > 0 || bbi::stallord_engaged(actor))) {
            s_dsPhase = 1;
        } else {
            hideNow = true;
            return 1.0f;
        }
    }

    if (s_dsPhase == 1) {
        engageOk = true;
        if (phase == 0 && act == 3) {
            if (s_dsDamageBone < 0) s_dsDamageBone = bone;
        } else {
            s_dsDamageBone = -1;
        }
        int effLvl = bone;
        if (s_dsDamageBone >= 0 && bone <= s_dsDamageBone) effLvl = bone + 1;
        if (effLvl > 3) effLvl = 3;
        if (phase != 0 || effLvl >= 3) {
            if (effLvl >= 3 && phase == 0) {
                if (++s_dsEmptyFrames > 60) { s_dsPhase = 2; s_dsEmptyFrames = 0; }
                return 0.0f;
            }
            s_dsPhase = 2; s_dsEmptyFrames = 0;
        } else {
            return 1.0f - static_cast<f32>(effLvl) / 3.0f;
        }
    }

    if (s_dsPhase == 2) {
        engageOk = true;
        hideNow  = true;
        if (phase != 0 && !cutscene) s_dsPhase = 3;
        return 1.0f;
    }

    if (s_dsPhase == 3) {
        engageOk = true;
        hideNow  = cutscene;
        if (dead || hp <= 0) { s_dsPhase = 4; return 0.0f; }
        f32 v = static_cast<f32>(hp) / 1080.0f;
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    return 0.0f;
}

static int s_yoPhase = 0;
static int s_yoEmptyFrames = 0;
static int s_yoDbgModel = 0, s_yoDbgLpd = 0, s_yoDbgAct = 0;
static void blizzeta_reset() { s_yoPhase = 0; s_yoEmptyFrames = 0;
    s_yoDbgModel = s_yoDbgLpd = s_yoDbgAct = 0; }

static f32 blizzeta_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int model, lpd, act;
    bbi::blizzeta_read(actor, model, lpd, act);
    s_yoDbgModel = model; s_yoDbgLpd = lpd; s_yoDbgAct = act;
    if (model < 0) model = 0;
    if (lpd < 0) lpd = 0; if (lpd > 3) lpd = 3;

    const bool cutscene   = dComIfGp_event_runCheck() != 0;
    const bool fishPhase2 = (model >= 7);
    const bool transDemo  = (act == 2 || act == 7);

    if (s_yoPhase == 0) {
        if (fishPhase2) { s_yoPhase = 3; }
        else if (!cutscene && act != 0 && !transDemo) {
            s_yoPhase = 1;
        } else { hideNow = true; return 1.0f; }
    }

    if (s_yoPhase == 1) {
        engageOk = true;
        if (fishPhase2) { s_yoPhase = 2; s_yoEmptyFrames = 0; }
        else if (transDemo) {
            if (++s_yoEmptyFrames > 40) { s_yoPhase = 2; s_yoEmptyFrames = 0; }
            return 0.0f;
        } else {
            s_yoEmptyFrames = 0;
            f32 v = 1.0f - static_cast<f32>(model) / 7.0f;
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
    }

    if (s_yoPhase == 2) {
        engageOk = true;
        hideNow  = true;
        if (fishPhase2 && !transDemo && !cutscene) s_yoPhase = 3;
        return 1.0f;
    }

    if (s_yoPhase == 3) {
        engageOk = true;
        hideNow  = cutscene;
        if (act == 9 || lpd >= 3) { s_yoPhase = 4; return 0.0f; }
        return 1.0f - static_cast<f32>(lpd) / 3.0f;
    }

    return 0.0f;
}

static bool darknut_los_hint(fopAc_ac_c* a) {
    if (a == nullptr) return false;
    camera_process_class* cam = dComIfGp_getCamera(0);
    if (cam == nullptr) return false;

    const cXyz eye(cam->view.lookat.eye.x, cam->view.lookat.eye.y, cam->view.lookat.eye.z);
    const cXyz chest(a->current.pos.x, a->current.pos.y + 100.0f, a->current.pos.z);

    dBgS_LinkLinChk lin;
    lin.Set(&eye, &chest, nullptr);
    return !g_dComIfG_gameInfo.play.mBgs.LineCross(&lin);
}
static int s_tnPhase = 0;
static int s_tnRefill = 0;
static int s_tnPeakCur = 0;
static int s_tnDbgMode = 0, s_tnDbgBrk = 0, s_tnDbgCur = 0, s_tnDbgMax = 0;
static void darknut_reset() { s_tnPhase = 0; s_tnRefill = 0; s_tnPeakCur = 0;
    s_tnDbgMode = s_tnDbgBrk = s_tnDbgCur = s_tnDbgMax = 0; }

static f32 darknut_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int mode, brk, cur, mx;
    bbi::darknut_dbg(actor, mode, brk, cur, mx);
    s_tnDbgMode = mode; s_tnDbgBrk = brk; s_tnDbgCur = cur; s_tnDbgMax = mx;
    if (mx <= 0) mx = 360;

    const bool cutscene = dComIfGp_event_runCheck() != 0;
    constexpr int kDnRefill = 90;

    if (s_tnPhase == 0) {
        if (mode >= 9 && mode <= 13) { s_tnPhase = 3; }
        else if (mode == 8) { s_tnPhase = 2; s_tnRefill = 0; }
        else if (mode >= 2 && mode <= 7 && !cutscene) { s_tnPhase = 1; }
        else { hideNow = true; return 1.0f; }
    }

    if (s_tnPhase == 1) {
        engageOk = true;
        hideNow = !darknut_los_hint(actor);
        if (mode == 8 || brk >= 12 || (mode >= 9 && mode <= 13)) {
            s_tnPhase = 2; s_tnRefill = 0;
        } else {
            f32 v = 1.0f - static_cast<f32>(brk) / 12.0f;
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
    }

    if (s_tnPhase == 2) {
        engageOk = true;
        hideNow = !darknut_los_hint(actor);
        if (s_tnRefill < kDnRefill) s_tnRefill++;
        if (s_tnRefill >= kDnRefill && mode >= 9) { s_tnPhase = 3; return 1.0f; }
        f32 v = static_cast<f32>(s_tnRefill) / static_cast<f32>(kDnRefill);
        return v > 1.0f ? 1.0f : v;
    }

    if (s_tnPhase == 3) {
        engageOk = true;
        hideNow = cutscene || !darknut_los_hint(actor);
        if (mode == 14 || cur >= mx) { s_tnPhase = 4; return 0.0f; }
        if (cur > s_tnPeakCur) s_tnPeakCur = cur;
        const int kFinish = 60;
        f32 v;
        if (s_tnPeakCur <= mx - kFinish) {
            f32 f = static_cast<f32>(s_tnPeakCur) / static_cast<f32>(mx - kFinish);
            v = 1.0f - f * (2.0f / 3.0f);
        } else {
            f32 f = static_cast<f32>(s_tnPeakCur - (mx - kFinish)) / static_cast<f32>(kFinish);
            v = (1.0f / 3.0f) * (1.0f - f);
        }
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    return 0.0f;
}

static int s_drPhase = 0;
static int s_drEmptyFrames = 0;
static int s_drCycle = 0;
static bool s_drInWeek = false;
static int s_drDbgMode = 0, s_drDbgP = 0, s_drDbgParts = 0, s_drDbgHp = 0, s_drDbgWk = 0, s_drDbgA0 = 0, s_drDbgAnm = 0;
static void argorok_reset() {
    s_drPhase = 0; s_drEmptyFrames = 0; s_drCycle = 0; s_drInWeek = false;
    s_drDbgMode = s_drDbgP = s_drDbgParts = s_drDbgHp = s_drDbgWk = s_drDbgA0 = s_drDbgAnm = 0;
}

static f32 argorok_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int mode, ph, parts, hp, wk, arg0, anm;
    bbi::argorok_read(actor, mode, ph, parts, hp, wk, arg0, anm);
    s_drDbgMode = mode; s_drDbgP = ph; s_drDbgParts = parts; s_drDbgHp = hp;
    s_drDbgWk = wk; s_drDbgA0 = arg0; s_drDbgAnm = anm;

    const bool cutscene = dComIfGp_event_runCheck() != 0 || arg0 > 1;
    const bool transDemo = (mode == 12);

    if (ph == 2) {
        if (mode == 3 && !s_drInWeek) s_drInWeek = true;
        else if (mode != 3 && s_drInWeek) { s_drInWeek = false; if (s_drCycle < 3) s_drCycle++; }
    }

    const bool toPhase2 = (ph == 2 || transDemo || arg0 == 254 || arg0 == 1);

    if (s_drPhase == 0) {
        if (ph == 2) { s_drPhase = 3; }
        else if (toPhase2) { s_drPhase = 2; s_drEmptyFrames = 0; }
        else if (!cutscene && mode >= 1 && mode != 12) { s_drPhase = 1; }
        else { hideNow = true; return 1.0f; }
    }

    if (s_drPhase == 1) {
        engageOk = true;
        if (toPhase2) { s_drPhase = 2; s_drEmptyFrames = 0; }
        else {
            int p = parts; if (p < 0) p = 0; if (p > 2) p = 2;
            if (p >= 2) {
                if (!bbi::argorok_flat(anm)) { s_drEmptyFrames = 0; return 0.5f; }
                if (++s_drEmptyFrames > 45) { s_drPhase = 2; s_drEmptyFrames = 0; }
                return 0.0f;
            }
            s_drEmptyFrames = 0;
            return 1.0f - static_cast<f32>(p) / 2.0f;
        }
    }

    if (s_drPhase == 2) {
        engageOk = true;
        hideNow  = true;
        if (ph == 2 && !cutscene && !transDemo) s_drPhase = 3;
        return 1.0f;
    }

    if (s_drPhase == 3) {
        engageOk = true;
        hideNow  = cutscene;
        if (mode == 13 || hp <= 0) { s_drPhase = 4; return 0.0f; }
        f32 intra = s_drInWeek ? (static_cast<f32>(wk < 0 ? 0 : (wk > 4 ? 4 : wk)) / 4.0f) : 0.0f;
        f32 used = (static_cast<f32>(s_drCycle) + intra) / 3.0f;
        f32 v = 1.0f - used;
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    return 0.0f;
}

static int s_znState = 0;
static int s_znPhaseSeen = -1;
static f32 s_znShown = 1.0f;
static int s_znDrainT = 0;
static int s_znDbgPhase = 0, s_znDbgAct = 0, s_znDbgCyc = 0, s_znDbgHp = 0, s_znDbgBig = 0;
static void zant_reset() {
    s_znState = 0; s_znPhaseSeen = -1; s_znShown = 1.0f; s_znDrainT = 0;
    s_znDbgPhase = s_znDbgAct = s_znDbgCyc = s_znDbgHp = s_znDbgBig = 0;
}

static f32 zant_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int phase, act, lastAct, cyc, hp;
    bool big;
    bbi::zant_read(actor, phase, act, lastAct, cyc, hp, big);
    s_znDbgPhase = phase; s_znDbgAct = act; s_znDbgCyc = cyc;
    s_znDbgHp = hp; s_znDbgBig = big ? 1 : 0;

    const int ACT_OPENING = 4, ACT_LAST_START_DEMO = 18, ACT_LAST_END_DEMO = 22, ACT_ROOM_CHANGE = 23;
    const int PHASE_OP = 0, PHASE_LAST = 6;

    const bool opening    = (phase == PHASE_OP) || (act == ACT_OPENING);
    const bool roomChange = (act == ACT_ROOM_CHANGE);
    const bool lastIntro  = (act == ACT_LAST_START_DEMO);
    const bool lastEnd    = (act == ACT_LAST_END_DEMO);
    const bool inLast     = (phase == PHASE_LAST);

    if (s_znState == 4 || lastEnd) { s_znState = 4; return 0.0f; }

    if (s_znState == 0) {
        if (opening) { hideNow = true; return 1.0f; }
        s_znState = inLast ? 3 : 1;
        s_znPhaseSeen = phase;
        s_znShown = 1.0f;
    }

    if (s_znState == 2) {
        engageOk = true;
        hideNow  = true;
        if (!roomChange && !opening && !lastIntro && phase != s_znPhaseSeen) {
            s_znPhaseSeen = phase;
            s_znShown = 1.0f;
            s_znState = inLast ? 3 : 1;
        }
        return 0.0f;
    }

    if (s_znState == 1 && phase != s_znPhaseSeen && !roomChange) {
        s_znPhaseSeen = phase;
        s_znShown = 1.0f;
        s_znState = inLast ? 3 : 1;
    }

    if (s_znState == 1) {
        engageOk = true;
        if (roomChange || lastIntro) {
            if (s_znDrainT < 8) {
                s_znShown += (0.0f - s_znShown) * 0.4f;
                if (s_znShown < 0.01f) s_znShown = 0.0f;
            } else {
                s_znShown = 0.0f;
            }
            if (++s_znDrainT >= 22) {
                s_znState = 2;
                hideNow  = true;
            }
            return s_znShown;
        }
        s_znDrainT = 0;

        int h = hp; if (h < 0) h = 0; if (h > 280) h = 280;
        f32 v = static_cast<f32>(h) / 280.0f;
        if (v < 0.04f) v = 0.04f;
        if (v < s_znShown) s_znShown = v;
        return s_znShown;
    }

    if (s_znState == 3) {
        engageOk = true;
        hideNow  = lastIntro;
        if (lastEnd) { s_znState = 4; return 0.0f; }
        int c = cyc; if (c < 0) c = 0; if (c > 3) c = 3;
        if (c >= 3) { s_znShown = 0.0f; return 0.0f; }
        const f32 caps[3] = { 600.0f, 400.0f, 200.0f };
        f32 frac = bbi::clamp01(static_cast<f32>(hp < 0 ? 0 : hp) / caps[c]);
        f32 v = (static_cast<f32>(2 - c) + frac) / 3.0f;
        if (v < s_znShown) s_znShown = v;
        return s_znShown;
    }

    return s_znShown;
}

static int s_gndState = 0;
static f32 s_gndPrevHp = -1.0f;
static f32 s_gndShown = 1.0f;
static int s_gndDbgAm = 0, s_gndDbgDcm = 0, s_gndDbgHorse = 0, s_gndDbgHp = 0, s_gndDbgKd = 0;
static void ganondorf_reset() {
    s_gndState = 0; s_gndPrevHp = -1.0f; s_gndShown = 1.0f;
    s_gndDbgAm = s_gndDbgDcm = s_gndDbgHorse = s_gndDbgHp = s_gndDbgKd = 0;
}

static f32 ganondorf_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int am, mm, dcm, horse, hp, kd;
    bbi::ganondorf_read(actor, am, mm, dcm, horse, hp, kd);
    s_gndDbgAm = am; s_gndDbgDcm = dcm; s_gndDbgHorse = horse; s_gndDbgHp = hp; s_gndDbgKd = kd;

    const int ACT_HEND = 6, ACT_WAIT = 10, ACT_END = 22;
    const bool event     = dComIfGp_event_runCheck() != 0;
    const bool horseback = (horse != 0) || (am >= 1 && am <= 5);
    const bool duelAct   = (am >= ACT_WAIT && am <= ACT_END);
    const bool demo      = (dcm != 0) || event;
    const bool dead      = (am == ACT_END);

    if (s_gndState == 3 || dead) {
        s_gndState = 3;
        s_gndPrevHp = static_cast<f32>(hp);
        return 0.0f;
    }
    if (s_gndState <= 1 && am == ACT_HEND) {
        s_gndState = 1;
    } else if (s_gndState == 0 && duelAct && !horseback) {
        s_gndState = demo ? 1 : 2;
        s_gndShown = 1.0f;
    } else if (s_gndState == 1 && duelAct && !demo) {
        s_gndState = 2;
        s_gndShown = 1.0f;
    }

    if ((s_gndState == 0 || s_gndState == 2) && s_gndPrevHp >= 0.0f &&
        static_cast<f32>(hp) < s_gndPrevHp - 0.5f) {
        engageOk = true;
    }
    s_gndPrevHp = static_cast<f32>(hp);

    if (s_gndState == 1) {
        hideNow  = true;
        engageOk = true;
        return 1.0f;
    }

    if (s_gndState == 2) {
        hideNow  = demo;
        if (!demo) engageOk = true;

        const int ACTION_DOWN = 21;
        const bool finishable = (am == ACTION_DOWN) && (mm < 3);
        int kc = kd; if (kc < 0) kc = 0;
        const int kBase = (kc > 2) ? 2 : kc;
        const f32 base = 1.0f - static_cast<f32>(kBase) / 3.0f;
        const f32 roundFrac = bbi::clamp01(1.0f - static_cast<f32>(hp < 0 ? 0 : hp) / 100.0f);

        const f32 target = (kc >= 3 && finishable) ? 0.0f
                                                   : base - roundFrac / 3.0f;

        if (target <= s_gndShown) {
            s_gndShown = target;
        } else {
            s_gndShown += 1.0f / 75.0f;
            if (s_gndShown > target) s_gndShown = target;
        }
        return s_gndShown;
    }

    hideNow = demo;
    return bbi::clamp01(static_cast<f32>(hp < 0 ? 0 : hp) / 24.0f);
}

static bool s_mgnDown = false;
static int  s_mgnAffAtDown = 0;
static f32  s_mgnShown = 1.0f;
static int  s_mgnDbgAct = 0, s_mgnDbgHp = 0, s_mgnDbgPwr = 0, s_mgnDbgInv = 0;
static int  s_mgnDbgAff = 0;

static void beastganon_reset() {
    s_mgnDown = false;
    s_mgnAffAtDown = 0;
    s_mgnShown = 1.0f;
    s_mgnDbgAct = s_mgnDbgHp = s_mgnDbgPwr = s_mgnDbgInv = s_mgnDbgAff = 0;
}

static f32 mgn_base_bar(int aff) {
    int third = aff / 2;
    if (third < 0) third = 0;
    if (third > 3) third = 3;
    return 1.0f - static_cast<f32>(third) / 3.0f;
}

static f32 beastganon_live01(fopAc_ac_c* actor, bool& engageOk, bool& hideNow) {
    engageOk = false;
    hideNow  = false;
    int act, hp, pwr, invuln, aa8, a9c, b00, afd;
    bool isDown;
    bbi::beastganon_read(actor, act, hp, isDown, pwr, invuln, aa8, a9c, b00, afd);

    int aff, downTimer;
    bool down;
    bbi::beastganon_progress(actor, aff, down, downTimer);

    s_mgnDbgAct = act;
    s_mgnDbgHp  = hp;
    s_mgnDbgPwr = pwr;
    s_mgnDbgInv = invuln;
    s_mgnDbgAff = aff;

    const bool dead = (act == daB_MGN_c::ACTION_DEATH_e) || aff >= 6;
    if (dead) {
        return 0.0f;
    }

    const bool opening = (act == daB_MGN_c::ACTION_OPENING_e) ||
                         (dComIfGp_event_runCheck() != 0 && aff == 0 && !isDown && !down);
    if (opening) {
        hideNow = true;
        return 1.0f;
    }

    engageOk = true;

    if (hp < 0) hp = 0;
    if (hp > 700) hp = 700;
    return static_cast<f32>(hp) / 700.0f;
}

static int collectBossCallback(void* pActor, void*) {
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(pActor);
    if (!a) return 0;

    s16 nm = fopAcM_GetName(a);
    const BossDef* def = classifyBoss(nm);
    if (!def) return 0;
    if (def->ignoreFn && def->ignoreFn(a)) return 0;

    if (def->miniboss) {
        if (dComIfGs_isStageMiddleBoss()) return 0;
    } else if (dComIfGs_isStageBossEnemy()) {
        return 0;
    }

    fpc_ProcID id = fopAcM_GetID(a);
    if (s_defeated.count(id)) return 0;

    Candidate c;
    c.id = id;
    c.name = nm;
    c.health = a->health;
    c.actor = a;
    c.def = def;
    c.hasRatio = false;
    c.ratio = 1.0f;
    if (def->hpRatio) {
        c.hasRatio = true;
        c.ratio = def->hpRatio(a);
    }
    s_cands.push_back(c);
    return 0;
}

static fpc_ProcID s_diaActiveId = 0;
static fpc_ProcID s_obActiveId = 0;
static fpc_ProcID s_dsActiveId = 0;
static fpc_ProcID s_yoActiveId = 0;
static fpc_ProcID s_tnActiveId = 0;
static fpc_ProcID s_drActiveId = 0;
static fpc_ProcID s_znActiveId = 0;
static fpc_ProcID s_gndActiveId = 0;
static fpc_ProcID s_mgnActiveId = 0;

static bool s_fmFinalDown = false;

static void reset_state() {
    s_boss = {};
    s_fmFinalDown = false;
}

void boss_bar_force_reset() {
    reset_state();
    s_defeated.clear();
    s_recentEngagedLabel = nullptr;
    s_recentEngagedFrames = 0;
    s_justDefeated = false;
}

void update_boss_bar(const LogService*, ModContext*) {
    if (s_bossBarPreviewFrames > 0) s_bossBarPreviewFrames--;
    if (!g_configBossBarEnabled && !boss_rush_is_fighting_here()) {
        reset_state();
        s_defeated.clear();
        return;
    }

    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) {
        return;
    }

    if (boss_rush_is_returning_to_chamber()) {
        if (s_boss.valid) reset_state();
        return;
    }

    if (boss_rush_is_fight_retry_warp()) {
        if (s_boss.valid) reset_state();
        return;
    }

    s_cands.clear();
    fopAcIt_Executor(collectBossCallback, nullptr);

    for (const auto& c : s_cands) {
        if (c.name == fpcNm_B_GG_e && c.actor) {
            c.actor->attention_info.flags |= fopAc_AttnFlag_BATTLE_e;
        }
    }

    if (s_recentEngagedFrames > 0) s_recentEngagedFrames--;

    static int s_idleFrames = 0;
    if (s_cands.empty() && !s_boss.valid) {
        if (++s_idleFrames > 300) s_defeated.clear();
    } else {
        s_idleFrames = 0;
    }

    const Candidate* best = nullptr;
    dAttention_c* att = dComIfGp_getAttention();
    fopAc_ac_c* lockTarget = att ? att->LockonTarget(0) : nullptr;

    if (lockTarget != nullptr) {
        for (const auto& c : s_cands) {
            if (!c.def->ownBar) continue;
            if ((c.name == fpcNm_B_TN_e || c.name == fpcNm_B_GG_e) && c.actor == lockTarget) {
                best = &c;
                break;
            }
        }
    }

    if (!best) {
        for (const auto& c : s_cands) {
            if (!c.def->ownBar) continue;
            if (s_boss.valid && c.id == s_boss.id) { best = &c; break; }
            if (!best || c.health > best->health) best = &c;
        }
    }

    if (best) {
        bool switchingSameBoss = false;
        if (!s_boss.valid || s_boss.id != best->id) {
            const bool prevSwitchable = (s_boss.name == fpcNm_B_TN_e || s_boss.name == fpcNm_B_GG_e);
            const bool nextSwitchable = (best->name == fpcNm_B_TN_e || best->name == fpcNm_B_GG_e);
            switchingSameBoss = (s_boss.valid && prevSwitchable && nextSwitchable && s_boss.engaged);
            const f32 prevAlpha = s_boss.alpha;
            reset_state();
            s_boss.valid = true;
            s_boss.id = best->id;
            s_boss.name = best->name;
            s_boss.label = best->def->label;
            s_boss.miniboss = best->def->miniboss;
            s_boss.custom = best->hasRatio || best->def->aggregate;

            if (best->def->aggregate) {
                if (s_diaActiveId != best->id) { diababa_reset(); s_diaActiveId = best->id; }
            } else {
                s_diaActiveId = 0;
            }
            if (best->name == fpcNm_B_OB_e) {
                if (s_obActiveId != best->id) { morpheel_reset(); s_obActiveId = best->id; }
            } else {
                s_obActiveId = 0;
            }
            if (best->name == fpcNm_B_DS_e) {
                if (s_dsActiveId != best->id) { stallord_reset(); s_dsActiveId = best->id; }
            } else {
                s_dsActiveId = 0;
            }
            if (best->name == fpcNm_B_YO_e) {
                if (s_yoActiveId != best->id) { blizzeta_reset(); s_yoActiveId = best->id; }
            } else {
                s_yoActiveId = 0;
            }
            if (best->name == fpcNm_B_TN_e) {
                if (s_tnActiveId != best->id) { darknut_reset(); s_tnActiveId = best->id; }
            } else {
                s_tnActiveId = 0;
            }
            if (best->name == fpcNm_B_DR_e) {
                if (s_drActiveId != best->id) { argorok_reset(); s_drActiveId = best->id; }
            } else {
                s_drActiveId = 0;
            }
            if (best->name == fpcNm_B_ZANT_e) {
                if (s_znActiveId != best->id) { zant_reset(); s_znActiveId = best->id; }
            } else {
                s_znActiveId = 0;
            }
            if (best->name == fpcNm_B_GND_e) {
                if (s_gndActiveId != best->id) { ganondorf_reset(); s_gndActiveId = best->id; }
            } else {
                s_gndActiveId = 0;
            }
            if (best->name == fpcNm_B_MGN_e) {
                if (s_mgnActiveId != best->id) { beastganon_reset(); s_mgnActiveId = best->id; }
            } else {
                s_mgnActiveId = 0;
            }
            s_boss.maxHp = best->health > 0 ? best->health : 0;
            s_boss.curHp = best->health;
            s_boss.lastHealth = best->health;
            s_boss.sawHealth = best->health > 0;
            s_boss.live01 = 1.0f;
            s_boss.shownRatio = 1.0f;
            s_boss.displayRatio = 1.0f;
            s_boss.alpha = switchingSameBoss ? prevAlpha : 0.0f;

            if (switchingSameBoss) {
                s_boss.engaged = true;
                s_boss.alpha = 1.0f;
            }

            if (s_recentEngagedLabel && s_boss.label &&
                std::strcmp(s_recentEngagedLabel, s_boss.label) == 0 && s_recentEngagedFrames > 0 &&
                (!best->def->engagedHint || best->def->engagedHint(best->actor))) {
                s_boss.engaged = true;
            }

            char buf[112];
            std::snprintf(buf, sizeof(buf), "[BossBar] tracking %s (custom=%d hp=%d eng=%d)",
                          s_boss.label ? s_boss.label : "?",
                          static_cast<int>(s_boss.custom), static_cast<int>(best->health),
                          static_cast<int>(s_boss.engaged));
        }

        s_boss.missingFrames = 0;
        s_boss.gone = false;

        s_dbgExtra[0] = '\0';
        if (best->actor) {
            int a = 0, b = 0, c = 0, d = 0;
            if (best->name == fpcNm_E_FM_e) {
                bbi::fyrus_dbg(best->actor, a, b, c, d);
                s_fmFinalDown = (a >= 3);
                std::snprintf(s_dbgExtra, sizeof(s_dbgExtra), " | FM downs=%d hp=%d act=%d demo=%d", a, b, c, d);
            } else if (best->name == fpcNm_B_GM_e) {
                bbi::armogohma_dbg(best->actor, a, b, c);
                std::snprintf(s_dbgExtra, sizeof(s_dbgExtra), " | GM hits=%d act=%d demo=%d", a, b, c);
            } else if (best->name == fpcNm_E_GOB_e) {
                bbi::dangoro_dbg(best->actor, a, b);
                std::snprintf(s_dbgExtra, sizeof(s_dbgExtra), " | GOB thrown=%d bounce=%d", a, b);
            } else if (best->name == fpcNm_E_VT_e) {
                bbi::deathsword_dbg(best->actor, a, b, c);
                std::snprintf(s_dbgExtra, sizeof(s_dbgExtra), " | VT dmg=%d delta=%d act=%d", a, b, c);
            }
        }

        const bool wasEngaged = s_boss.engaged;
        f32 newLive;

        const bool hintOnlyEngage = (best->name == fpcNm_E_VT_e);

        s_boss.aggHide = false;
        if (best->def->aggregate) {
            bool diaEngage = false, diaHide = false;
            newLive = diababa_live01(diaEngage, diaHide);
            s_boss.aggHide = diaHide;
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra), " | BQ ph=%d hp=%d cyc=%d p2=%d hide=%d",
                          s_diaDbgPhase, s_diaDbgHeads, s_diaDbgRounds,
                          static_cast<int>(s_diaP2Started), static_cast<int>(diaHide));
            if (diaEngage) s_boss.engaged = true;
        } else if (best->name == fpcNm_B_OB_e && best->actor) {
            bool obEngage = false, obHide = false;
            newLive = morpheel_live01(best->actor, obEngage, obHide);
            s_boss.aggHide = obHide;
            if (obEngage) s_boss.engaged = true;
            s_boss.curHp = static_cast<s16>(s_obDbgHp);
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | OB ph=%d act=%d demo=%d hp=%d fish=%d fin=%d hit=%d hide=%d",
                          s_obPhase, s_obDbgAct, s_obDbgDemo, s_obDbgHp, s_obDbgFish,
                          s_obDbgFin, s_obDbgHit, static_cast<int>(obHide));
        } else if (best->name == fpcNm_B_DS_e && best->actor) {
            bool dsEngage = false, dsHide = false;
            newLive = stallord_live01(best->actor, dsEngage, dsHide);
            s_boss.aggHide = dsHide;
            if (dsEngage) s_boss.engaged = true;
            s_boss.curHp = static_cast<s16>(s_dsDbgHp);
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | DS ph=%d phase=%d bone=%d hp=%d act=%d hide=%d",
                          s_dsPhase, s_dsDbgPhase, s_dsDbgBone, s_dsDbgHp, s_dsDbgAct,
                          static_cast<int>(dsHide));
        } else if (best->name == fpcNm_B_YO_e && best->actor) {
            bool yoEngage = false, yoHide = false;
            newLive = blizzeta_live01(best->actor, yoEngage, yoHide);
            s_boss.aggHide = yoHide;
            if (yoEngage) s_boss.engaged = true;
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | YO ph=%d model=%d lpd=%d act=%d hide=%d",
                          s_yoPhase, s_yoDbgModel, s_yoDbgLpd, s_yoDbgAct,
                          static_cast<int>(yoHide));
        } else if (best->name == fpcNm_B_TN_e && best->actor) {
            bool tnEngage = false, tnHide = false;
            newLive = darknut_live01(best->actor, tnEngage, tnHide);
            s_boss.aggHide = tnHide;
            if (tnEngage) s_boss.engaged = true;

            if (switchingSameBoss) {
                s_boss.live01 = newLive;
                s_boss.shownRatio = newLive;
                s_boss.displayRatio = newLive;
            }

            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | TN ph=%d mode=%d break=%d dmg=%d(peak%d)/%d refill=%d hide=%d",
                          s_tnPhase, s_tnDbgMode, s_tnDbgBrk, s_tnDbgCur, s_tnPeakCur,
                          s_tnDbgMax, s_tnRefill, static_cast<int>(tnHide));
        } else if (best->name == fpcNm_B_DR_e && best->actor) {
            bool drEngage = false, drHide = false;
            newLive = argorok_live01(best->actor, drEngage, drHide);
            s_boss.aggHide = drHide;
            if (drEngage) s_boss.engaged = true;
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | DR ph=%d mode=%d a0=%d anm=%d p7d1=%d parts=%d hp=%d wk=%d cyc=%d hide=%d",
                          s_drPhase, s_drDbgMode, s_drDbgA0, s_drDbgAnm, s_drDbgP, s_drDbgParts,
                          s_drDbgHp, s_drDbgWk, s_drCycle, static_cast<int>(drHide));
        } else if (best->name == fpcNm_B_ZANT_e && best->actor) {
            bool znEngage = false, znHide = false;
            newLive = zant_live01(best->actor, znEngage, znHide);
            s_boss.aggHide = znHide;
            if (znEngage) s_boss.engaged = true;
            s_boss.curHp = static_cast<s16>(s_znDbgHp);
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | ZAN st=%d phase=%d act=%d cyc=%d big=%d hp=%d hide=%d",
                          s_znState, s_znDbgPhase, s_znDbgAct, s_znDbgCyc, s_znDbgBig,
                          s_znDbgHp, static_cast<int>(znHide));
        } else if (best->name == fpcNm_B_GND_e && best->actor) {
            bool gndEngage = false, gndHide = false;
            newLive = ganondorf_live01(best->actor, gndEngage, gndHide);
            s_boss.aggHide = gndHide;
            if (gndEngage) s_boss.engaged = true;
            s_boss.curHp = static_cast<s16>(s_gndDbgHp);
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | GND st=%d am=%d dcm=%d horse=%d hp=%d kd=%d hide=%d",
                          s_gndState, s_gndDbgAm, s_gndDbgDcm, s_gndDbgHorse, s_gndDbgHp,
                          s_gndDbgKd, static_cast<int>(gndHide));
        } else if (best->name == fpcNm_B_MGN_e && best->actor) {
            bool mgnEngage = false, mgnHide = false;
            newLive = beastganon_live01(best->actor, mgnEngage, mgnHide);
            s_boss.aggHide = mgnHide;
            s_boss.curHp = static_cast<s16>(newLive * 100.0f);
            std::snprintf(s_dbgExtra, sizeof(s_dbgExtra),
                          " | MGN act=%d hp=%d pwr=%d aff=%d down=%d hide=%d",
                          s_mgnDbgAct, s_mgnDbgHp, s_mgnDbgPwr, s_mgnDbgAff,
                          static_cast<int>(s_mgnDown), static_cast<int>(mgnHide));
        } else if (s_boss.custom) {
            newLive = best->ratio;
            if (newLive < 0.997f && !hintOnlyEngage) s_boss.engaged = true;

            if (switchingSameBoss) {
                s_boss.live01 = newLive;
                s_boss.shownRatio = newLive;
                s_boss.displayRatio = newLive;
            }
        } else {
            const s16 h = best->health;
            if (h > s_boss.maxHp) s_boss.maxHp = h;
            if (h > 0) s_boss.sawHealth = true;

            if (s_boss.lastHealth > 0 && h >= 0 && h < s_boss.lastHealth) {
                s_boss.engaged = true;
            }
            s_boss.lastHealth = h;
            s_boss.curHp = h;

            newLive = (s_boss.sawHealth && s_boss.maxHp > 0)
                        ? static_cast<f32>(h) / static_cast<f32>(s_boss.maxHp)
                        : 1.0f;

            if (switchingSameBoss) {
                s_boss.live01 = newLive;
                s_boss.shownRatio = newLive;
                s_boss.displayRatio = newLive;
            }
        }
        if (best->name == fpcNm_B_GG_e && best->actor) {
            s_boss.aggHide = !darknut_los_hint(best->actor);
        }

        if (newLive < 0.0f) newLive = 0.0f;
        if (newLive > 1.0f) newLive = 1.0f;
        s_boss.live01 = newLive;

        bool lockedOn = false;
        if (best->actor) {
            dAttention_c* att = dComIfGp_getAttention();
            if (att && att->LockonTarget(0) == best->actor) lockedOn = true;
        }
        if (lockedOn) {
            if (++s_boss.lockFrames >= 8 && !hintOnlyEngage) s_boss.engaged = true;
        } else {
            s_boss.lockFrames = 0;
        }

        if (best->def->engagedHint && best->actor && !dComIfGp_event_runCheck() &&
            best->def->engagedHint(best->actor)) {
            s_boss.engaged = true;
        }

        if (s_boss.engaged && !wasEngaged) {
            s_recentEngagedLabel = s_boss.label;
            s_recentEngagedFrames = 150;
            char buf[112];
            std::snprintf(buf, sizeof(buf), "[BossBar] fight started (%s %d/%d lock=%d)",
                          s_boss.label ? s_boss.label : "?",
                          static_cast<int>(s_boss.curHp), static_cast<int>(s_boss.maxHp),
                          static_cast<int>(lockedOn));
        }
        if (s_boss.engaged) s_recentEngagedFrames = 150;

            const bool phase1Empty = (best->name == fpcNm_B_OB_e && s_obPhase < 4) ||
                                  (best->name == fpcNm_B_BQ_e && !s_diaP2Started) ||
                                  (best->name == fpcNm_B_DS_e && s_dsPhase < 4) ||
                                  (best->name == fpcNm_B_YO_e && s_yoPhase < 4) ||
                                  (best->name == fpcNm_B_TN_e && s_tnPhase < 4) ||
                                  (best->name == fpcNm_B_DR_e && s_drPhase < 4) ||
                                  (best->name == fpcNm_B_ZANT_e && s_znState != 4) ||
                                  (best->name == fpcNm_B_GND_e && s_gndState != 3) ||
                                  (best->name == fpcNm_B_MGN_e && s_mgnDbgAct != daB_MGN_c::ACTION_DEATH_e);
            if (s_boss.engaged && s_boss.live01 <= 0.02f && !phase1Empty) s_boss.deadTimer++;
            else s_boss.deadTimer = 0;

        } else if (s_boss.valid) {
            s_boss.missingFrames++;
            if (s_boss.engaged) {
                const bool fyrusFinalDownVanished = (s_boss.name == fpcNm_E_FM_e) && s_fmFinalDown;
                const bool deathSwordVanished = (s_boss.name == fpcNm_E_VT_e);
                const bool diaPhase1 = (s_boss.name == fpcNm_B_BQ_e) && !s_diaP2Started;
                const bool likelyDead = (!diaPhase1 && (!s_boss.custom || fyrusFinalDownVanished || deathSwordVanished) && s_boss.live01 < 0.45f);
                if (likelyDead) {
                    s_boss.live01 += (0.0f - s_boss.live01) * 0.35f;
                    if (s_boss.live01 < 0.01f) s_boss.live01 = 0.0f;
                }
                if (s_boss.missingFrames > (likelyDead ? 10 : 60)) s_boss.gone = true;
            if (s_boss.missingFrames > (likelyDead ? 24 : 240)) {
                if (likelyDead) {
                    s_defeated.insert(s_boss.id);
                    s_justDefeated = true;
                }
                reset_state();
            }
        } else {
            if (s_boss.missingFrames > 150) reset_state();
        }
    }

    if (s_boss.valid) {
        if (s_boss.aggHide) {
            if (s_boss.alpha < 0.04f) {
                s_boss.shownRatio = s_boss.live01;
                s_boss.displayRatio = s_boss.live01;
            }
        } else {
            s_boss.shownRatio = s_boss.live01;
            s_boss.displayRatio += (s_boss.live01 - s_boss.displayRatio) * 0.15f;
        }

        const bool dead = (s_boss.deadTimer > 18);
        f32 target = (s_boss.engaged && !s_boss.gone && !dead && !s_boss.aggHide) ? 1.0f : 0.0f;

        f32 speed = (target < s_boss.alpha) ? 0.24f : 0.18f;
        s_boss.alpha += (target - s_boss.alpha) * speed;

        if (dead && s_boss.alpha < 0.01f) {
            s_defeated.insert(s_boss.id);
            s_justDefeated = true;
            reset_state();
        }
    }
}

static JUTFont* boss_name_font() {
    JUTFont* font = mDoExt_getRubyFont();
    if (!font) font = mDoExt_getSubFont();
    if (!font) font = mDoExt_getMesgFont();
    return font;
}

static f32 get_text_width_ingame(const char* text, f32 charW) {
    JUTFont* font = boss_name_font();
    if (font) {
        f32 total = 0.0f;
        f32 base = static_cast<f32>(font->getWidth());
        if (base <= 0.0f) base = 1.0f;
        for (size_t i = 0; text[i] != '\0'; i++) {
            f32 w = static_cast<f32>(font->getWidth(text[i]));
            if (w <= 0.0f) w = base;
            total += w * (charW / base);
        }
        return total;
    }
    return static_cast<f32>(std::strlen(text)) * charW;
}

static void draw_text_ingame(const char* text, f32 x, f32 y, f32 charW, f32 charH, JUtility::TColor color) {
    JUTFont* font = boss_name_font();
    if (!font) return;

    font->setGX();

    static const f32 kOff[8][2] = {
        { 1.1f, 0.0f}, {-1.1f, 0.0f}, {0.0f,  1.1f}, {0.0f, -1.1f},
        { 0.8f, 0.8f}, {0.8f, -0.8f}, {-0.8f, 0.8f}, {-0.8f, -0.8f},
    };
    u8 a = color.a;
    font->setCharColor(JUtility::TColor(32, 20, 12, static_cast<u8>(a * 0.9f)));
    for (const auto& o : kOff) {
        font->drawString_scale(x + o[0], y + o[1], charW, charH, text, true);
    }

    font->setGradColor(JUtility::TColor(255, 250, 232, a),
                       JUtility::TColor(226, 196, 140, a));
    font->drawString_scale(x, y, charW, charH, text, true);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

static void fill_vgrad(f32 x, f32 y, f32 w, f32 h, JUtility::TColor top, JUtility::TColor bot) {
    if (w <= 0.0f || h <= 0.0f) return;
    J2DOrthoGraph g;
    g.setColor(top, top, bot, bot);
    g.fillBox(JGeometry::TBox2<f32>(x, y, x + w, y + h));
}

static void draw_rail(f32 x, f32 y, f32 w, f32 h, f32 a) {
    auto A = [a](u8 base) -> u8 { return static_cast<u8>(static_cast<f32>(base) * a); };
    fill_vgrad(x, y, w, h, JUtility::TColor(12, 9, 7, A(245)), JUtility::TColor(6, 5, 4, A(245)));
    fill_vgrad(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f,
               JUtility::TColor(220, 188, 124, A(245)), JUtility::TColor(116, 88, 44, A(245)));
    fill_vgrad(x + 1.0f, y + 1.0f, w - 2.0f, h * 0.35f + 0.5f,
               JUtility::TColor(248, 230, 186, A(200)), JUtility::TColor(248, 230, 186, A(0)));
}

static dMeter2Draw_c* s_meter2 = nullptr;

static void draw_bar_endcaps(f32 barX, f32 barW, f32 barY, f32 barH, f32 a) {
    if (!s_meter2 || !s_meter2->mpKanteraScreen) return;
    J2DScreen* screen = s_meter2->mpKanteraScreen;

    CPaneMgr* meter  = s_meter2->mpMagicMeter;
    CPaneMgr* base   = s_meter2->mpMagicBase;
    CPaneMgr* frameL = s_meter2->mpMagicFrameL;
    CPaneMgr* frameR = s_meter2->mpMagicFrameR;
    CPaneMgr* parent = s_meter2->mpMagicParent;
    if (!meter || !base || !frameL || !frameR || !parent) return;
    J2DPane* flPane = frameL->getPanePtr();
    J2DPane* frPane = frameR->getPanePtr();
    if (!flPane || !frPane) return;

    const f32 kEndScale    = 1.15f;
    const f32 kEndOverhang = 2.0f;
    const f32 kEndXAdj     = 23.0f;
    const f32 kEndYAdj     = 2.0f;

    const f32 svPScX = parent->getScaleX(), svPScY = parent->getScaleY();
    const f32 svPA = parent->getAlphaRate();
    const f32 svMA = meter->getAlphaRate(), svBA = base->getAlphaRate();
    const f32 svFLA = frameL->getAlphaRate(), svFRA = frameR->getAlphaRate();
    const f32 svFLScX = frameL->getScaleX(), svFLScY = frameL->getScaleY();
    const f32 svFRScX = frameR->getScaleX(), svFRScY = frameR->getScaleY();
    const f32 svMW = meter->getSizeX(), svMH = meter->getSizeY();
    const f32 svBW = base->getSizeX(),  svBH = base->getSizeY();

    parent->setAlphaRate(a);
    parent->scale(1.0f, 1.0f);
    meter->setAlphaRate(0.0f);
    base->setAlphaRate(0.0f);
    meter->resize(0.0f, 0.0f);
    base->resize(0.0f, 0.0f);
    frameL->scale(kEndScale, kEndScale);
    frameR->scale(kEndScale, kEndScale);
    frameL->setAlphaRate(0.0f);
    frameR->setAlphaRate(0.0f);

    J2DGrafContext* graf = dComIfGp_getCurrentGrafPort();
    if (graf) graf->setup2D();

    screen->draw(0.0f, 0.0f, graf);

    const f32 flx = flPane->getGlbBounds().i.x, fly = flPane->getGlbBounds().i.y;
    const f32 frx = frPane->getGlbBounds().i.x, fry = frPane->getGlbBounds().i.y;
    const f32 ew = flPane->getGlbBounds().getWidth()  * kEndScale;
    const f32 eh = flPane->getGlbBounds().getHeight() * kEndScale;

    const f32 ty = barY + barH * 0.5f - eh * 0.5f + kEndYAdj;
    const f32 lxOff = (barX          - ew * 0.5f - kEndOverhang + kEndXAdj) - flx;
    const f32 rxOff = (barX + barW   - ew * 0.5f + kEndOverhang + kEndXAdj) - frx;

    frameL->setAlphaRate(a);
    frameR->setAlphaRate(0.0f);
    screen->draw(lxOff, ty - fly, graf);

    frameL->setAlphaRate(0.0f);
    frameR->setAlphaRate(a);
    screen->draw(rxOff, ty - fry, graf);

    parent->scale(svPScX, svPScY);
    parent->setAlphaRate(svPA);
    meter->setAlphaRate(svMA);
    base->setAlphaRate(svBA);
    frameL->setAlphaRate(svFLA);
    frameR->setAlphaRate(svFRA);
    frameL->scale(svFLScX, svFLScY);
    frameR->scale(svFRScX, svFRScY);
    meter->resize(svMW, svMH);
    base->resize(svBW, svBH);
}

static f32 s_twilightHdShift = 0.0f;

static f32 boss_bar_twilight_hd_shift(f32 baseBarY) {
    f32 target = 0.0f;
    if (twilight_hd_enabled()) {
        f32 stackBottom = stamina_twilight_hd_bottom();
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* meterDraw = (meter != nullptr) ? meter->getMeterDrawPtr() : nullptr;
        if (twilight_hd_gauge_visible(meterDraw)) {
            const f32 gaugeBottom =
                twilight_hd_top_meter_center_y() + stamina_twilight_hd_frame_height() * 0.5f;
            if (gaugeBottom > stackBottom) {
                stackBottom = gaugeBottom;
            }
        }
        constexpr f32 kBossBarLabelSpace = 22.0f;
        const f32 frameTop = baseBarY - 4.0f - kBossBarLabelSpace;
        if (stackBottom > 0.0f && stackBottom + 4.0f > frameTop) {
            target = stackBottom + 4.0f - frameTop;
        }
    }
    s_twilightHdShift += (target - s_twilightHdShift) * 0.15f;
    if (s_twilightHdShift < 0.05f && target == 0.0f) {
        s_twilightHdShift = 0.0f;
    }
    return s_twilightHdShift;
}

static void draw_text_soft(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                           JUtility::TColor top, JUtility::TColor bot, JUtility::TColor shadow,
                           f32 radius) {
    JUTFont* font = boss_name_font();
    if (!font) return;
    font->setGX();
    if (shadow.a > 0 && radius > 0.0f) {
        static const f32 kDir[8][2] = {
            { 1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f,  1.0f}, {0.0f, -1.0f},
            { 0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {-0.7f, -0.7f},
        };
        font->setCharColor(shadow);
        for (const auto& d : kDir) {
            font->drawString_scale(x + d[0] * radius, y + d[1] * radius, charW, charH, text, true);
        }
    }
    font->setGradColor(top, bot);
    font->drawString_scale(x, y, charW, charH, text, true);
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

static void copy_boss_name(const char* src, char* out, size_t outSize, bool upper) {
    if (!src) src = "Boss";
    size_t i = 0;
    for (; src[i] != '\0' && i < outSize - 1; i++) {
        const char c = src[i];
        out[i] = (upper && c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    out[i] = '\0';
}

static const char* elden_ring_title(const char* label) {
    static const struct { const char* label; const char* title; } kTitles[] = {
        {"Diababa",          "Diababa, the Twilit Parasite"},
        {"Fyrus",            "Fyrus, the Twilit Igniter"},
        {"Morpheel",         "Morpheel, the Twilit Aquatic"},
        {"Stallord",         "Stallord, the Twilit Fossil"},
        {"Blizzeta",         "Blizzeta, the Twilit Ice Mass"},
        {"Armogohma",        "Armogohma, the Twilit Arachnid"},
        {"Argorok",          "Argorok, the Twilit Dragon"},
        {"Zant",             "Zant, the Usurper King"},
        {"Phantom Zant",     "Phantom Zant, the False King"},
        {"Puppet Zelda",     "Zelda, the Puppet Princess"},
        {"Ganondorf",        "Ganondorf, the Dark Lord"},
        {"Dark Beast Ganon", "Ganon, the Dark Beast"},
        {"King Bulblin",     "King Bulblin, the Warlord"},
        {"Death Sword",      "Death Sword, the Cursed Blade"},
        {"Darkhammer",       "Darkhammer, the Iron Wall"},
        {"Deku Toad",        "Deku Toad, the Tunnel Dweller"},
        {"Dangoro",          "Dangoro, the Goron Guardian"},
        {"Ook",              "Ook, the Forest Thief"},
        {"Darknut",          "Darknut, the Iron Knight"},
        {"Aeralfos",         "Aeralfos, the Sky Guard"},
    };
    if (label) {
        for (const auto& t : kTitles) {
            if (std::strcmp(label, t.label) == 0) return t.title;
        }
    }
    return label ? label : "Boss";
}

struct BossBarScreen {
    f32 usableW;
    f32 centreX;
    f32 topY;
    f32 bottomY;
};

static BossBarScreen boss_bar_screen() {
    BossBarScreen s;
    f32 minX = mDoGph_gInf_c::getMinXF();
    f32 maxX = mDoGph_gInf_c::getMaxXF();
    if (maxX <= minX + 1.0f) { minX = 0.0f; maxX = 640.0f; }
    const f32 sMin = mDoGph_gInf_c::getSafeMinXF();
    const f32 sMax = mDoGph_gInf_c::getSafeMaxXF();
    s.usableW = (sMax > sMin + 1.0f) ? (sMax - sMin) : (maxX - minX);
    s.centreX = (minX + maxX) * 0.5f;
    s.topY = mDoGph_gInf_c::getMinYF();
    if (s.topY < 0.0f || s.topY > 200.0f) s.topY = 0.0f;
    f32 bottom = mDoGph_gInf_c::getSafeMaxYF();
    const f32 maxY = mDoGph_gInf_c::getMaxYF();
    if (bottom <= s.topY + 100.0f) bottom = maxY;
    if (bottom <= s.topY + 100.0f) bottom = 448.0f;
    s.bottomY = bottom;
    return s;
}

static f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void fill_hgrad(f32 x, f32 y, f32 w, f32 h, JUtility::TColor left, JUtility::TColor right) {
    if (w <= 0.0f || h <= 0.0f) return;
    J2DOrthoGraph g;
    g.setColor(left, right, left, right);
    g.fillBox(JGeometry::TBox2<f32>(x, y, x + w, y + h));
}

extern const ResourceService* get_resource_service();
extern "C" ModContext* mod_ctx;

static ResourceBuffer s_erLeftBti = RESOURCE_BUFFER_INIT;
static ResourceBuffer s_erRightBti = RESOURCE_BUFFER_INIT;
static J2DPicture* s_erLeftPic = nullptr;
static J2DPicture* s_erRightPic = nullptr;
static bool s_erTexTried = false;

static J2DPicture* make_er_picture(ResourceBuffer& buf) {
    if (buf.data == nullptr) return nullptr;
    ResTIMG* img = reinterpret_cast<ResTIMG*>(buf.data);
    img->alphaEnabled = 1;
    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
    J2DPicture* pic = JKR_NEW J2DPicture(img);
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    return pic;
}

static void load_er_ornaments() {
    if (s_erTexTried) return;
    s_erTexTried = true;
    const ResourceService* res = get_resource_service();
    if (res == nullptr || mod_ctx == nullptr) return;
    res->load(mod_ctx, "textures/boss_bar/er_ornament_left.bti", &s_erLeftBti);
    res->load(mod_ctx, "textures/boss_bar/er_ornament_right.bti", &s_erRightBti);
    s_erLeftPic = make_er_picture(s_erLeftBti);
    s_erRightPic = make_er_picture(s_erRightBti);
}

static void free_er_ornaments() {
    JKR_DELETE(s_erLeftPic);
    JKR_DELETE(s_erRightPic);
    s_erLeftPic = nullptr;
    s_erRightPic = nullptr;
    s_erTexTried = false;
    const ResourceService* res = get_resource_service();
    if (res != nullptr && mod_ctx != nullptr) {
        res->free(mod_ctx, &s_erLeftBti);
        res->free(mod_ctx, &s_erRightBti);
    }
}

static void draw_er_picture(J2DPicture* pic, f32 x, f32 y, f32 w, f32 h, u8 alpha) {
    if (pic == nullptr) return;
    pic->setAlpha(alpha);
    pic->draw(x, y, w, h, false, false, false);
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

static void draw_boss_bar_elden_ring(f32 a, const char* label, f32 live, f32 chip,
                                     const BossBarScreen& scr) {
    auto A = [a](u8 base) -> u8 { return static_cast<u8>(static_cast<f32>(base) * a); };

    const f32 barW = clampf(scr.usableW * 0.54f, 340.0f, 600.0f);
    const f32 barH = 4.5f;
    const f32 barX = scr.centreX - barW * 0.5f + g_configBossBarX;
    const f32 barY = scr.bottomY - 84.0f + g_configBossBarY;

    constexpr f32 kOrnScale = 0.42f;
    constexpr f32 kTexLineCenterY = 26.0f;
    const f32 lineH = 4.0f * kOrnScale;
    const f32 lineY = barY + barH + 0.6f;
    const f32 lineCenterY = lineY + lineH * 0.5f;

    load_er_ornaments();
    qa_hud_scale_begin(barX + barW * 0.5f, barY);

    fill_vgrad(barX - 1.0f, barY - 1.5f, barW + 2.0f, barH + 3.0f,
               JUtility::TColor(0, 0, 0, A(120)), JUtility::TColor(0, 0, 0, A(150)));
    fill_vgrad(barX, barY, barW, barH,
               JUtility::TColor(26, 20, 20, A(200)), JUtility::TColor(12, 9, 9, A(200)));

    if (chip > live + 0.001f) {
        fill_vgrad(barX + barW * live, barY, barW * (chip - live), barH,
                   JUtility::TColor(240, 212, 128, A(235)), JUtility::TColor(196, 150, 70, A(235)));
    }
    if (live > 0.0f) {
        const f32 fw = barW * live;
        fill_vgrad(barX, barY, fw, barH,
                   JUtility::TColor(182, 34, 38, A(250)), JUtility::TColor(108, 12, 18, A(250)));
        fill_vgrad(barX, barY, fw, 1.0f,
                   JUtility::TColor(255, 128, 116, A(110)), JUtility::TColor(255, 128, 116, A(40)));
        const f32 glowW = fw < 10.0f ? fw : 10.0f;
        fill_hgrad(barX + fw - glowW, barY, glowW, barH,
                   JUtility::TColor(255, 170, 160, A(0)), JUtility::TColor(255, 196, 186, A(190)));
        J2DFillBox(barX + fw - 1.5f, barY, 1.5f, barH, JUtility::TColor(255, 214, 204, A(235)));
    }

    fill_vgrad(barX, lineY, barW, lineH,
               JUtility::TColor(236, 230, 196, A(235)), JUtility::TColor(128, 118, 76, A(235)));

    const f32 leftW = 64.0f * kOrnScale;
    const f32 leftH = 32.0f * kOrnScale;
    draw_er_picture(s_erLeftPic, barX - 34.0f * kOrnScale, lineCenterY - kTexLineCenterY * kOrnScale,
                    leftW, leftH, A(255));
    const f32 rightW = 32.0f * kOrnScale;
    const f32 rightH = 32.0f * kOrnScale;
    draw_er_picture(s_erRightPic, barX + barW - 19.0f * kOrnScale + 2.0f,
                    lineCenterY - kTexLineCenterY * kOrnScale, rightW, rightH, A(255));

    char nm[64];
    copy_boss_name(elden_ring_title(label), nm, sizeof(nm), false);
    draw_text_soft(nm, barX + 4.0f, barY - 7.0f, 11.5f, 13.5f,
                   JUtility::TColor(240, 234, 218, A(255)), JUtility::TColor(206, 198, 180, A(255)),
                   JUtility::TColor(0, 0, 0, A(150)), 1.0f);

    qa_hud_scale_end();
}

static void draw_boss_bar_core(f32 a, const char* label, f32 live, f32 chip) {
    if (a < 0.01f) return;
    if (a > 1.0f) a = 1.0f;

    if (g_configBossBarStyle == kBossBarStyleEldenRing) {
        live = clampf(live, 0.0f, 1.0f);
        chip = clampf(chip < live ? live : chip, 0.0f, 1.0f);
        draw_boss_bar_elden_ring(a, label, live, chip, boss_bar_screen());
        return;
    }

    f32 minX = mDoGph_gInf_c::getMinXF();
    f32 maxX = mDoGph_gInf_c::getMaxXF();
    if (maxX <= minX + 1.0f) { minX = 0.0f; maxX = 640.0f; }
    f32 sMin = mDoGph_gInf_c::getSafeMinXF();
    f32 sMax = mDoGph_gInf_c::getSafeMaxXF();
    f32 usableW = (sMax > sMin + 1.0f) ? (sMax - sMin) : (maxX - minX);
    f32 centreX = (minX + maxX) * 0.5f;
    f32 topY = mDoGph_gInf_c::getMinYF();
    if (topY < 0.0f || topY > 200.0f) topY = 0.0f;

    f32 barW = usableW * 0.30f;
    if (barW > 440.0f) barW = 440.0f;
    if (barW < 280.0f) barW = 280.0f;
    const f32 barH = 10.0f;
    const f32 barX = centreX - barW * 0.5f + g_configBossBarX;
    const f32 barY = topY + 41.0f + g_configBossBarY + boss_bar_twilight_hd_shift(topY + 41.0f + g_configBossBarY);

    constexpr f32 kBossBarScaleAnchorBlendX = 1.0f;
    constexpr f32 kBossBarScaleAnchorBlendY = 1.0f;
    qa_hud_scale_begin(centreX + kBossBarScaleAnchorBlendX * ((barX + barW * 0.5f) - centreX),
                       topY + kBossBarScaleAnchorBlendY * (barY - topY));

    auto A = [a](u8 base) -> u8 { return static_cast<u8>(static_cast<f32>(base) * a); };

    if (live < 0.0f) live = 0.0f;
    if (live > 1.0f) live = 1.0f;
    if (chip < live) chip = live;
    if (chip > 1.0f) chip = 1.0f;

    u8 r, g, b;
    if (USE_STATIC_COLOR) {
        r = 210; g = 24; b = 40;
    } else if (live > 0.5f) {
        f32 f = (live - 0.5f) * 2.0f;
        r = static_cast<u8>((1.0f - f) * 214.0f + f * 46.0f);
        g = static_cast<u8>(f * 194.0f + (1.0f - f) * 168.0f);
        b = static_cast<u8>(f * 74.0f + (1.0f - f) * 46.0f);
    } else {
        f32 f = live * 2.0f;
        r = static_cast<u8>((1.0f - f) * 206.0f + f * 214.0f);
        g = static_cast<u8>(f * 158.0f + 22.0f);
        b = static_cast<u8>((1.0f - f) * 42.0f + f * 46.0f);
    }
    auto lift  = [](u8 c, f32 t) -> u8 { f32 v = c + (255.0f - c) * t; return static_cast<u8>(v > 255.0f ? 255.0f : v); };
    auto shade = [](u8 c, f32 t) -> u8 { return static_cast<u8>(static_cast<f32>(c) * t); };
    const JUtility::TColor hpTop(lift(r, 0.40f), lift(g, 0.40f), lift(b, 0.40f), A(245));
    const JUtility::TColor hpMid(r, g, b, A(245));
    const JUtility::TColor hpBot(shade(r, 0.52f), shade(g, 0.52f), shade(b, 0.52f), A(245));

    const f32 fx = barX - 4.0f, fy = barY - 4.0f, fW = barW + 8.0f, fH = barH + 8.0f;
    draw_rail(fx + 2.5f,       fy,            fW - 5.0f, 4.0f,       a);
    draw_rail(fx + 2.5f,       fy + fH - 4.0f, fW - 5.0f, 4.0f,      a);
    draw_rail(fx,              fy + 2.5f,     4.0f, fH - 5.0f,       a);
    draw_rail(fx + fW - 4.0f,  fy + 2.5f,     4.0f, fH - 5.0f,       a);

    fill_vgrad(barX - 1.0f, barY - 1.0f, barW + 2.0f, barH + 2.0f,
               JUtility::TColor(3, 2, 2, A(245)), JUtility::TColor(3, 2, 2, A(245)));
    fill_vgrad(barX, barY, barW, barH,
               JUtility::TColor(31, 27, 35, A(240)), JUtility::TColor(9, 8, 13, A(240)));
    fill_vgrad(barX, barY, barW, 2.0f, JUtility::TColor(0, 0, 0, A(115)), JUtility::TColor(0, 0, 0, A(0)));

    if (chip > live + 0.001f) {
        fill_vgrad(barX + barW * live, barY, barW * (chip - live), barH,
                   JUtility::TColor(236, 226, 200, A(150)), JUtility::TColor(188, 162, 118, A(85)));
    }

    if (live > 0.0f) {
        f32 fwid = barW * live;
        fill_vgrad(barX, barY,             fwid, barH * 0.5f, hpTop, hpMid);
        fill_vgrad(barX, barY + barH * 0.5f, fwid, barH * 0.5f, hpMid, hpBot);
        fill_vgrad(barX, barY, fwid, barH * 0.5f,
                   JUtility::TColor(255, 255, 255, A(85)), JUtility::TColor(255, 255, 255, A(0)));
        if (live < 0.999f) {
            J2DFillBox(barX + fwid - 1.0f, barY, 1.6f, barH, JUtility::TColor(255, 244, 214, A(170)));
        }
    }

    for (int i = 1; i < 4; i++) {
        f32 tx = barX + barW * (static_cast<f32>(i) / 4.0f);
        fill_vgrad(tx, barY + 2.0f, 1.0f, barH - 4.0f,
                   JUtility::TColor(0, 0, 0, A(70)), JUtility::TColor(0, 0, 0, A(30)));
    }

    const char* nm0 = label;
    if (!nm0) nm0 = "Boss";
    char nmBuf[48];
    {
        size_t i = 0;
        for (; nm0[i] != '\0' && i < sizeof(nmBuf) - 1; i++) {
            char c = nm0[i];
            nmBuf[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
        }
        nmBuf[i] = '\0';
    }
    const char* nm = nmBuf;
    const f32 fw = 16.0f;
    const f32 fh = 18.5f;
    const f32 kSmallCap = 0.72f;
    const f32 fwS = fw * kSmallCap;
    const f32 fhS = fh * kSmallCap;
    auto is_big = [nm](size_t i) { return nm[i] == ' ' || i == 0 || nm[i - 1] == ' '; };

    f32 ty = barY - 4.0f - fh * 0.2f;

    f32 tw = 0.0f;
    for (size_t i = 0; nm[i] != '\0'; i++) {
        char one[2] = { nm[i], '\0' };
        tw += get_text_width_ingame(one, is_big(i) ? fw : fwS);
    }
    f32 tx = barX + barW * 0.5f - tw * 0.5f;

    for (size_t i = 0; nm[i] != '\0';) {
        bool big = is_big(i);
        size_t j = i;
        while (nm[j] != '\0' && is_big(j) == big) j++;
        char run[48];
        size_t len = (j - i < sizeof(run) - 1) ? (j - i) : sizeof(run) - 1;
        std::memcpy(run, nm + i, len);
        run[len] = '\0';
        draw_text_ingame(run, tx, ty, big ? fw : fwS, big ? fh : fhS,
                         JUtility::TColor(255, 246, 224, A(255)));
        tx += get_text_width_ingame(run, big ? fw : fwS);
        i = j;
    }

    draw_bar_endcaps(barX, barW, barY, barH, a);
    qa_hud_scale_end();
}

static void draw_boss_bar() {
    if (!s_boss.valid) return;
    const char* label = s_boss.label ? s_boss.label : (s_boss.miniboss ? "Miniboss" : "Boss");
    draw_boss_bar_core(s_boss.alpha, label, s_boss.shownRatio, s_boss.displayRatio);
}

static void draw_boss_bar_preview() {
    draw_boss_bar_core(1.0f, "Boss", 1.0f, 1.0f);
}

static void on_meter2_draw_post(ModContext*, void* args, void*, void*) {
    s_meter2 = args ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;

    if (s_bossBarPreviewFrames > 0) {
        draw_boss_bar_preview();
        return;
    }

    if (!g_configBossBarEnabled) return;
    if (!s_boss.valid) return;

    if (dComIfGp_isPauseFlag()) return;

    draw_boss_bar();
}

ModResult init_boss_bar(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_OK;
    return mods::hook::add_post<BossBarMeter2DrawHook>(hook_svc, on_meter2_draw_post);
}

void shutdown_boss_bar() {
    free_er_ornaments();
    reset_state();
    diababa_reset();
    s_diaActiveId = 0;
    morpheel_reset();
    s_obActiveId = 0;
    stallord_reset();
    s_dsActiveId = 0;
    blizzeta_reset();
    s_yoActiveId = 0;
    darknut_reset();
    s_tnActiveId = 0;
    argorok_reset();
    s_drActiveId = 0;
    zant_reset();
    s_znActiveId = 0;
    ganondorf_reset();
    s_gndActiveId = 0;
    s_defeated.clear();
    s_cands.clear();
}
