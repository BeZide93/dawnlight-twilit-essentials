#include "boss_rush.hpp"
#include "boss_rush_midna.hpp"
#include "boss_rush_collection.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush_models.hpp"
#include "boss_rush_masterswd.hpp"
#include "boss_rush_texts.hpp"
#include "boss_rush_equipment.hpp"
#include "boss_rush_timer.hpp"
#include "boss_rush_save.hpp"
#include "ganondorf_cape.hpp"
#include "../util.hpp"
#include "../boss_bar/boss_bar.hpp"
#include "../boss_bar/boss_internals.hpp"
#include "../general/human_warp.hpp"
#include <collection_lib/collection_lib.hpp>

#include "mods/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/flow.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"
#include "mods/svc/config.h"
#include "mods/svc/save.h"
#include "mods/svc/actor.h"

extern const ActorService* svc_actor;
extern const SaveService* svc_save;

#include "c/c_damagereaction.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_spinner.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_s_play.h"
#include "d/d_msg_object.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "d/d_event.h"
#include "d/d_demo.h"
#include "d/actor/d_a_door_shutter.h"
#include "d/actor/d_a_door_bossL1.h"
#include "d/actor/d_a_obj_lv4EdShutter.h"
#include "d/actor/d_a_obj_lv4PoGate.h"
#include "d/actor/d_a_midna.h"
#include "d/actor/d_a_obj_gb.h"
#include "d/actor/d_a_e_md.h"
#include "d/actor/d_a_e_vt.h"
#include "d/actor/d_a_obj_lv4sand.h"
#include "d/actor/d_a_obj_lv4RailWall.h"
#include "d/actor/d_a_obj_lv4bridge.h"
#include "d/actor/d_a_obj_swspinner.h"
#include "d/actor/d_a_obj_msima.h"
#include "d/actor/d_a_mant.h"
#include "f_op/f_op_camera_mng.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_manager.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "JSystem/JUtility/JUTFader.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_lib.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "m_Do/m_Do_audio.h"
#include "dusk/config_var.hpp"
#include "../general/faster_transitions.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

static const BossPartAttachment g_darknutArmorParts[] = {
    {"tn_armor_arm_l.bmd",      8,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_arm_r.bmd",      14, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_chest_b.bmd",    3,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_chest_f.bmd",    3,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_head_b.bmd",     5,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_head_f.bmd",     5,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_shoulder_l.bmd", 11, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_shoulder_r.bmd", 17, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_waist_b.bmd",    26, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_waist_f.bmd",    25, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_waist_l.bmd",    27, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_armor_waist_r.bmd",    28, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_shield.bmd",           9,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_sword_a.bmd",          15, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_sword_b_saya.bmd",     27, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"tn_sword_b.bmd",          -1, 14, {0.0f, 0.0f, 0.0f}, {0, 0, 0}, "B_tn", "tnb_sword_b_pull_a.bck", 0.0f},
};
static constexpr u8 kDarknutArmorPartCount =
    static_cast<u8>(sizeof(g_darknutArmorParts) / sizeof(g_darknutArmorParts[0]));

static const BossPartAttachment g_dangoroParts[] = {
    {"mg_met.bmd", 23, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
};
static constexpr u8 kDangoroPartCount =
    static_cast<u8>(sizeof(g_dangoroParts) / sizeof(g_dangoroParts[0]));

static const BossPartAttachment g_fyrusParts[] = {
    {"fm_core.bmd", 3, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}, nullptr, nullptr, 0.0f, nullptr, "core_beat.btk"},
};
static constexpr u8 kFyrusPartCount =
    static_cast<u8>(sizeof(g_fyrusParts) / sizeof(g_fyrusParts[0]));

static const BossPartAttachment g_ganondorfParts[] = {
    {"egnd_sword.bmd", 33, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
};
static constexpr u8 kGanondorfPartCount =
    static_cast<u8>(sizeof(g_ganondorfParts) / sizeof(g_ganondorfParts[0]));

static const BossPartAttachment g_aeralfosParts[] = {
    {"gg_met.bmd",    5,  -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"gg_shield.bmd", 11, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
    {"gg_sword.bmd",  16, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
};
static constexpr u8 kAeralfosPartCount =
    static_cast<u8>(sizeof(g_aeralfosParts) / sizeof(g_aeralfosParts[0]));

static const BossPartAttachment g_morpheelParts[] = {
    {"oh_core.bmd", 0, -1, {0.0f, 0.0f, 520.0f}, {0, 0, 0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd",  8, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd",  9, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 10, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 11, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 12, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 13, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 14, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
    {"oh.bmd", 15, -1, {0,0,0}, {0,0,0}, nullptr, nullptr, 0.0f, "oh_loop.brk", "oh_loop.btk"},
};
static constexpr u8 kMorpheelPartCount =
    static_cast<u8>(sizeof(g_morpheelParts) / sizeof(g_morpheelParts[0]));

static const BossPartAttachment g_deathSwordParts[] = {
    {"va_weapon.bmd", 23, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}, nullptr, nullptr, 0.0f, "va_weapon.brk", nullptr},
};
static constexpr u8 kDeathSwordPartCount =
    static_cast<u8>(sizeof(g_deathSwordParts) / sizeof(g_deathSwordParts[0]));

static const BossPartAttachment g_blizzetaParts[] = {
    {"ykw_b.bmd", 0, -1, {0.0f, 580.0f, 0.0f}, {0, 0, 0}, "B_yo", "ykw_b_float.bck", 0.0f, "ykw_b_angry.brk", "ykw_b_float.btk"},
};
static constexpr u8 kBlizzetaPartCount =
    static_cast<u8>(sizeof(g_blizzetaParts) / sizeof(g_blizzetaParts[0]));

static const BossPartAttachment g_puppetZeldaParts[] = {
    {"hzelda_sword.bmd", 28, -1, {0.0f, 0.0f, 0.0f}, {0, 0, 0}},
};
static constexpr u8 kPuppetZeldaPartCount =
    static_cast<u8>(sizeof(g_puppetZeldaParts) / sizeof(g_puppetZeldaParts[0]));

static const cXyz kDiababaFightSpawnPos{4.17f, 5.41f, 2662.34f};

static const cXyz kFyrusFightSpawnPos{-1.0f, 0.0f, 1473.0f};

static const cXyz kDangoroFightSpawnPos{21.83f, 879.22f, 817.38f};

static const cXyz kMorpheelFightSpawnPos{-1193.0f, -24000.0f, -770.0f};
static const s16 kMorpheelFightAngle = static_cast<s16>(0x2A02);

static const cXyz kDeathSwordFightSpawnPos{270.0f, 0.0f, 210.0f};

static const cXyz kStallordFightSpawnPos{-60.0f, 1775.0f, 4449.0f};

static const cXyz kBlizzetaFightSpawnPos{-200.0f, 2.0f, 580.0f};

static const cXyz kDarknutFightSpawnPos{150.0f, -350.0f, 600.0f};

static const cXyz kZantFightSpawnPos{0.0f, 0.0f, 0.0f};

static const cXyz kArmogohmaFightSpawnPos{0.0f, 0.0f, 2391.84f};

static const cXyz kBeastGanonFightSpawnPos{0.0f, 0.0f, -2890.0f};

static const cXyz kGanondorfFightSpawnPos{600.0f, 1100.0f, 0.0f};
static const s16 kGanondorfFightAngle = static_cast<s16>(-0x4000);

const BossGalleryEntry g_bossGalleryTable[] = {
    {"Ook",          "Forest Temple",       "E_mk",   "mk.bmd",     nullptr,  "mk_wait.bck",       "D_MN05B", 0, 51, 0, 1.2f,  0.0f,  260.0f},
    {"Diababa",      "Forest Temple",       "B_bq",   "bq.bmd",     nullptr,  "bq_wait01.bck",     "D_MN05A", 0, 50, 0, 0.275f, 0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
     &kDiababaFightSpawnPos, static_cast<s16>(0x8000)},
    {"Dangoro",      "Goron Mines",         "E_gob",  "mg.bmd",     nullptr,  "mg_wait.bck",       "D_MN04B", 3, 51, 0, 0.9f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_dangoroParts, kDangoroPartCount, nullptr, nullptr, nullptr, nullptr,
     &kDangoroFightSpawnPos, static_cast<s16>(0x8000)},
    {"Fyrus",        "Goron Mines",         "E_fm",   "fm.bmd",     nullptr,  "fm_wait01.bck",     "D_MN04A", 0, 50, 0, 0.5f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_fyrusParts, kFyrusPartCount, nullptr, "fm.brk", nullptr, "fm.btk",
     &kFyrusFightSpawnPos, static_cast<s16>(0x8000)},
    {"Deku Toad",    "Lakebed Temple",      "E_dt",   "dt.bmd",     nullptr,  "dt_wait01.bck",     "D_MN01B", 0, 51, 0, 0.55f,  0.0f,  260.0f, 150.0f},
    {"Morpheel",     "Lakebed Temple",      "B_oh",   "oi_head.bmd",nullptr,  "",                  "D_MN01A", 0, 50, 0, 0.275f, -165.0f, 240.0f, 60.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_morpheelParts, kMorpheelPartCount,
     nullptr, nullptr, nullptr, nullptr, &kMorpheelFightSpawnPos, kMorpheelFightAngle,
     csXyz(static_cast<s16>(-0x4000), 0, 0)},
    {"Death Sword",  "Arbiter's Grounds",   "E_va",   "va.bmd",     nullptr,  "va_subs_wait.bck",  "D_MN10B", 0, 51, 0, 0.7f,  0.0f, 260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_deathSwordParts, kDeathSwordPartCount,
     nullptr, nullptr, nullptr, nullptr, &kDeathSwordFightSpawnPos, static_cast<s16>(-0x6000)},
    {"Stallord",     "Arbiter's Grounds",   "B_ds",   "ds.bmd",     nullptr,  "ds_wait01_a.bck",   "D_MN10A", 0, 50, 0, 0.14f,  0.0f,  260.0f, 100.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
     &kStallordFightSpawnPos, static_cast<s16>(-0x8000)},
    {"Darkhammer",   "Snowpeak Ruins",      "E_th",   "th.bmd",     nullptr,  "th_wait.bck",       "D_MN11B", 0, 51, 0, 0.9f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, "E_th_ball"},
    {"Blizzeta",     "Snowpeak Ruins",      "B_yo",   "yo_core.bmd",nullptr,  "",                  "D_MN11A", 0, 50, 0, 0.3f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_blizzetaParts, kBlizzetaPartCount,
     nullptr, nullptr, nullptr, nullptr, &kBlizzetaFightSpawnPos, static_cast<s16>(0x6AAB)},
    {"Darknut",      "Temple of Time",      "B_tnp",  "tn.bmd",     "B_tn",   "tnb_wait.bck",      "D_MN06B", 0, 51, 0, 1.0f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_darknutArmorParts, kDarknutArmorPartCount,
     nullptr, nullptr, nullptr, nullptr, &kDarknutFightSpawnPos, static_cast<s16>(-0x7000)},
    {"Armogohma",    "Temple of Time",      "B_gm",   "goma.bmd",   nullptr,  "goma_wait.bck",     "D_MN06A", 0, 50, 0, 0.3f,  0.0f,  260.0f, 50.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
     &kArmogohmaFightSpawnPos, static_cast<s16>(0x8000)},
    {"Aeralfos",     "City in the Sky",     "B_gg",   "gg.bmd",     nullptr,  "ggb_wait_a.bck",    "D_MN07B", 2, 51, 0, 0.7f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_aeralfosParts, kAeralfosPartCount},
    {"Argorok",      "City in the Sky",     "B_dr",   "dr.bmd",     nullptr,  "dr_pole_stayb.bck",   "D_MN07A", 2, 50, 0, 0.275f, 75.0f, 260.0f},
    {"Zant",         "Palace of Twilight",  "B_zan",  "zan.bmd",    nullptr,  "zan_wait.bck",      "D_MN08D", 0, 53, 0, 1.0f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
     &kZantFightSpawnPos, static_cast<s16>(0)},
    {"Puppet Zelda", "Hyrule Castle",       "Hzelda", "hzelda.bmd", nullptr,  "hzelda_fwait.bck",  "D_MN09A", 0, 50, 0, 1.0f,  50.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_puppetZeldaParts, kPuppetZeldaPartCount},
    {"Beast Ganon",      "Hyrule Castle",       "B_mgn",  "mgn.bmd",    nullptr,  "mgn_wait.bck",      "D_MN09A", 2, 50, 1, 0.45f,  0.0f,  260.0f, 100.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
     &kBeastGanonFightSpawnPos, static_cast<s16>(0)},
    {"Horseback Ganon",  "Hyrule Field",        "Horse",  "hs.bmd",     nullptr,  "hs_wait_01.bck",    "D_MN09B", 0,  0, 0, 0.8f,  0.0f,  260.0f, 0.0f, "HoZelda", "zelh.bmd", nullptr, "zelh_waith.bck"},
    {"Ganondorf",    "Hyrule Castle",       "B_gnd",  "egnd.bmd",   nullptr,  "egnd_wait02.bck",   "D_MN09B", 1, 0,  0, 0.8f,  0.0f,  260.0f, 0.0f,
     nullptr, nullptr, nullptr, nullptr, nullptr, g_ganondorfParts, kGanondorfPartCount, nullptr, "egnd_core_beat.brk",
     nullptr, nullptr, &kGanondorfFightSpawnPos, kGanondorfFightAngle},
};
const size_t g_bossGalleryCount = sizeof(g_bossGalleryTable) / sizeof(g_bossGalleryTable[0]);

bool g_configBossRushSuggestedItems = false;
bool g_configBossRushRefillAfterFight = false;
bool g_configBossRushSeparateGanon = false;

bool is_boss_gallery_entry_filtered(size_t tableIdx) {
    if (g_configBossRushSeparateGanon) {
        return false;
    }
    const char* name = g_bossGalleryTable[tableIdx].displayName;
    return std::strcmp(name, "Puppet Zelda") == 0 ||
           std::strcmp(name, "Beast Ganon") == 0 ||
           std::strcmp(name, "Horseback Ganon") == 0;
}

size_t boss_rush_get_active_gallery_count() {
    if (g_configBossRushSeparateGanon) {
        return g_bossGalleryCount;
    }
    size_t count = 0;
    for (size_t i = 0; i < g_bossGalleryCount; ++i) {
        if (!is_boss_gallery_entry_filtered(i)) {
            ++count;
        }
    }
    return count;
}

size_t boss_rush_get_active_gallery_table_index(size_t circleSlot) {
    if (g_configBossRushSeparateGanon) {
        return circleSlot;
    }
    size_t activeIdx = 0;
    for (size_t i = 0; i < g_bossGalleryCount; ++i) {
        if (!is_boss_gallery_entry_filtered(i)) {
            if (activeIdx == circleSlot) {
                return i;
            }
            ++activeIdx;
        }
    }
    return 0;
}

size_t boss_rush_get_circle_slot_for_table_index(size_t tableIdx) {
    const size_t activeCount = boss_rush_get_active_gallery_count();
    for (size_t s = 0; s < activeCount; ++s) {
        if (boss_rush_get_active_gallery_table_index(s) == tableIdx) {
            return s;
        }
    }
    return 0;
}

extern u8 g_zInventorySlot;
extern u8 g_zMixSlot;

namespace {

static bool s_bossRushModeActive = false;
static ModContext* s_modCtx = nullptr;
static const LogService* s_logSvc = nullptr;

static bool s_transitionsUserSnapshot = true;
static bool s_transitionsSnapshotValid = false;
static dusk::config::ConfigVar<bool>* s_bossRushFastTransitionsVar = nullptr;

static void force_boss_rush_fast_transitions() {
    if (!s_transitionsSnapshotValid) {
        s_transitionsUserSnapshot = g_configFasterTransitions;
        s_transitionsSnapshotValid = true;
    }
    if (s_bossRushFastTransitionsVar != nullptr && !s_bossRushFastTransitionsVar->getValue()) {
        s_bossRushFastTransitionsVar->setOverrideValue(true);
    }
    if (!g_configFasterTransitions) {
        g_configFasterTransitions = true;
        faster_transitions_apply_mode();
    }
}

static void restore_boss_rush_fast_transitions() {
    if (s_bossRushFastTransitionsVar != nullptr) {
        s_bossRushFastTransitionsVar->clearOverride();
    }
    g_configFasterTransitions = s_transitionsUserSnapshot;
    faster_transitions_apply_mode();
    s_transitionsSnapshotValid = false;
}

#include <cstdarg>
static void rush_debug_logf(const char* fmt, ...) {
    if (s_logSvc == nullptr || s_modCtx == nullptr) return;
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    s_logSvc->info(s_modCtx, msg);
}

static int s_activeFightIndex = -1;
static u16 s_fightStartLife = 0;

static int s_pendingFightIndex = -1;

static bool s_rushRunActive = false;
static size_t s_rushRunOrder[kMaxBossGalleryEntries];
static size_t s_rushRunCount = 0;
static size_t s_rushRunPos = 0;

static bool s_pendingFightFromArena = false;
static int s_warpWatchdogFrames = 0;
static int s_killWatchdogFrames = 0;

static bool s_pendingWarpSawEnableNextStage = false;

static u32 s_fightWarpGen = 0;

static bool instant_fight_rearm(u32& io_gen) {
    if (io_gen == s_fightWarpGen) return false;
    io_gen = s_fightWarpGen;
    return true;
}

static constexpr int kArenaSettleFrames = 15;
static constexpr int kBossUnfreezeFrame = 15;
static int s_arenaSettle = 0;

static int arena_settle_frames_for(const BossGalleryEntry& boss) {
    if (std::strcmp(boss.displayName, "Beast Ganon") == 0) {
        return 30;
    }
    return kArenaSettleFrames;
}

static int s_arenaFreezeUntil = kBossUnfreezeFrame;

static bool s_swordDrawnLatched = false;

static bool s_pendingGearSaveApply = false;
static int s_pendingGearSaveKind = 0;  // 1 = fight restriction, 2 = chamber equips
static const BossGalleryEntry* s_pendingGearBoss = nullptr;
static bool s_retryWarpActive = false;
static bool s_gauntletLadderActive = false;
static int s_gauntletPhase = 0;

static bool s_sawSwordDrawnAtCommit = false;

static int s_morpheelPosPinFrames = 0;
static int s_morpheelCamArmFrames = 0;
static int s_horsebackGanonKoTimer = -1;
static bool s_horsebackGanonSawHorse = false;
static bool s_horsebackRetryLanding = false;
static int s_beastGanonKoTimer = -1;

static bool s_returningToChamber = false;
static bool s_returnSawFadeOut = false;

static bool s_needsChamberSpawn = false;
static int s_chamberSpawnFrames = 0;
static int s_chamberCamArmFrames = 0;

static bool s_portalArrivalAnimPending = false;

constexpr u16 kChamberFullLife = 20 * 5;

struct SavedPlayerLocation {
    char stage[8] = {0};
    s8 room = 0;
    s8 layer = 0;
    cXyz pos{0.0f, 0.0f, 0.0f};
    s16 angle = 0;
};
static SavedPlayerLocation s_savedLocation;
static bool s_hasSavedLocation = false;
static constexpr const char* kBossRushLocationBlobName = "boss_rush_return_location";

static void persist_saved_location_to_disk() {
    if (svc_save == nullptr || s_modCtx == nullptr) {
        return;
    }
    svc_save->set_blob(s_modCtx, kBossRushLocationBlobName, &s_savedLocation,
                       sizeof(s_savedLocation));
}

// Marks an in-flight boss rush session so that a mod reload while standing in
// the chamber room (which vanilla progression can also reach) does not resume
// boss rush unless a session was actually started.
static constexpr const char* kBossRushSessionBlobName = "boss_rush_session_active";

static void persist_boss_rush_session_marker() {
    if (svc_save == nullptr || s_modCtx == nullptr) {
        return;
    }
    const u32 marker = 1;
    svc_save->set_blob(s_modCtx, kBossRushSessionBlobName, &marker, sizeof(marker));
}

static bool boss_rush_session_marker_present() {
    if (svc_save == nullptr || s_modCtx == nullptr) {
        return false;
    }
    u32 marker = 0;
    size_t size = sizeof(marker);
    return svc_save->get_blob(s_modCtx, kBossRushSessionBlobName, &marker, &size) == MOD_OK &&
           size == sizeof(marker) && marker == 1;
}

static void clear_boss_rush_session_marker() {
    if (svc_save == nullptr || s_modCtx == nullptr) {
        return;
    }
    svc_save->delete_blob(s_modCtx, kBossRushSessionBlobName);
}

static bool s_chamberEquipsPending = false;
static int  s_chamberEquipsFrames  = 0;

// Set when the entry warp ends; the custom equip suppression must only land
// once the transition fader is fully black, or the model visibly pops from the
// custom tunic to the hero tunic in the last frames of the warp cinematic.
static bool s_equipSuppressWaitBlack = false;
static int  s_equipSuppressWaitBlackFrames = 0;

static bool s_exitSaveReloadPending = false;
static bool s_exitCardLoadArmed = false;
static bool s_exitCardDataReady = false;
static int  s_exitSaveReloadFrames = 0;
constexpr int kExitSaveReloadTimeoutFrames = 600;
static u8 s_exitCardBuf[QUEST_LOG_SIZE * 3];
static bool s_pendingInitialInventory = false;
static bool s_exitingBossRush = false;
static int s_chamberCleanupFrames = 0;
constexpr int kChamberCleanupWindow = 180;

static bool s_dungeonClearWarpPending = false;
static int  s_dungeonClearWarpFrames  = 0;
static bool s_exitWarpViaDissolve = false;

DEFINE_HOOK(&dStage_changeScene, BossRushChangeSceneHook);
DEFINE_HOOK(&dStage_changeScene, BossRushChangeSceneFightGuardHook);
DEFINE_HOOK(&dStage_changeScene4Event, BossRushChangeScene4EventHook);
DEFINE_HOOK(&daDoor20_c::openInit, BossRushDoorOpenHook);
DEFINE_HOOK(&daAlink_c::dungeonReturnWarp, BossRushDungeonReturnWarp);
DEFINE_HOOK(&daAlink_c::skipPortalObjWarp, BossRushSkipPortalObjWarp);

DEFINE_HOOK(&daAlink_c::procCoDead, BossRushProcCoDeadHook);

DEFINE_HOOK(&daBdoorL1_c::execute, BossRushBossDoorExecuteHook);

DEFINE_HOOK(&daAlink_c::draw, BossRushDrawHook);

DEFINE_HOOK(&daAlink_c::execute, BossRushAlinkExecuteHook);

DEFINE_HOOK(&dMeter2Draw_c::draw, BossRushMeterDrawHook);

DEFINE_HOOK(&dSv_memBit_c::isDungeonItem, BossRushDefeatOverrideHook);

static bool is_in_chamber_room() {
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr && std::strcmp(stage, kBossRushChamberStage) == 0 &&
           dComIfGp_roomControl_getStayNo() == kBossRushChamberRoom;
}

static camera_process_class* boss_rush_get_active_player_camera() {
    return dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
}

static int boss_rush_target_index() {
    if (s_activeFightIndex >= 0 && static_cast<size_t>(s_activeFightIndex) < g_bossGalleryCount) {
        return s_activeFightIndex;
    }
    if (s_pendingFightIndex >= 0 && static_cast<size_t>(s_pendingFightIndex) < g_bossGalleryCount) {
        return s_pendingFightIndex;
    }
    return -1;
}

static bool boss_rush_room_matches_target(const BossGalleryEntry& target, s8 curRoom) {
    if (curRoom == target.room || curRoom < 0) {
        return true;
    }
    if (std::strcmp(target.stage, "D_MN09A") == 0 && (curRoom == 50 || curRoom == 51)) {
        return true;
    }
    if (std::strcmp(target.stage, "D_MN08D") == 0) {
        static const s8 kZantRooms[] = {50, 53, 54, 55, 56, 57, 60};
        for (s8 r : kZantRooms) {
            if (curRoom == r) return true;
        }
    }
    return false;
}

static void boss_rush_screen_fade_out(f32 speed) {
    if (mDoGph_gInf_c::isFade() != 0) {
        if (mDoGph_gInf_c::getFadeRate() >= 1.0f) return;
        if (mDoGph_gInf_c::getFadeSpeed() > 0.0f) return;
    }
    mDoGph_gInf_c::fadeOut(speed, g_blackColor);
}

static void boss_rush_screen_hold_black() {
    mDoGph_gInf_c::fadeOut(0.0f, g_blackColor);
    mDoGph_gInf_c::setFadeRate(1.0f);
}

static bool boss_rush_screen_is_fully_black() {
    return (mDoGph_gInf_c::isFade() != 0 && mDoGph_gInf_c::getFadeRate() >= 1.0f);
}

static void boss_rush_screen_fade_in(f32 speed) {
    mDoGph_gInf_c::fadeIn(speed, g_blackColor);
}

}

bool boss_rush_is_fighting_here() {
    const int idx = boss_rush_target_index();
    if (idx < 0) {
        return false;
    }
    const char* curStage = dComIfGp_getStartStageName();
    if (curStage == nullptr) {
        return false;
    }
    const s8 curRoom = static_cast<s8>(dComIfGp_roomControl_getStayNo());
    const auto& target = g_bossGalleryTable[idx];

    if (!g_configBossRushSeparateGanon && s_gauntletLadderActive &&
        (std::strcmp(target.displayName, "Ganondorf") == 0 ||
         std::strcmp(target.displayName, "Puppet Zelda") == 0 ||
         std::strcmp(target.displayName, "Beast Ganon") == 0 ||
         std::strcmp(target.displayName, "Horseback Ganon") == 0)) {
        return is_boss_rush_ganon_stage(curStage);
    }

    if (std::strcmp(curStage, target.stage) != 0) {
        return false;
    }

    bool roomMatches = boss_rush_room_matches_target(target, curRoom);
    if (!roomMatches) {
        return false;
    }

    if (is_in_chamber_room()) {
        if (s_activeFightIndex >= 0 || s_pendingFightFromArena) {
            return true;
        }
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        return (faderStatus == JUTFader::FadeOut || faderStatus == JUTFader::None);
    }

    return true;
}

bool boss_rush_is_returning_to_chamber() {
    return s_returningToChamber;
}

const char* boss_rush_current_target_name() {
    const int idx = boss_rush_target_index();
    if (idx < 0 || static_cast<size_t>(idx) >= g_bossGalleryCount) {
        return nullptr;
    }
    return g_bossGalleryTable[idx].displayName;
}

int boss_rush_current_target_index() {
    const int idx = boss_rush_target_index();
    if (idx < 0 || static_cast<size_t>(idx) >= g_bossGalleryCount) {
        return -1;
    }
    return idx;
}

bool boss_rush_is_fight_retry_warp() {
    return s_pendingFightFromArena && s_pendingFightIndex != -1;
}

static bool is_boss_rush_mod_warp_in_flight() {
    if (s_pendingFightIndex != -1 || s_returningToChamber || s_exitingBossRush ||
        s_dungeonClearWarpPending) {
        return true;
    }
    return dComIfGp_isEnableNextStage() || fopOvlpM_IsPeek();
}

unsigned int boss_rush_debug_transition_bits() {
    u32 bits = 0;
    if (s_pendingFightIndex != -1) bits |= 1u << 0;
    if (s_returningToChamber) bits |= 1u << 1;
    if (s_exitingBossRush) bits |= 1u << 2;
    if (s_dungeonClearWarpPending) bits |= 1u << 3;
    if (dComIfGp_isEnableNextStage()) bits |= 1u << 4;
    if (fopOvlpM_IsPeek()) bits |= 1u << 5;
    JUTFader* fader = mDoGph_gInf_c::getFader();
    if (fader != nullptr) {
        const s32 status = fader->getStatus();
        if (status == JUTFader::FadeOut) bits |= 1u << 6;
        if (status == JUTFader::None) bits |= 1u << 7;
        bits |= (static_cast<u32>(status) & 0xFFu) << 8;
    }
    return bits;
}

bool is_boss_rush_transition_in_flight() {
    if (is_boss_rush_mod_warp_in_flight()) {
        return true;
    }
    JUTFader* fader = mDoGph_gInf_c::getFader();
    if (fader != nullptr) {
        const s32 status = fader->getStatus();
        if (status == JUTFader::FadeOut || status == JUTFader::None) {
            return true;
        }
    }
    return false;
}

namespace {

static void reset_boss_rush_save_flags() {
    for (int s = 0; s < dSv_save_c::STAGE_MAX; ++s) {
        auto& bit = g_dComIfG_gameInfo.info.getSavedata().getSave(s).getBit();
        bit.offStageBossEnemy();
        bit.offStageBossEnemy2();
        bit.offStageBossDemo();
        bit.init();
    }
    auto& memBit = g_dComIfG_gameInfo.info.getMemory().getBit();
    memBit.offStageBossEnemy();
    memBit.offStageBossEnemy2();
    memBit.offStageBossDemo();
    memBit.init();
    g_dComIfG_gameInfo.info.resetDan();

    for (int z = 0; z < dSv_info_c::ZONE_MAX; ++z) {
        g_dComIfG_gameInfo.info.getZone(z).getBit().init();
        g_dComIfG_gameInfo.info.getZone(z).getActor().init();
        g_dComIfG_gameInfo.info.getZone(z).reset();
    }
}

static HookAction on_defeat_check_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;

    int bit = mods::arg<int>(args, 1);
    if (bit != dSv_memBit_c::STAGE_BOSS_ENEMY &&
        bit != dSv_memBit_c::STAGE_BOSS_ENEMY_2 &&
        bit != dSv_memBit_c::STAGE_BOSS_DEMO) {
        return HOOK_CONTINUE;
    }

    if (boss_rush_is_fighting_here() && !s_returningToChamber) {
        if (bit == dSv_memBit_c::STAGE_BOSS_DEMO) {
            *static_cast<s32*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        *static_cast<s32*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (is_in_chamber_room()) {
        *static_cast<s32*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

static bool s_portalCineWasHuman = false;

static int s_bmgOpeningSkipWait = 0;

static void update_beastganon_instant_fight(bool gameFrameTick);

static void update_beastganon_instant_fight(bool gameFrameTick) {
    static u32 s_gen = ~0u;
    if (instant_fight_rearm(s_gen)) {
        s_bmgOpeningSkipWait = 0;
    }

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN09A") != 0) {
        s_bmgOpeningSkipWait = 0;
        return;
    }
    if (s_returningToChamber) return;

    const int t = boss_rush_target_index();
    bool isBeastFight = false;
    if (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount) {
        if (std::strcmp(g_bossGalleryTable[t].displayName, "Beast Ganon") == 0) {
            isBeastFight = true;
        } else if (!g_configBossRushSeparateGanon &&
                   std::strcmp(g_bossGalleryTable[t].displayName, "Ganondorf") == 0 &&
                   fopAcM_SearchByName(fpcNm_B_MGN_e) != nullptr) {
            isBeastFight = true;
        }
    }
    if (!isBeastFight) {
        return;
    }

    fopAc_ac_c* mgnBase = fopAcM_SearchByName(fpcNm_B_MGN_e);
    if (mgnBase == nullptr) return;
    if (!bbi::beastganon_model_ready(mgnBase)) return;
    daB_MGN_c* mgn = reinterpret_cast<daB_MGN_c*>(mgnBase);

    if (!fopAcM_CheckStatus(mgnBase, fopAcStts_BOSS_e)) {
        fopAcM_OnStatus(mgnBase, fopAcStts_BOSS_e);
    }

    if (mgn->field_0xaff >= 6 || mgn->mActionMode == daB_MGN_c::ACTION_DEATH_e) {
        if (mgnBase->health > 0) mgnBase->health = 0;
    } else if (mgn->field_0xaff >= 4) {
        if (mgnBase->health > 300) mgnBase->health = 300;
    } else if (mgn->field_0xaff >= 2) {
        if (mgnBase->health > 500) mgnBase->health = 500;
    }

    if (mgnBase->health <= 0 && mgn->mActionMode == daB_MGN_c::ACTION_DEATH_e) return;

    if (mgn->mActionMode == daB_MGN_c::ACTION_OPENING_e) {
        mgn->current.angle.set(0, (s16)0x8000, 0);
        mgn->shape_angle.set(0, (s16)0x8000, 0);
        mgn->field_0xb14 = (s16)0x8000;
        mgn->field_0xb16 = (s16)0x8000;
        mgn->field_0xb18 = 0;

        daAlink_c* linkWait = daAlink_getAlinkActorClass();
        if (linkWait != nullptr) {
            linkWait->current.pos = kBeastGanonFightSpawnPos;
            linkWait->old.pos = kBeastGanonFightSpawnPos;
            linkWait->current.angle.set(0, 0, 0);
            linkWait->shape_angle.set(0, 0, 0);
            linkWait->speed.set(0.0f, 0.0f, 0.0f);
            linkWait->speedF = 0.0f;
        }

        const bool eventLive = dComIfGp_event_runCheck() != 0;
        if (eventLive || ++s_bmgOpeningSkipWait >= 90) {
            mgn->demo_skip(0);
            mgn->current.pos.set(0.0f, 0.0f, 90.0f);
            mgn->current.angle.set(0, (s16)0x8000, 0);
            mgn->shape_angle.set(0, (s16)0x8000, 0);
            mgn->field_0xb14 = (s16)0x8000;
            mgn->field_0xb16 = (s16)0x8000;
            mgn->field_0xb18 = 0;
            mgn->mtx_set();

            mgn->mDemoCamCenter.set(0.0f, -180.0f, -2090.0f);
            mgn->mDemoCamEye.set(0.0f, -200.0f, -3090.0f);
            camera_process_class* cam = boss_rush_get_active_player_camera();
            if (cam != nullptr) {
                cam->mCamera.Reset(mgn->mDemoCamCenter, mgn->mDemoCamEye);
                cam->mCamera.Start();
                cam->mCamera.SetTrimSize(0);
            }

            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) {
                link->current.pos = kBeastGanonFightSpawnPos;
                link->old.pos = kBeastGanonFightSpawnPos;
                link->current.angle.set(0, 0, 0);
                link->shape_angle.set(0, 0, 0);
                link->speed.set(0.0f, 0.0f, 0.0f);
                link->speedF = 0.0f;
            }
        }
        return;
    }
    s_bmgOpeningSkipWait = 0;

    if (mgn->mActionMode == daB_MGN_c::ACTION_DASH_e && mgn->mMoveMode <= 1) {
        mgn->current.angle.set(0, (s16)0x8000, 0);
        mgn->shape_angle.set(0, (s16)0x8000, 0);
        mgn->field_0xb14 = (s16)0x8000;
        mgn->field_0xb16 = (s16)0x8000;
        mgn->field_0xb18 = 0;
    }
}

// Calling Midna already freezes Beast Ganon correctly: talking to her runs
// as a proper engine event, and dComIfGp_event_moveApproval() suspends every
// actor that isn't part of that event (see f_op_actor.cpp's execute
// dispatch). A plain wolf<->human transformation is not an engine event, so
// that suspension never kicks in and Beast Ganon keeps acting while the
// player can't. Skip its own execute() outright for the duration instead -
// unlike toggling fopAcStts_NOEXEC_e, this leaves it fully drawn in place.
DEFINE_HOOK(&daB_MGN_c::execute, BossRushBeastGanonTransformFreezeHook);

static HookAction on_beastganon_execute_pre(ModContext*, void*, void* retval, void*) {
    if (!s_bossRushModeActive || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr || !link->checkMetamorphose()) {
        return HOOK_CONTINUE;
    }
    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}



DEFINE_HOOK(&cc_at_check, BossRushBeastGanonArrowHook);

static HookAction on_beastganon_arrow_hit_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;

    fopAc_ac_c* defender = mods::arg<fopAc_ac_c*>(args, 0);
    if (defender == nullptr || fopAcM_GetName(defender) != fpcNm_B_MGN_e) return HOOK_CONTINUE;

    dCcU_AtInfo* atInfo = mods::arg<dCcU_AtInfo*>(args, 1);
    if (atInfo == nullptr || atInfo->mpCollider == nullptr) return HOOK_CONTINUE;
    if (!atInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) return HOOK_CONTINUE;

    *static_cast<fopAc_ac_c**>(retval) = nullptr;
    return HOOK_SKIP_ORIGINAL;
}

static s16 s_bmgPreHealth = 0;
static int s_bmgPreAff = 0;

DEFINE_HOOK(&daB_MGN_c::damage_check, BossRushBeastGanonDamageHook);

static HookAction on_bmg_damage_pre(ModContext*, void* args, void*, void*) {
    auto* mgn = mods::arg<daB_MGN_c*>(args, 0);
    if (mgn == nullptr || !s_bossRushModeActive) return HOOK_CONTINUE;
    s_bmgPreHealth = mgn->health;
    s_bmgPreAff = mgn->field_0xaff;
    return HOOK_CONTINUE;
}

static void on_bmg_damage_post(ModContext*, void* args, void*, void*) {
    auto* mgn = mods::arg<daB_MGN_c*>(args, 0);
    if (mgn == nullptr || !s_bossRushModeActive) return;
    if (mgn->health == s_bmgPreHealth) return;

    int phaseFloor = (s_bmgPreAff < 2) ? 500 : ((s_bmgPreAff < 4) ? 300 : 0);
    if (mgn->health < phaseFloor) {
        mgn->health = phaseFloor;
    }
}

DEFINE_HOOK(&daE_MD_c::Execute, DarkhammerArmorExecuteHook);

static HookAction on_darkhammer_armor_execute_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;

    daE_MD_c* a_this = mods::arg<daE_MD_c*>(args, 0);
    if (a_this == nullptr) return HOOK_CONTINUE;

    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr && std::strcmp(stage, "D_MN11B") == 0) {
        const int t = boss_rush_target_index();
        if (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount &&
            std::strcmp(g_bossGalleryTable[t].displayName, "Darkhammer") == 0) {
            fopAcM_delete(a_this);
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

static HookAction on_bossdoor_execute_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;
    if (!boss_rush_is_fighting_here()) return HOOK_CONTINUE;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount) return HOOK_CONTINUE;
    const char* name = g_bossGalleryTable[t].displayName;
    if (std::strcmp(name, "Fyrus") != 0) {
        return HOOK_CONTINUE;
    }

    daBdoorL1_c* a_this = mods::arg<daBdoorL1_c*>(args, 0);
    if (a_this == nullptr) return HOOK_CONTINUE;

    if (a_this->mAction != daBdoorL1_c::ACTION_END && a_this->mAction != 0) {
        if (a_this->field_0x590 != nullptr && a_this->field_0x590->ChkUsed()) {
            dComIfG_Bgsp().Release(a_this->field_0x590);
        }
        a_this->setAction(daBdoorL1_c::ACTION_END);
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&dSv_info_c::isSwitch, BossRushSwitchOverrideHook);

static HookAction on_switch_check_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;
    if (!is_in_chamber_room()) return HOOK_CONTINUE;

    const int roomNo = mods::arg<int>(args, 2);
    if (roomNo != kBossRushChamberRoom) return HOOK_CONTINUE;

    if (boss_rush_is_fighting_here() && !s_returningToChamber) {
        *static_cast<s32*>(retval) = 0;
    } else {
        *static_cast<s32*>(retval) = 1;
    }
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&dSv_danBit_c::isSwitch, BossRushDanSwitchHook);

static HookAction on_dan_switch_check_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval) return HOOK_CONTINUE;

    const int swNo = mods::arg<int>(args, 1);
    if (swNo == 1) {
        const char* curStage = dComIfGp_getStartStageName();
        if (curStage != nullptr && std::strcmp(curStage, "D_MN09B") == 0) {
            const int targetIdx = boss_rush_target_index();
            if (targetIdx >= 0 && static_cast<size_t>(targetIdx) < g_bossGalleryCount) {
                const auto& target = g_bossGalleryTable[targetIdx];
                if (std::strcmp(target.displayName, "Ganondorf") == 0) {
                    if (!g_configBossRushSeparateGanon) {
                        return HOOK_CONTINUE;
                    }
                    *static_cast<BOOL*>(retval) = TRUE;
                    return HOOK_SKIP_ORIGINAL;
                } else if (std::strcmp(target.displayName, "Horseback Ganon") == 0) {
                    *static_cast<BOOL*>(retval) = FALSE;
                    return HOOK_SKIP_ORIGINAL;
                }
            }
        }
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&dSv_zoneBit_c::isSwitch, BossRushZoneSwitchHook);

static HookAction on_zone_switch_check_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_bossRushModeActive || !args || !retval || s_returningToChamber) return HOOK_CONTINUE;
    if (mods::arg<int>(args, 1) != 0) return HOOK_CONTINUE;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN07A") != 0) return HOOK_CONTINUE;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Argorok") != 0) {
        return HOOK_CONTINUE;
    }

    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

static void clear_all_select_items() {
    dComIfGs_setMixItemIndex(SELECT_ITEM_X, 0xFF);
    dComIfGs_setMixItemIndex(SELECT_ITEM_Y, 0xFF);
    dComIfGs_setMixItemIndex(2, 0xFF);
    dComIfGs_setMixItemIndex(3, 0xFF);
    dComIfGs_setSelectItemIndex(SELECT_ITEM_X, 0xFF);
    dComIfGs_setSelectItemIndex(SELECT_ITEM_Y, 0xFF);
    dComIfGs_setSelectItemIndex(2, 0xFF);
    dComIfGs_setSelectItemIndex(3, 0xFF);
    g_zInventorySlot = 0xFF;
    g_zMixSlot = 0xFF;
    g_dComIfG_gameInfo.play.setSelectItem(0, dItemNo_NONE_e);
    g_dComIfG_gameInfo.play.setSelectItem(1, dItemNo_NONE_e);
    g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_NONE_e);
    g_dComIfG_gameInfo.play.setSelectItem(3, dItemNo_NONE_e);
}

static void assign_select_item(int btn, u8 slotNo) {
    dComIfGs_setMixItemIndex(btn, 0xFF);
    dComIfGs_setSelectItemIndex(btn, slotNo);
}

static void apply_zant_room_suggested_items(s8 room) {
    clear_all_select_items();

    switch (room) {
    case 50:
    case 53:
        assign_select_item(SELECT_ITEM_X, SLOT_0);
        break;
    case 54:
        assign_select_item(SELECT_ITEM_X, SLOT_3);
        break;
    case 55:
        assign_select_item(SELECT_ITEM_X, SLOT_3);
        assign_select_item(SELECT_ITEM_Y, SLOT_10);
        if (daAlink_c* link = daAlink_getAlinkActorClass()) {
            if (!link->checkEquipHeavyBoots()) {
                link->setHeavyBoots(1);
            }
        }
        break;
    case 56:
        assign_select_item(SELECT_ITEM_X, SLOT_6);
        break;
    case 57:
        assign_select_item(SELECT_ITEM_X, SLOT_6);
        break;
    case 60:
        break;
    default:
        break;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        link->setSelectEquipItem(FALSE);
    }
}

static void apply_boss_suggested_items(const BossGalleryEntry& boss) {
    const char* name = boss.displayName;

    clear_all_select_items();

    dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
    dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);

    if (std::strcmp(name, "Ook") == 0) {
    } else if (std::strcmp(name, "Diababa") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_0);
    } else if (std::strcmp(name, "Dangoro") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_3);
    } else if (std::strcmp(name, "Fyrus") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_3);
        assign_select_item(SELECT_ITEM_Y, SLOT_4);
    } else if (std::strcmp(name, "Deku Toad") == 0) {
    } else if (std::strcmp(name, "Morpheel") == 0) {
        dMeter2Info_setCloth(dItemNo_WEAR_ZORA_e, false);
        dComIfGs_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
        dComIfGp_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link) {
            s_morpheelPosPinFrames = 0;
            s_morpheelCamArmFrames = 90;
            if (dComIfGs_getSelectEquipClothes() != dItemNo_WEAR_ZORA_e) {
                link->setClothesChange(0);
            }
            link->setSelectEquipItem(FALSE);
            if (!link->checkEquipHeavyBoots()) {
                link->setHeavyBoots(1);
            }
        }
        assign_select_item(SELECT_ITEM_X, SLOT_3);
        assign_select_item(SELECT_ITEM_Y, SLOT_10);
    } else if (std::strcmp(name, "Death Sword") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_4);
        assign_select_item(SELECT_ITEM_Y, SLOT_10);
    } else if (std::strcmp(name, "Stallord") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_2);
    } else if (std::strcmp(name, "Darkhammer") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_10);
    } else if (std::strcmp(name, "Blizzeta") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_6);
    } else if (std::strcmp(name, "Darknut") == 0) {
    } else if (std::strcmp(name, "Armogohma") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_4);
        assign_select_item(SELECT_ITEM_Y, SLOT_8);
    } else if (std::strcmp(name, "Aeralfos") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_10);
    } else if (std::strcmp(name, "Argorok") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_3);
        assign_select_item(SELECT_ITEM_Y, SLOT_10);
    } else if (std::strcmp(name, "Zant") == 0) {
        apply_zant_room_suggested_items(53);
    } else if (std::strcmp(name, "Puppet Zelda") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_4);
    } else if (std::strcmp(name, "Beast Ganon") == 0) {
        assign_select_item(SELECT_ITEM_X, SLOT_4);
    }
}

static void apply_boss_rush_loadout(bool i_refreshLink = true) {
    custom_equip_set_suppressed(true);
    boss_rush_save_apply_preset(i_refreshLink);
}

}

void boss_rush_debug_log(const char* fmt, ...) {
    if (s_logSvc == nullptr || s_modCtx == nullptr) return;
    va_list ap;
    char msg[256];
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    s_logSvc->info(s_modCtx, msg);
}

static int s_recordReturnFrames = -1;
static const char* s_recordReturnReason = nullptr;

static bool s_puppetZeldaWallsTracked = false;

static void* pz_force_gb_judge(void* actor, void*) {
    if (fopAcM_GetProfName(actor) != fpcNm_OBJ_GB_e) {
        return nullptr;
    }
    obj_gb_class* gb = static_cast<obj_gb_class*>(actor);
    dComIfGs_onSwitch(gb->mSw1, fopAcM_GetRoomNo(gb));
    dComIfGs_offSwitch(gb->mSw2, fopAcM_GetRoomNo(gb));
    return nullptr;
}

static void puppet_zelda_walls_keep() {
    if (!s_puppetZeldaWallsTracked) {
        return;
    }
    fopAcIt_Judge(pz_force_gb_judge, nullptr);
}

static void puppet_zelda_walls_end_track() {
    s_puppetZeldaWallsTracked = false;
}

static void puppet_zelda_walls_begin_track() {
    s_puppetZeldaWallsTracked = true;
    puppet_zelda_walls_keep();
}

void return_to_boss_rush_chamber(const LogService* log_svc, ModContext* mod_ctx,
                                  const char* reason) {
    rush_debug_logf("[hb-dbg] return_to_boss_rush_chamber reason=%s", reason ? reason : "?");
    puppet_zelda_walls_end_track();
    if (s_rushRunActive) {
        s_rushRunActive = false;
        boss_rush_timer_end_chain_run();
    }
    if (reason != nullptr && std::strstr(reason, "defeated") != nullptr) {
        boss_rush_timer_notify_defeat();
        if (s_recordReturnReason == nullptr && s_recordReturnFrames < 0 &&
            boss_rush_timer_last_was_record()) {
            s_recordReturnReason = reason;
            s_recordReturnFrames = 60;
            return;
        }
    }
    s_recordReturnReason = nullptr;
    s_recordReturnFrames = -1;


    boss_rush_timer_reset_run();
    boss_rush_timer_end_all_phases();

    // Don't clear an already-held black screen here - a caller (e.g. the
    // Horseback Ganon defeat sequence) may have faded to black deliberately
    // before calling in; clearing it right before the stage warp briefly
    // reveals whatever scene is still rendering underneath.
    if (!boss_rush_screen_is_fully_black()) {
        mDoGph_gInf_c::offFade();
    }
    Z2GetAudioMgr()->subBgmStop();
    if (reason == nullptr || std::strcmp(reason, "Died") != 0) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_WARP_FADE, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }

    s_returningToChamber = true;
    s_returnSawFadeOut = true;
    s_needsChamberSpawn = true;
    s_chamberSpawnFrames = 0;
    s_chamberCamArmFrames = 0;
    s_retryWarpActive = false;
    s_gauntletLadderActive = false;
    s_horsebackGanonKoTimer = -1;
    s_horsebackGanonSawHorse = false;
    s_beastGanonKoTimer = -1;
    boss_bar_consume_defeat_event();
    s_activeFightIndex = -1;
    s_pendingFightIndex = -1;
    s_pendingFightFromArena = false;
    s_pendingWarpSawEnableNextStage = false;
    boss_rush_texts_reset_fade();
    boss_rush_master_sword_reset_fade();

    if (g_configBossRushRefillAfterFight) {
        s_pendingInitialInventory = true;
    }

    reset_boss_rush_save_flags();

    g_dComIfG_gameInfo.info.getDan().offSwitch(1);
    g_dComIfG_gameInfo.info.getRestart().mLastMode &= ~0xFF000000;

    s_pendingGearSaveApply = true;
    s_pendingGearSaveKind = 2;
    s_pendingGearBoss = nullptr;
    {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            link->cancelOriginalDemo();
            if (link->checkEquipHeavyBoots()) {
                link->setHeavyBoots(0);
            }
        }
    }
    cDmr_SkipInfo = 0;
    s_chamberEquipsPending = true;

    cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
    dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
    dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
    dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                          kBossRushChamberLayer, 0.0f, 0, 1, 0, cM_deg2s(180.0f), 0, 0);
}

namespace {


static bool change_scene_reloads_current_room(int exitId, s8 roomNo) {
    const char* curStage = dComIfGp_getStartStageName();
    if (curStage == nullptr) return false;

    stage_scls_info_dummy_class* scls = nullptr;
    if (roomNo >= 0) {
        dStage_roomDt_c* room = dComIfGp_roomControl_getStatusRoomDt(roomNo);
        scls = (room != nullptr) ? room->getSclsInfo() : nullptr;
    } else {
        scls = dComIfGp_getStageSclsInfo();
    }
    if (scls == nullptr || exitId < 0 || exitId >= scls->num) return false;

    const stage_scls_info_class& entry = scls->m_entries[exitId];
    if (entry.mStage[0] == '\0') return false;
    if (std::strcmp(entry.mStage, curStage) != 0) return false;

    const s8 dstRoom = (entry.mRoom == -1) ? (s8)dComIfGp_roomControl_getStayNo()
                                           : entry.mRoom;
    return dstRoom == (s8)dComIfGp_roomControl_getStayNo();
}

static int s_spuriousReloadBlockLogTimer = 0;

static bool boss_rush_should_block_spurious_reload() {
    if (!s_bossRushModeActive) return false;
    if (boss_rush_timer_all_phases_active()) return false;
    if (s_pendingFightIndex != -1 || s_returningToChamber || s_exitingBossRush ||
        s_dungeonClearWarpPending) {
        return false;
    }
    return boss_rush_current_target_index() >= 0;
}

static HookAction on_door_open_pre(ModContext*, void* args, void* retval, void*) {
    if ((!s_bossRushModeActive && !s_exitingBossRush) || !args) return HOOK_CONTINUE;
    if (!is_in_chamber_room()) return HOOK_CONTINUE;
    if (s_pendingFightIndex != -1 || s_activeFightIndex != -1) return HOOK_CONTINUE;

    ::exit_boss_rush();
    return HOOK_CONTINUE;
}

static HookAction on_change_scene_pre(ModContext*, void* args, void* retval, void*) {
    if (s_dungeonClearWarpPending && retval) {
        if (s_exitWarpViaDissolve) {
            s_dungeonClearWarpPending = false;
            s_dungeonClearWarpFrames  = 0;
            s_exitWarpViaDissolve = false;
            if (s_portalCineWasHuman) {
                human_warp_cinematic_end();
                s_portalCineWasHuman = false;
            }
            human_warp_arm_arrival_replay();
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
        s_dungeonClearWarpPending = false;
        s_dungeonClearWarpFrames  = 0;
        dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
        cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
        dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
        dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
        dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                              kBossRushChamberLayer, 0.0f, 12, 0, 0, cM_deg2s(180.0f), 1, 0);
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }

    if ((!s_bossRushModeActive && !s_exitingBossRush) || !args || !retval) return HOOK_CONTINUE;
    if (!is_in_chamber_room()) return HOOK_CONTINUE;
    if (s_pendingFightIndex != -1 || s_activeFightIndex != -1) return HOOK_CONTINUE;

    if (s_bossRushModeActive) {
        ::exit_boss_rush();
    }
    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

static HookAction on_change_scene4_event_pre(ModContext*, void* args, void* retval, void*) {
    if (args == nullptr || retval == nullptr) return HOOK_CONTINUE;
    const int exitId = mods::arg<int>(args, 0);
    const s8 roomNo = mods::arg<s8>(args, 1);

    if (boss_rush_should_block_spurious_reload() && change_scene_reloads_current_room(exitId, roomNo)) {
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static HookAction on_change_scene_fight_guard_pre(ModContext*, void* args, void* retval, void*) {
    if (args == nullptr || retval == nullptr) return HOOK_CONTINUE;
    const int exitId = mods::arg<int>(args, 0);
    const s8 roomNo = mods::arg<s8>(args, 3);

    if (boss_rush_should_block_spurious_reload() && change_scene_reloads_current_room(exitId, roomNo)) {
        *static_cast<int*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static bool is_ui_or_menu_active() {
    if (dMeter2Info_getWindowStatus() != 0) {
        return true;
    }
    if (dMeter2Info_getPauseStatus() != 0 || dComIfGp_isPauseFlag() || dScnPly_c::isPause()) {
        return true;
    }
    if (dComIfGp_event_runCheck() || dMeter2Info_isShopTalkFlag() || dMsgObject_isTalkNowCheck()) {
        return true;
    }
    JUTFader* fader = mDoGph_gInf_c::getFader();
    if (fader != nullptr) {
        const s32 faderStatus = fader->getStatus();
        if (faderStatus == JUTFader::FadeOut || faderStatus == JUTFader::None) {
            return true;
        }
    }
    return false;
}

static void on_boss_rush_draw_post_impl(ModContext*, void*, void*, void*);

static void on_boss_rush_draw_post(ModContext* ctx, void* args, void* retval, void* user) {
#ifdef _MSC_VER
    __try {
        on_boss_rush_draw_post_impl(ctx, args, retval, user);
    } __except (1) {
    }
#else
    on_boss_rush_draw_post_impl(ctx, args, retval, user);
#endif
}

static void on_boss_rush_draw_post_impl(ModContext*, void*, void*, void*) {
    if (!kBossGalleryModelsEnabled) {
        return;
    }

    if (!is_in_boss_rush_chamber()) {
        return;
    }



    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return;
    }

    draw_boss_rush_models(kBossChamberFloorY);
}

constexpr int kRingFlamesPerBoss = 8;
constexpr f32 kRingFlameScale = 0.5f;

static ActorId s_ringFlameIds[kMaxBossGalleryEntries][kRingFlamesPerBoss] = {};

static bool boss_ring_has_flames(int bossIdx) {
    for (int k = 0; k < kRingFlamesPerBoss; ++k)
        if (s_ringFlameIds[bossIdx][k] != 0) return true;
    return false;
}

static void extinguish_boss_ring(int bossIdx) {
    for (int k = 0; k < kRingFlamesPerBoss; ++k) {
        if (s_ringFlameIds[bossIdx][k] != 0) {
            if (svc_actor != nullptr) svc_actor->delete_actor(s_modCtx, s_ringFlameIds[bossIdx][k]);
            s_ringFlameIds[bossIdx][k] = 0;
        }
    }
}

static void light_boss_ring(int bossIdx) {
    if (svc_actor == nullptr) return;
    cXyz bp;
    csXyz ba;
    const size_t activeCount = boss_rush_get_active_gallery_count();
    const size_t circleSlot = boss_rush_get_circle_slot_for_table_index(static_cast<size_t>(bossIdx));
    boss_rush_get_slot_transform(circleSlot, activeCount,
                                 kBossChamberFloorY, bp, ba);
    for (int k = 0; k < kRingFlamesPerBoss; ++k) {
        if (s_ringFlameIds[bossIdx][k] != 0) continue;
        const f32 ang = (2.0f * 3.14159265f * static_cast<f32>(k)) / static_cast<f32>(kRingFlamesPerBoss);
        ActorSpawnParams sp{};
        sp.parameters = (3u << 8) | 30u;
        sp.argument = 0;
        sp.room_num = static_cast<int8_t>(kBossRushChamberRoom);
        sp.position = {bp.x + kBossInteractRadius * std::sin(ang), kBossChamberFloorY,
                       bp.z + kBossInteractRadius * std::cos(ang)};
        sp.angle = {0, 0, 0};
        sp.scale = {kRingFlameScale, kRingFlameScale, kRingFlameScale};
        ActorId id{};
        svc_actor->create_actor(s_modCtx, fpcNm_Tag_KtOnFire_e, &sp, &id);
        s_ringFlameIds[bossIdx][k] = id;
    }
}

static void clear_ring_flames() {
    for (int b = 0; b < static_cast<int>(kMaxBossGalleryEntries); ++b) extinguish_boss_ring(b);
}

static bool any_ring_flames_lit() {
    for (int b = 0; b < static_cast<int>(kMaxBossGalleryEntries); ++b)
        if (boss_ring_has_flames(b)) return true;
    return false;
}

static int s_ignitedStatue = -1;

static void update_ring_flames() {
    if (svc_actor == nullptr) return;
    const int count = static_cast<int>(g_bossGalleryCount);
    for (int b = 0; b < count; ++b) {
        if (b == s_ignitedStatue) {
            if (!boss_ring_has_flames(b)) light_boss_ring(b);
        } else if (boss_ring_has_flames(b)) {
            extinguish_boss_ring(b);
        }
    }
}

static void update_morpheel_pos_pin() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN01A") != 0) {
        if (!is_boss_rush_active() || s_returningToChamber || boss_rush_target_index() < 0) {
            s_morpheelPosPinFrames = 0;
            s_morpheelCamArmFrames = 0;
        }
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) {
        s_morpheelPosPinFrames = 0;
        s_morpheelCamArmFrames = 0;
        return;
    }
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Morpheel") != 0) {
        s_morpheelPosPinFrames = 0;
        s_morpheelCamArmFrames = 0;
        return;
    }

    static u32 s_pinGen = ~0u;
    if (instant_fight_rearm(s_pinGen)) {
        s_morpheelPosPinFrames = 0;
        s_morpheelCamArmFrames = 90;
    }

    if (s_morpheelPosPinFrames > 0) {
        --s_morpheelPosPinFrames;

        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            link->current.pos = kMorpheelFightSpawnPos;
            link->old.pos = kMorpheelFightSpawnPos;
            link->current.angle.set(0, kMorpheelFightAngle, 0);
            link->shape_angle.set(0, kMorpheelFightAngle, 0);
            link->speed.set(0.0f, 0.0f, 0.0f);
            link->speedF = 0.0f;
        }
    }

    if (s_morpheelCamArmFrames <= 0) return;
    --s_morpheelCamArmFrames;

    if (dDemo_c::m_object != nullptr && dDemo_c::m_object->mpCamera != nullptr) {
        dDemo_c::m_object->mpCamera->mFlags = 0;
    }
    if (dDemo_c::getMode() != 0) {
        dDemo_c::end();
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    camera_process_class* cam = boss_rush_get_active_player_camera();
    if (link == nullptr || cam == nullptr) return;

    const cXyz& p = link->current.pos;
    const f32 fx = cM_ssin(kMorpheelFightAngle), fz = cM_scos(kMorpheelFightAngle);
    cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
    cXyz eye(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    cam->mCamera.QuickStart();
    cam->mCamera.SetTrimSize(0);
    cam->view.lookat.center.set(center.x, center.y, center.z);
    cam->view.lookat.eye.set(eye.x, eye.y, eye.z);
    fopCamM_SetAngleY(cam, kMorpheelFightAngle);
}

static void update_deathsword_auto_wolf() {
    const char* stage = dComIfGp_getStartStageName();
    static bool s_seenWolf = false;
    if (stage == nullptr || std::strcmp(stage, "D_MN10B") != 0 || !is_boss_rush_active()) {
        s_seenWolf = false;
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return;

    if (link->checkWolf()) {
        s_seenWolf = true;
        return;
    }
    if (s_seenWolf || link->checkMetamorphose() || link->checkEventRun()) return;

    fopAc_ac_c* va = fopAcM_SearchByName(fpcNm_E_VT_e);
    if (va == nullptr || !bbi::deathsword_ropes_cut(va)) return;

    link->procCoMetamorphoseInit();
}

static int gauntlet_next_phase_index(const char* currentName);

static bool s_duelReady = false;
static int  s_holdBlackFrames = 0;
static u32  s_duelGen = ~0u;
static ActorId s_gbId = {};
static u32  s_gndId = 0;
static int  s_gndStableFrames = 0;

static bool s_needInPlaceFade = false;

static void trigger_ganon_ground_duel() {
    rush_debug_logf("[hb-dbg] trigger_ganon_ground_duel");
    const int gndIdx = gauntlet_next_phase_index("Horseback Ganon");
    if (gndIdx >= 0) {
        s_activeFightIndex = gndIdx;
    }
    s_duelReady = false;
    s_holdBlackFrames = 0;
    s_gndStableFrames = 0;
    s_gndId = 0;
    s_gbId = {};
    s_needInPlaceFade = true;
    g_dComIfG_gameInfo.info.getDan().onSwitch(1);
    dComIfGs_onSaveDunSwitch(1);
    dComIfGs_onOneZoneSwitch(15, -1);
    s_gauntletPhase = 4;
}

static void update_ganon_ground_duel() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN09B") != 0) return;
    if (!is_boss_rush_active() || s_returningToChamber) return;
    if (s_pendingFightIndex != -1) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount) return;

    const bool isGanondorf = (std::strcmp(g_bossGalleryTable[t].displayName, "Ganondorf") == 0);
    const bool isGroundMode = (s_gauntletPhase == 4 ||
                               g_dComIfG_gameInfo.info.getDan().isSwitch(1) ||
                               dComIfGs_isSaveDunSwitch(1));
    if (!isGroundMode && !isGanondorf) return;

    if (instant_fight_rearm(s_duelGen)) {
        s_duelReady = false;
        s_holdBlackFrames = 0;
        s_gbId = {};
        s_gndId = 0;
        s_gndStableFrames = 0;
        s_needInPlaceFade = false;
    }

    fopAc_ac_c* fk = fopAcM_SearchByName(fpcNm_E_FK_e);
    if (fk != nullptr) {
        fopAcM_delete(fk);
    }
    fopAc_ac_c* zelda = fopAcM_SearchByName(fpcNm_HOZELDA_e);
    if (zelda != nullptr) {
        fopAcM_delete(zelda);
    }
    // Hide the horse instead of deleting it: Ganondorf's own final-death
    // demo camera (demo_camera() in d_a_b_gnd.cpp, case 62) unconditionally
    // calls dComIfGp_getHorseActor()->setHorsePosAndAngle(...) with no null
    // check, assuming the horse actor always still exists. Deleting it here
    // left that call dereferencing a stale pointer and crashed on the final
    // ground-duel kill.
    fopAc_ac_c* horse = reinterpret_cast<fopAc_ac_c*>(dComIfGp_getHorseActor());
    if (horse != nullptr) {
        cXyz awayPos(0.0f, -5000.0f, 0.0f);
        horse->current.pos = awayPos;
        horse->old.pos = awayPos;
        horse->speed.set(0.0f, 0.0f, 0.0f);
        horse->speedF = 0.0f;
    }

    cXyz arenaCenter(0.0f, 1100.0f, 0.0f);
    fopAc_ac_c* gb = fopAcM_SearchByName(fpcNm_OBJ_GB_e);
    bool gbNear = false;
    if (gb != nullptr) {
        const f32 dx = gb->current.pos.x - arenaCenter.x;
        const f32 dy = gb->current.pos.y - arenaCenter.y;
        const f32 dz = gb->current.pos.z - arenaCenter.z;
        gbNear = (dx * dx + dy * dy + dz * dz) < 5000.0f * 5000.0f;
    }
    if (gb != nullptr && gbNear) {
        if (s_gbId == 0) s_gbId = gb->id;
    } else if (s_gbId == 0 && svc_actor && s_modCtx) {
        ActorSpawnParams sp{};
        sp.parameters = 0xF0069600;
        sp.argument = 0;
        sp.room_num = 0;
        sp.position = {arenaCenter.x, arenaCenter.y, arenaCenter.z};
        sp.angle = {0, 0, 0};
        sp.scale = {1.0f, 1.0f, 1.0f};
        svc_actor->create_actor(s_modCtx, fpcNm_OBJ_GB_e, &sp, &s_gbId);
    }

    if (!s_duelReady) {
        if (s_needInPlaceFade) {
            boss_rush_screen_hold_black();
        }
        s_holdBlackFrames++;

        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link == nullptr) return;

        fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e);
        if (gnd == nullptr) return;

        if (gnd->id != s_gndId) {
            s_gndId = gnd->id;
            s_gndStableFrames = 0;
        } else if (s_gndStableFrames < 9999) {
            s_gndStableFrames++;
        }

        bbi::ganondorf_force_ground_duel(gnd);
        gnd->health = 100;

        b_gnd_class* bgnd = reinterpret_cast<b_gnd_class*>(gnd);
        bgnd->mHideSheath = 1;
        bgnd->field_0x770 = 0;
        bgnd->field_0x772 = 0;
        bgnd->mMoveMode = 1;
        bgnd->field_0xc44[0] = 200;

        if (bgnd->mpModelMorf != nullptr) {
            void* bck = dComIfG_getObjectRes("B_gnd", 0x5D /* B_GND_BCK_EGND_WALK */);
            if (bck != nullptr) {
                bgnd->mAnmID = 0x5D;
                bgnd->mpModelMorf->setAnm(reinterpret_cast<J3DAnmTransform*>(bck), 2, 0.0f, 1.0f, 0.0f, -1.0f);
            }
        }

        cXyz gndPos(-600.0f, 1100.0f, 0.0f);
        gnd->current.pos = gndPos;
        gnd->old.pos = gndPos;
        gnd->shape_angle.x = 0;
        gnd->shape_angle.z = 0;
        gnd->shape_angle.y = 0x4000;
        gnd->current.angle.x = 0;
        gnd->current.angle.z = 0;
        gnd->current.angle.y = 0x4000;
        gnd->speed.set(0.0f, 0.0f, 0.0f);
        gnd->speedF = 0.0f;
        gnd->gravity = -5.0f;

        bgnd->mAcch.CrrPos(dComIfG_Bgsp());

        mant_class* mant_p = reinterpret_cast<mant_class*>(fopAcM_SearchByID(bgnd->mMantChildID));
        if (mant_p != nullptr) {
            mant_p->field_0x3969 = 1;
        }

        dScnKy_env_light_c* kankyo = dKy_getEnvlight();
        if (kankyo != nullptr) {
            kankyo->wether = 1;
        }

        if (link->checkHorseRide()) {
            link->onForceHorseGetOff();
        }
        link->procWaitInit();
        link->cancelOriginalDemo();
        dComIfGp_event_reset();
        if (link->mEquipItem != 0x103) {
            link->swordEquip(TRUE);
            link->setSwordModel();
        }

        cXyz linkPos(600.0f, 1100.0f, 0.0f);
        const s16 linkAngle = static_cast<s16>(-0x4000);
        link->current.pos = linkPos;
        link->old.pos = linkPos;
        link->shape_angle.y = linkAngle;
        link->current.angle.y = linkAngle;
        link->speed.set(0.0f, 0.0f, 0.0f);
        link->speedF = 0.0f;

        camera_process_class* cam = boss_rush_get_active_player_camera();
        if (cam != nullptr) {
            const f32 fx = cM_ssin(linkAngle), fz = cM_scos(linkAngle);
            cXyz center(linkPos.x + fx * 200.0f, linkPos.y + 100.0f, linkPos.z + fz * 200.0f);
            cXyz eye(linkPos.x - fx * 450.0f, linkPos.y + 170.0f, linkPos.z - fz * 450.0f);
            cam->mCamera.Reset(center, eye);
            cam->mCamera.Start();
            cam->mCamera.SetTrimSize(0);
            fopCamM_SetAngleY(cam, linkAngle);
        }

        if (!s_needInPlaceFade || (s_holdBlackFrames >= 25 && s_gndStableFrames >= 15 && s_gbId != 0) || s_holdBlackFrames >= 45) {
            s_duelReady = true;
            Z2GetAudioMgr()->bgmStart(Z2BGM_VS_GANON_04, 0, 0);
            if (s_needInPlaceFade) {
                boss_rush_screen_fade_in(0.06f);
                s_needInPlaceFade = false;
            }
        }
    }
}

static void update_morpheel_iron_boots() {
    static bool s_done = false;
    static u32  s_gen = ~0u;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN01A") != 0) {
        s_done = false;
        return;
    }

    if (instant_fight_rearm(s_gen)) {
        s_done = false;
    }

    if (s_done || !is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Morpheel") != 0) {
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr || link->checkEventRun()) return;

    if (link->checkEquipHeavyBoots()) {
        s_done = true;
        return;
    }

    fopAc_ac_c* ob = fopAcM_SearchByName(fpcNm_B_OB_e);
    if (ob != nullptr && bbi::morpheel_is_phase2(ob)) {
        s_done = true;
        return;
    }

    const bool inWater = link->checkModeFlg(0x40000) ||
                         link->mWaterY > link->current.pos.y + 20.0f;
    if (!inWater) return;

    link->setHeavyBoots(1);
    s_done = true;
}

struct MinibossBgm { const char* name; const char* stage; s16 proc; u32 theme; s32 subStatus; };
static const MinibossBgm kMinibossBgm[] = {
    {"Ook",         "D_MN05B", fpcNm_E_MK_e,  Z2BGM_BOOMERAMG_MONKEY, -1},
    {"Dangoro",     "D_MN04B", fpcNm_E_GOB_e, Z2BGM_MAGNE_GORON,      -1},
    {"Deku Toad",   "D_MN01B", fpcNm_E_DT_e,  Z2BGM_DEKUTOAD,         -1},
    {"Death Sword", "D_MN10B", fpcNm_E_VT_e,  Z2BGM_VARIANT,           1},
    {"Darknut",     "D_MN06B", fpcNm_B_TN_e,  Z2BGM_TN_MBOSS,         -1},
    {"Darkhammer",  "D_MN11B", fpcNm_E_TH_e,  Z2BGM_IB_MBOSS,         -1},
    {"Aeralfos",    "D_MN07B", fpcNm_B_GG_e,  Z2BGM_GG_MBOSS,         -1},
};

static void fix_boss_rush_miniboss_bgm() {
    static bool s_muted = false;
    static const MinibossBgm* s_activeMb = nullptr;
    static int s_frames = 0;

    const MinibossBgm* mb = nullptr;
    if (is_boss_rush_active() && !s_returningToChamber) {
        const int t = boss_rush_target_index();
        if (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount) {
            const char* name = g_bossGalleryTable[t].displayName;
            const char* stage = dComIfGp_getStartStageName();
            for (const MinibossBgm& e : kMinibossBgm) {
                if (std::strcmp(e.name, name) != 0) continue;
                if (stage != nullptr && std::strcmp(stage, e.stage) == 0 &&
                    boss_rush_is_fighting_here() &&
                    fopAcM_SearchByName(e.proc) != nullptr) {
                    daAlink_c* link = daAlink_getAlinkActorClass();
                    if (link == nullptr || !link->checkEventRun()) mb = &e;
                }
                break;
            }
        }
    }

    if (mb != s_activeMb) {
        s_activeMb = mb;
        s_frames = 0;
    }

    Z2AudioMgr* audio = Z2GetAudioMgr();
    if (audio == nullptr) return;

    if (mb != nullptr) {
        ++s_frames;
        if (s_frames < 120 && audio->getSubBgmID() != mb->theme) {
            audio->subBgmStart(mb->theme);
            if (mb->subStatus >= 0) audio->changeSubBgmStatus(mb->subStatus);
        }
        audio->muteSceneBgm(0, 0.0f);
        s_muted = true;
    } else if (s_muted) {
        audio->unMuteSceneBgm(30);
        s_muted = false;
    }
}

static void update_darkhammer_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static u32 s_actorId = 0;
    static int s_frames = 0;
    if (instant_fight_rearm(s_gen)) s_done = false;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN11B") != 0) {
        s_done = false;
        s_actorId = 0;
        s_frames = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Darkhammer") != 0) {
        return;
    }

    if (s_done) return;

    cDmr_SkipInfo = 1;

    fopAc_ac_c* th = fopAcM_SearchByName(fpcNm_E_TH_e);
    if (th == nullptr) {
        return;
    }
    if (++s_frames > 600) {
        cDmr_SkipInfo = 0;
        s_done = true;
        return;
    }

    const u32 thId = th->id;
    if (thId == s_actorId) return;

    e_th_class* eth = reinterpret_cast<e_th_class*>(th);
    if (eth->mpModelMorf == nullptr) {
        return;
    }

    bbi::darkhammer_force_fight_start(th);
    s_actorId = thId;
    cDmr_SkipInfo = 0;
    s_done = true;
}

static int s_dangoroCamArmFrames = 0;

static void update_dangoro_camera() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN04B") != 0) {
        s_dangoroCamArmFrames = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) {
        s_dangoroCamArmFrames = 0;
        return;
    }
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Dangoro") != 0) {
        s_dangoroCamArmFrames = 0;
        return;
    }

    if (s_dangoroCamArmFrames <= 0) return;
    --s_dangoroCamArmFrames;

    daAlink_c* link = daAlink_getAlinkActorClass();
    camera_process_class* cam = boss_rush_get_active_player_camera();
    if (link == nullptr || cam == nullptr) return;

    const cXyz& p = link->current.pos;
    const s16 ang = link->current.angle.y;
    const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
    cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
    cXyz eye(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    cam->mCamera.SetTrimSize(0);
    fopCamM_SetAngleY(cam, ang);
}

static void update_dangoro_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static int  s_platformCheckFrames = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN04B") != 0) {
        s_done = false;
        s_platformCheckFrames = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Dangoro") != 0) {
        return;
    }

    if (instant_fight_rearm(s_gen)) {
        s_done = false;
        s_platformCheckFrames = 90;
        dComIfGs_onZoneSwitch(5, 51);
        dComIfGs_onZoneSwitch(5, -1);
        cDmr_SkipInfo = 1;
    }

    dComIfGs_onZoneSwitch(5, 51);
    dComIfGs_onZoneSwitch(5, -1);

    fopAc_ac_c* platformAc = fopAcM_SearchByName(fpcNm_OBJ_MSIMA_e);
    if (platformAc != nullptr) {
        obj_msima_class* platform = reinterpret_cast<obj_msima_class*>(platformAc);
        if (platform->field_0x58c != 0.0f) {
            platform->field_0x58c = 0.0f;
        }
        if (platform->mAction != obj_msima_class::ACTION_FLOAT_1 &&
            platform->mAction != obj_msima_class::ACTION_FLOAT_2) {
            platform->mAction = obj_msima_class::ACTION_FLOAT_1;
        }
    }

    if (s_done && s_platformCheckFrames <= 0) return;
    if (s_platformCheckFrames > 0) --s_platformCheckFrames;

    if (s_done) {
        if (platformAc == nullptr) return;
        obj_msima_class* platformOnly = reinterpret_cast<obj_msima_class*>(platformAc);
        for (int i = 0; i < 4; ++i) {
            platformOnly->mChains[i].field_0x92 = 1;
        }
        platformOnly->field_0x58c = 0.0f;
        platformOnly->field_0x59c = 300.0f;
        platformOnly->field_0x5a0 = 0;
        platformOnly->field_0x5a4 = 0.0f;
        platformOnly->field_0x5a8 = 0.0f;
        platformAc->current.pos.x = 0.0f;
        platformAc->current.pos.y = 801.0f;
        platformAc->current.pos.z = 0.0f;
        platformAc->old.pos = platformAc->current.pos;
        platformAc->speed.set(0.0f, 0.0f, 0.0f);
        platformAc->shape_angle.x = 0;
        platformAc->shape_angle.z = 0;
        platformOnly->mAction = obj_msima_class::ACTION_FLOAT_1;
        return;
    }

    cDmr_SkipInfo = 1;

    fopAc_ac_c* gob = fopAcM_SearchByName(fpcNm_E_GOB_e);
    if (gob == nullptr) return;
    if (!bbi::dangoro_model_ready(gob)) return;
    if (platformAc == nullptr) return;

    if (bbi::dangoro_awaiting_lower(gob)) {
        bbi::dangoro_force_fight_start(gob);
    }

    obj_msima_class* platform = reinterpret_cast<obj_msima_class*>(platformAc);
    for (int i = 0; i < 4; ++i) {
        platform->mChains[i].field_0x92 = 1;
    }
    platform->field_0x58c = 0.0f;
    platform->field_0x59c = 300.0f;
    platform->field_0x5a0 = 0;
    platform->field_0x5a4 = 0.0f;
    platform->field_0x5a8 = 0.0f;
    platformAc->current.pos.x = 0.0f;
    platformAc->current.pos.y = 801.0f;
    platformAc->current.pos.z = 0.0f;
    platformAc->old.pos = platformAc->current.pos;
    platformAc->speed.set(0.0f, 0.0f, 0.0f);
    platformAc->shape_angle.x = 0;
    platformAc->shape_angle.z = 0;
    platform->mAction = obj_msima_class::ACTION_FLOAT_1;

    daAlink_c* dangoroLink = daAlink_getAlinkActorClass();
    if (dangoroLink != nullptr) {
        dangoroLink->current.angle.y = static_cast<s16>(0x8000);
        dangoroLink->shape_angle.y = static_cast<s16>(0x8000);
    }
    s_dangoroCamArmFrames = 20;

    cDmr_SkipInfo = 0;
    s_done = true;
}

static void update_ook_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    if (instant_fight_rearm(s_gen)) s_done = false;
    static int  s_camFrames = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN05B") != 0) {
        s_done = false;
        s_camFrames = 0;
        return;
    }
    if (s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Ook") != 0) {
        s_done = false;
        s_camFrames = 0;
        return;
    }

    if (s_camFrames > 0) {
        --s_camFrames;
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_class* cam = static_cast<camera_class*>(dComIfGp_getCamera(0));
        if (link != nullptr && cam != nullptr) {
            const cXyz& p = link->current.pos;
            const s16 ang = link->current.angle.y;
            const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
            cam->view.lookat.center.set(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
            cam->view.lookat.eye.set(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
            fopCamM_SetAngleY(cam, ang);
        }
    }

    fopAc_ac_c* mk = fopAcM_SearchByName(fpcNm_E_MK_e);
    if (mk == nullptr) return;

    if (bbi::ook_intro_pending(mk)) {
        e_mk_class* m = reinterpret_cast<e_mk_class*>(mk);
        m->demoMode = e_mk_class::DEMO_MODE_NONE;
        m->demoSubMode = 0;
        m->action = e_mk_class::ACT_WAIT;
        m->mode = 0;
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) link->cancelOriginalDemo();
        dComIfGp_event_reset();
        camera_process_class* cam1 = dComIfGp_getCamera(0);
        if (cam1 != nullptr) {
            cam1->mCamera.Start();
            cam1->mCamera.SetTrimSize(0);
        }
        s_camFrames = 10;
    }

    if (s_done || !is_boss_rush_active() || s_arenaSettle > kBossUnfreezeFrame) return;

    e_mk_class* m = reinterpret_cast<e_mk_class*>(mk);
    m->action = e_mk_class::ACT_MOVE;
    m->mode = 0;
    s_done = true;
}

static void update_diababa_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static int s_frames = 0;
    static int s_camFrames = 0;
    static u32 s_actorId = 0;
    static u32 s_seenId = 0;
    static int s_watchdog = 0;
    if (instant_fight_rearm(s_gen)) {
        s_done = false;
        s_frames = 0;
        s_camFrames = 0;
    }

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN05A") != 0) {
        s_done = false;
        s_frames = 0;
        s_camFrames = 0;
        s_actorId = 0;
        s_seenId = 0;
        s_watchdog = 0;
        return;
    }
    if (s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Diababa") != 0) {
        s_done = false;
        s_camFrames = 0;
        return;
    }

    if (s_watchdog > 0) {
        --s_watchdog;
        if (!boss_rush_midna_talk_hold_active() && dComIfGp_event_runCheck()) {
            dComIfGp_event_reset();
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) link->cancelOriginalDemo();
            camera_process_class* cam0 = dComIfGp_getCamera(0);
            if (cam0 != nullptr) {
                cam0->mCamera.Start();
                cam0->mCamera.SetTrimSize(0);
            }
        }
    }

    if (s_camFrames > 0) {
        --s_camFrames;
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_class* cam = static_cast<camera_class*>(dComIfGp_getCamera(0));
        if (link != nullptr && cam != nullptr) {
            const cXyz& p = link->current.pos;
            cam->view.lookat.center.set(p.x, p.y + 100.0f, p.z - 200.0f);
            cam->view.lookat.eye.set(p.x, p.y + 170.0f, p.z + 500.0f);
            fopCamM_SetAngleY(cam, static_cast<s16>(0x8000));
        }
    }
    if (s_done) return;

    fopAc_ac_c* bq = fopAcM_SearchByName(fpcNm_B_BQ_e);
    if (bq == nullptr) {
        cDmr_SkipInfo = 60;
        return;
    }
    if (++s_frames > 900) {
        cDmr_SkipInfo = 0;
        s_done = true;
        return;
    }

    b_bq_class* b = reinterpret_cast<b_bq_class*>(bq);

    const u32 bqId = bq->id;
    if (bqId != s_seenId) {
        s_seenId = bqId;
        s_frames = 0;
    }
    if (bqId == s_actorId) {
        cDmr_SkipInfo = 60;
        return;
    }

    fopAc_ac_c* t0 = fopAcM_SearchByID(b->mTentacleIDs[0]);
    fopAc_ac_c* t1 = fopAcM_SearchByID(b->mTentacleIDs[1]);
    if (++s_frames < 60 && (t0 == nullptr || t1 == nullptr)) {
        cDmr_SkipInfo = 60;
        return;
    }

    const u8 sw = static_cast<u8>((fopAcM_GetParam(bq) & 0x0000FF00) >> 8);
    g_dComIfG_gameInfo.info.onSwitch(sw, fopAcM_GetRoomNo(bq));

    const u8 sw2 = static_cast<u8>((fopAcM_GetParam(bq) >> 0x10) & 0xFF);
    g_dComIfG_gameInfo.info.offSwitch(sw2, fopAcM_GetRoomNo(bq));

    if (b->mDemoMode >= 10) {
        b->mDemoMode = 0;
    }

    b->mAction = 0;
    b->field_0x1392 = 2;
    b->mDisableDraw = true;
    b->mColpatType = 1;
    b->mColpatBlend = 1.0f;
    Z2GetAudioMgr()->bgmStart(Z2BGM_BOSSBABA_1, 0, 0);

    for (int i = 0; i < 2; ++i) {
        fopAc_ac_c* tent = fopAcM_SearchByID(b->mTentacleIDs[i]);
        if (tent != nullptr) {
            b_bh_class* bh = reinterpret_cast<b_bh_class*>(tent);
            if (bh->mAction == 50) {
                bh->mAction = 0;
                bh->mMode = 0;
                bh->mBasePos = tent->home.pos;
                tent->current.pos.y = tent->home.pos.y - 500.0f;
                tent->shape_angle.z = 0;
            }
        }
    }

    dComIfGp_event_reset();
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        link->cancelOriginalDemo();
    }
    camera_process_class* cam1 = dComIfGp_getCamera(0);
    if (cam1 != nullptr) {
        cam1->mCamera.Start();
        cam1->mCamera.SetTrimSize(0);
    }
    s_camFrames = 10;

    cDmr_SkipInfo = 0;
    s_actorId = bqId;
    s_watchdog = 180;
    s_done = true;
}

static void update_fyrus_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    if (instant_fight_rearm(s_gen)) s_done = false;
    static int  s_camFrames = 0;

    if (s_returningToChamber) return;
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Fyrus") != 0) {
        s_done = false;
        s_camFrames = 0;
        return;
    }

    if (s_camFrames > 0) {
        --s_camFrames;
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_class* cam = static_cast<camera_class*>(dComIfGp_getCamera(0));
        if (link != nullptr && cam != nullptr) {
            const cXyz& p = link->current.pos;
            cam->view.lookat.center.set(p.x, p.y + 100.0f, p.z - 200.0f);
            cam->view.lookat.eye.set(p.x, p.y + 170.0f, p.z + 500.0f);
            fopCamM_SetAngleY(cam, static_cast<s16>(0x8000));
        }
    }
    if (s_done) return;

    cDmr_SkipInfo = 1;

    fopAc_ac_c* fm = fopAcM_SearchByName(fpcNm_E_FM_e);
    if (fm == nullptr) return;
    if (!bbi::fyrus_model_ready(fm) || fm->health <= 0) return;

    if (bbi::fyrus_in_intro(fm)) {
        bbi::fyrus_force_fight_start(fm);
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) link->cancelOriginalDemo();
        dComIfGp_event_reset();
    } else {
        dComIfGp_event_reset();
    }
    cDmr_SkipInfo = 0;
    s_camFrames = 6;
    s_done = true;
}

static void update_fyrus_intro_watchdog() {
    static int s_introEventKillFrames = 0;
    if (s_returningToChamber) return;
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Fyrus") != 0) {
        s_introEventKillFrames = 0;
        return;
    }
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN04A") != 0) {
        s_introEventKillFrames = 0;
        return;
    }

    if (s_introEventKillFrames > 0) {
        --s_introEventKillFrames;
        if (dComIfGp_event_runCheck()) {
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) {
                link->cancelOriginalDemo();
            }
            dComIfGp_event_reset();
        }
    }

    fopAc_ac_c* fm = fopAcM_SearchByName(fpcNm_E_FM_e);
    if (fm == nullptr) return;
    if (fm->health <= 0) return;
    auto* f = reinterpret_cast<e_fm_class*>(fm);
    if (f->mAction != 11) return;

    bbi::fyrus_force_fight_start(fm);
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        link->cancelOriginalDemo();
    }
    dComIfGp_event_reset();
    s_introEventKillFrames = 90;
    camera_process_class* cam = boss_rush_get_active_player_camera();
    if (cam != nullptr && link != nullptr) {
        const cXyz& p = link->current.pos;
        const s16 ang = link->current.angle.y;
        const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
        cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
        cXyz eye(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
        cam->mCamera.Reset(center, eye);
        cam->mCamera.Start();
        cam->mCamera.SetTrimSize(0);
        fopCamM_SetAngleY(cam, ang);
    }
    cDmr_SkipInfo = 0;
}

static void update_dekutoad_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static u32 s_actorId = 0;
    static int s_frames = 0;
    if (instant_fight_rearm(s_gen)) s_done = false;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN01B") != 0) {
        s_done = false;
        s_actorId = 0;
        s_frames = 0;
        return;
    }
    if (s_done || !is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Deku Toad") != 0) {
        return;
    }

    cDmr_SkipInfo = 1;

    fopAc_ac_c* dt = fopAcM_SearchByName(fpcNm_E_DT_e);
    if (dt == nullptr) return;
    if (!bbi::dekutoad_model_ready(dt)) return;
    if (++s_frames > 600) {
        cDmr_SkipInfo = 0;
        s_done = true;
        return;
    }

    const u32 dtId = dt->id;
    if (dtId == s_actorId) return;

    if (bbi::dekutoad_in_opening(dt)) {
        bbi::dekutoad_force_fight_start(dt);
        dt->current.pos.set(0.0f, 0.0f, -500.0f);
        dt->old.pos = dt->current.pos;
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            dt->shape_angle.y = cLib_targetAngleY(&dt->current.pos, &link->current.pos);
            link->cancelOriginalDemo();
        }
        Z2GetAudioMgr()->subBgmStart(Z2BGM_DEKUTOAD);
        dComIfGs_onOneZoneSwitch(3, fopAcM_GetRoomNo(dt));
        dComIfGp_event_reset();
        s_actorId = dtId;
        cDmr_SkipInfo = 0;
        s_done = true;
    } else if (std::fabs(dt->current.pos.z - -500.0f) < 50.0f) {
        s_actorId = dtId;
        cDmr_SkipInfo = 0;
        s_done = true;
    }
}

static void update_morpheel_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static int  s_frames = 0;
    static u32  s_actorId = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN01A") != 0) {
        s_done = false;
        s_frames = 0;
        s_actorId = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Morpheel") != 0) {
        return;
    }

    if (instant_fight_rearm(s_gen)) {
        s_done = false;
        s_frames = 0;
        s_actorId = 0;
        s_morpheelPosPinFrames = 0;
        s_morpheelCamArmFrames = 90;
    }
    if (s_done) return;

    cDmr_SkipInfo = 60;

    fopAc_ac_c* ob = fopAcM_SearchByName(fpcNm_B_OB_e);
    if (ob == nullptr) return;
    if (!bbi::morpheel_model_ready(ob)) return;

    const u32 obId = ob->id;
    if (obId == s_actorId) return;

    if (++s_frames < 120 && !bbi::morpheel_tentacles_ready(ob)) return;

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr && !link->checkEquipHeavyBoots() && !bbi::morpheel_is_phase2(ob)) {
        link->setHeavyBoots(1);
    }

    s_actorId = obId;
    s_done = true;
}

static void update_deathsword_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static u32 s_actorId = 0;
    static int s_frames = 0;
    if (instant_fight_rearm(s_gen)) s_done = false;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN10B") != 0) {
        s_done = false;
        s_actorId = 0;
        s_frames = 0;
        return;
    }
    if (s_done || !is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Death Sword") != 0) {
        return;
    }

    static bool s_rewardDoorShut = false;
    static u32 s_rewardDoorGen = ~0u;
    if (instant_fight_rearm(s_rewardDoorGen)) {
        s_rewardDoorShut = false;
    }
    if (!s_rewardDoorShut && !s_returningToChamber) {
        fopAc_ac_c* shutterAc = fopAcM_SearchByName(fpcNm_Obj_Lv4EdShutter_e);
        if (shutterAc != nullptr) {
            daLv4EdShutter_c* shutter = static_cast<daLv4EdShutter_c*>(shutterAc);
            dComIfGs_offSwitch(shutter->mZenmetuSw, fopAcM_GetRoomNo(shutterAc));
            dComIfGs_offSwitch(shutter->mOpenSw, fopAcM_GetRoomNo(shutterAc));
            dComIfGs_offSwitch(shutter->mCloseSw, fopAcM_GetRoomNo(shutterAc));
            shutter->init_modeClose();
            s_rewardDoorShut = true;
        }
    }

    cDmr_SkipInfo = 1;

    fopAc_ac_c* va = fopAcM_SearchByName(fpcNm_E_VT_e);
    if (va == nullptr) return;
    if (!bbi::deathsword_model_ready(va)) return;
    if (++s_frames > 600) {
        if (!bbi::deathsword_in_intro(va)) {
            cDmr_SkipInfo = 0;
            s_done = true;
            return;
        }
    }

    const u32 vaId = va->id;
    if (vaId == s_actorId) return;

    if (bbi::deathsword_in_intro(va) &&
        !dComIfGs_isOneZoneSwitch(9, fopAcM_GetRoomNo(va))) {
        if (s_frames < 600) return;
        daE_VA_c* v = static_cast<daE_VA_c*>(static_cast<void*>(va));
        v->mRopesEnabled = false;
        v->setActionMode(daE_VA_c::ACTION_CLEAR_WAIT_e, 0);
        dComIfGs_onOneZoneSwitch(9, fopAcM_GetRoomNo(va));
        Z2GetAudioMgr()->subBgmStart(Z2BGM_VARIANT);
        Z2GetAudioMgr()->changeSubBgmStatus(1);
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) link->cancelOriginalDemo();
        dComIfGp_event_reset();
        s_actorId = vaId;
        cDmr_SkipInfo = 0;
        s_done = true;
    } else if (bbi::deathsword_ropes_cut(va) &&
               dComIfGs_isOneZoneSwitch(9, fopAcM_GetRoomNo(va))) {
        s_actorId = vaId;
        cDmr_SkipInfo = 0;
        s_done = true;
    }
}

static void update_blizzeta_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    static u32 s_actorId = 0;
    static int  s_frames = 0;
    static int  s_eventKill = 0;
    if (instant_fight_rearm(s_gen)) s_done = false;
    static int  s_camFrames = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN11A") != 0) {
        s_done = false;
        s_camFrames = 0;
        s_actorId = 0;
        s_frames = 0;
        s_eventKill = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Blizzeta") != 0) {
        return;
    }

    if (s_camFrames > 0) {
        --s_camFrames;
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_class* cam = static_cast<camera_class*>(dComIfGp_getCamera(0));
        if (link != nullptr && cam != nullptr) {
            const cXyz& p = link->current.pos;
            const s16 ang = static_cast<s16>(0x6AAB);
            const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
            cam->view.lookat.center.set(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
            cam->view.lookat.eye.set(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
            fopCamM_SetAngleY(cam, ang);
        }
    }
    if (s_done) {
        if (s_eventKill > 0) {
            --s_eventKill;
            if (!boss_rush_midna_talk_hold_active() && dComIfGp_event_runCheck()) {
                daAlink_c* link = daAlink_getAlinkActorClass();
                if (link != nullptr) link->cancelOriginalDemo();
                dComIfGp_event_reset();
            }
        }
        return;
    }

    cDmr_SkipInfo = 1;

    fopAc_ac_c* yo = fopAcM_SearchByName(fpcNm_B_YO_e);
    if (yo == nullptr) return;
    if (!bbi::blizzeta_model_ready(yo)) return;
    if (++s_frames > 600) {
        cDmr_SkipInfo = 0;
        s_eventKill = 300;
        s_done = true;
        return;
    }

    const u32 yoId = yo->id;
    if (yoId == s_actorId) return;

    daB_YO_c* y = static_cast<daB_YO_c*>(static_cast<void*>(yo));
    if (y->mAction == 0) return;

    dComIfGp_event_reset();
    Z2GetAudioMgr()->setDemoName("force_end");

    s_actorId = yoId;
    cDmr_SkipInfo = 0;
    s_camFrames = 8;
    s_eventKill = 300;
    s_done = true;
}

static void update_stallord_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    if (instant_fight_rearm(s_gen)) s_done = false;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN10A") != 0) {
        s_done = false;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Stallord") != 0) {
        return;
    }
    if (s_done) return;

    static constexpr f32 kPoeGateDownY = 1800.0f;
    if (!s_returningToChamber) {
        fopAc_ac_c* bdoorAc = fopAcM_SearchByName(fpcNm_L1BOSS_DOOR_e);
        if (bdoorAc != nullptr) {
            daBdoorL1_c* bdoor = static_cast<daBdoorL1_c*>(bdoorAc);
            if (bdoor->field_0x588 != nullptr && bdoor->mAction != daBdoorL1_c::ACTION_WAIT) {
                bdoor->closeInit();
                bdoor->field_0x588->setFrame(bdoor->field_0x588->getEndFrame());
                bdoor->calcMtx();
                bdoor->setAction(daBdoorL1_c::ACTION_WAIT);
                rush_debug_logf("[ds-door] stallord boss door closed");
            }
        }
        fopAc_ac_c* gateAc = fopAcM_SearchByName(fpcNm_Obj_Lv4PoGate_e);
        if (gateAc != nullptr) {
            daLv4PoGate_c* gate = static_cast<daLv4PoGate_c*>(gateAc);
            dComIfGs_offSwitch(gate->mSw, fopAcM_GetRoomNo(gateAc));
            if (gate->mMoveValue != 0.0f || gate->current.pos.y != kPoeGateDownY ||
                gate->mMode != daLv4PoGate_c::MODE_WAIT_e) {
                gate->mMoveValue = 0.0f;
                gate->current.pos.y = kPoeGateDownY;
                gate->init_modeWait();
            }
        }
    }

    for (int z = 0; z < dSv_info_c::ZONE_MAX; ++z) {
        dSv_zone_c& zone = g_dComIfG_gameInfo.info.getZone(z);
        if (zone.getRoomNo() == 50 || zone.getRoomNo() == -1) {
            zone.getBit().offSwitch(6);
            zone.getBit().offSwitch(7);
            zone.getBit().offSwitch(8);
        }
    }

    cDmr_SkipInfo = 1;

    fopAc_ac_c* ds = fopAcM_SearchByName(fpcNm_B_DS_e);
    if (ds == nullptr) return;
    if (!bbi::stallord_model_ready(ds)) return;

    static u32 s_skippedId = 0;
    const u32 dsId = ds->id;
    if (dsId == s_skippedId) {
        s_done = true;
        return;
    }

    dComIfGp_event_reset();
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) link->cancelOriginalDemo();
    Z2GetAudioMgr()->setDemoName("force_end");

    s_skippedId = dsId;
    s_done = true;
}

static int s_stallordP2CamFrames = 0;
static int s_stallordArenaSnapFrames = 0;

static int stallord_arena_snap_phase2(void* i_actor, void* i_count) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(i_actor);
    if (ac == nullptr) return 0;
    int* n = static_cast<int*>(i_count);
    const s16 nm = fopAcM_GetName(ac);

    if (nm == fpcNm_Obj_Lv4Sand_e) {
        daObjLv4Sand_c* s = static_cast<daObjLv4Sand_c*>(ac);
        s->mHeight = -3500.0f;
        s->mMode = daObjLv4Sand_c::MODE_DEAD;
        ++*n;
    } else if (nm == fpcNm_Obj_Lv4RailWall_e) {
        daObjLv4Wall_c* w = static_cast<daObjLv4Wall_c*>(ac);
        w->mHeight = 3375.0f;
        w->mMode = daObjLv4Wall_c::MODE_DEAD;
        ++*n;
    } else if (nm == fpcNm_Obj_Lv4Bridge_e) {
        daObjLv4Brg_c* b = static_cast<daObjLv4Brg_c*>(ac);
        b->current.pos.y = -100000.0f;
        b->old.pos.y = -100000.0f;
        ++*n;
    } else if (nm == fpcNm_Obj_SwSpinner_e) {
        daObjSwSpinner_c* sp = static_cast<daObjSwSpinner_c*>(ac);
        sp->mPartBHeight = 50.0f;
        sp->mCanUse = false;
        ++*n;
    }
    return 0;
}

static void stallord_phase2_finalize(fopAc_ac_c* ds) {
    camera_process_class* cam = boss_rush_get_active_player_camera();
    daAlink_c* link = daAlink_getAlinkActorClass();
    const s8 room = static_cast<s8>(fopAcM_GetRoomNo(ds));
    const cXyz p(2088.60f, -1594.61f, -1337.98f);
    const s16 faceYaw = cM_deg2s(356.0f);

    dComIfGp_event_reset();
    if (link != nullptr) {
        daSpinner_c* spinner = link->getSpinnerActor();
        if (spinner != nullptr) {
            spinner->forceDelete();
        }
        link->cancelOriginalDemo();
        link->current.pos = p;
        link->old.pos = p;
        link->shape_angle.y = faceYaw;
        link->current.angle.y = faceYaw;
        link->speed.set(0.0f, 0.0f, 0.0f);
        link->speedF = 0.0f;
    }
    if (cam != nullptr) {
        const f32 fx = cM_ssin(faceYaw), fz = cM_scos(faceYaw);
        cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
        cXyz eye(p.x - fx * 450.0f, p.y + 170.0f, p.z - fz * 450.0f);
        cam->mCamera.Reset(center, eye);
        cam->mCamera.Start();
        cam->mCamera.SetTrimSize(0);
        fopCamM_SetAngleY(cam, faceYaw);
        s_stallordP2CamFrames = 90;
    }

    bbi::stallord_prep_phase2_wait(ds);

    dComIfGs_onZoneSwitch(6, room);
    dComIfGs_onZoneSwitch(7, room);
    dComIfGs_onZoneSwitch(8, room);
    dComIfGs_setRestartRoom(p, faceYaw, 50);
    dComIfGs_setRestartRoomParam((50 & 0x3F) | (0xFF << 24));
    Z2GetAudioMgr()->bgmStart(Z2BGM_HARAGIGANT_BTL02, 0, 0);
    Z2GetAudioMgr()->setDemoName("force_end");

    int snapped = 0;
    fopAcIt_Executor(stallord_arena_snap_phase2, &snapped);
    s_stallordArenaSnapFrames = 40;
}

static void update_stallord_phase_transition_skip() {
    static u32  s_gen = ~0u;
    static bool s_p1Killed = false;
    static bool s_p2Finalized = false;
    static bool s_done = false;
    static int  s_fadeFrames = 0;
    if (instant_fight_rearm(s_gen)) {
        s_p1Killed = false; s_p2Finalized = false; s_done = false; s_fadeFrames = 0;
        s_stallordP2CamFrames = 0; s_stallordArenaSnapFrames = 0;
        mDoGph_gInf_c::offFade();
    }

    if (s_stallordArenaSnapFrames > 0) {
        --s_stallordArenaSnapFrames;
        int n = 0;
        fopAcIt_Executor(stallord_arena_snap_phase2, &n);
    }

    if (s_stallordP2CamFrames > 0) {
        --s_stallordP2CamFrames;
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_process_class* cam = boss_rush_get_active_player_camera();
        if (link != nullptr && cam != nullptr) {
            const s16 ang = link->shape_angle.y;
            const cXyz& lp = link->current.pos;
            const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
            cXyz center(lp.x + fx * 200.0f, lp.y + 100.0f, lp.z + fz * 200.0f);
            cXyz eye(lp.x - fx * 450.0f, lp.y + 170.0f, lp.z - fz * 450.0f);
            cam->mCamera.Reset(center, eye);
            cam->mCamera.Start();
            cam->mCamera.SetTrimSize(0);
            fopCamM_SetAngleY(cam, ang);
        }
    }

    if (s_done) return;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN10A") != 0) return;
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Stallord") != 0) {
        return;
    }

    fopAc_ac_c* ds = fopAcM_SearchByName(fpcNm_B_DS_e);

    if (!s_p1Killed) {
        if (ds != nullptr && bbi::stallord_p1_death_demo(ds)) {
            mDoGph_gInf_c::fadeOut(0.2f);
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) link->cancelOriginalDemo();
            dComIfGp_event_reset();

            ActorSpawnParams sp{};
            sp.parameters = fopAcM_GetParam(ds) | daB_DS_c::TYPE_BATTLE_2;
            sp.argument = static_cast<int8_t>(0xFF);
            sp.room_num = static_cast<int8_t>(fopAcM_GetRoomNo(ds));
            sp.position = {ds->current.pos.x, ds->current.pos.y, ds->current.pos.z};
            sp.angle = {0, 0, 0};
            sp.scale = {1.0f, 1.0f, 1.0f};
            ActorId p2Id{};
            svc_actor->create_actor(s_modCtx, fpcNm_B_DS_e, &sp, &p2Id);
            fopAcM_delete(ds);
            s_p1Killed = true;
            s_fadeFrames = 5;
        }
        return;
    }

    if (!s_p2Finalized) {
        if (s_fadeFrames > 0) {
            --s_fadeFrames;
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) link->cancelOriginalDemo();
            dComIfGp_event_reset();
        }

        if (ds != nullptr) {
            daB_DS_c* dsBoss = reinterpret_cast<daB_DS_c*>(ds);
            if (dsBoss->mBossPhase != 0 || bbi::stallord_p2_in_opening(ds) || bbi::stallord_p2_ready(ds)) {
                stallord_phase2_finalize(ds);
                dComIfGs_onZoneSwitch(7, static_cast<s8>(fopAcM_GetRoomNo(ds)));
                s_p2Finalized = true;
                mDoGph_gInf_c::fadeIn(0.2f);
                s_stallordP2CamFrames = 90;
                s_done = true;
            }
        }
    }
}

static int argorok_peahat_cam_finish(void* i_actor, void*) {
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(i_actor);
    if (a != nullptr && fopAcM_GetName(a) == fpcNm_E_PH_e) {
        bbi::peahat_snap_phase2(a);
    }
    return 0;
}

static void update_argorok_phase_transition_skip() {
    static u32  s_argorokGen = ~0u;
    static bool s_p2Done = false;
    static int  s_fadeFrames = 0;
    static bool s_groundValid = false;
    static cXyz s_groundPos;
    static s16  s_groundYAngle = 0;

    if (instant_fight_rearm(s_argorokGen)) {
        s_p2Done = false;
        s_fadeFrames = 0;
        s_groundValid = false;
        mDoGph_gInf_c::offFade();

        const int argoTarget = boss_rush_target_index();
        if (argoTarget >= 0 && static_cast<size_t>(argoTarget) < g_bossGalleryCount &&
            std::strcmp(g_bossGalleryTable[argoTarget].displayName, "Argorok") == 0) {
            dScnKy_env_light_c* kankyo = dKy_getEnvlight();
            if (kankyo != nullptr) {
                kankyo->wether = 0;
                kankyo->mColpatWeather = 0;
                kankyo->wether_pat0 = 0;
                kankyo->wether_pat1 = 0;
                kankyo->pat_ratio = 0.0f;
                kankyo->mColpatCurrGather = 0;
                kankyo->mColpatPrevGather = 0;
                kankyo->mColPatBlendGather = 0.0f;
                kankyo->raincnt = 0;
                kankyo->base_raincnt = 0;
            }
        }
    }

    if (s_p2Done) return;
    if (!boss_rush_is_fighting_here() || s_returningToChamber) return;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN07A") != 0) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Argorok") != 0) {
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return;

    fopAc_ac_c* dr = fopAcM_SearchByName(fpcNm_B_DR_e);
    if (dr == nullptr) return;
    const daB_DR_c* d = reinterpret_cast<const daB_DR_c*>(dr);
    if (d->mpModelMorf == nullptr) return;

    if (link->mLinkAcch.ChkGroundHit()) {
        s_groundPos = link->current.pos;
        s_groundYAngle = link->shape_angle.y;
        s_groundValid = true;
    }

    if (bbi::argorok_in_phase2_cutscene(dr)) {
        if (s_fadeFrames == 0) {
            mDoGph_gInf_c::fadeOut(0.2f);
        }
        s_fadeFrames++;

        const s8 room = static_cast<s8>(fopAcM_GetRoomNo(dr));

        fopAc_ac_c* dre = fopAcM_SearchByName(fpcNm_B_DRE_e);
        if (dre != nullptr) {
            fopAcM_delete(dre);
        }
        fpc_ProcID pid = bbi::argorok_get_parent(dr);
        if (pid != 0) {
            fopAc_ac_c* pAc = fopAcM_SearchByID(pid);
            if (pAc != dre && pAc != nullptr) {
                fopAcM_delete(pAc);
            }
        }

        bbi::argorok_force_phase2(dr);

        dComIfGs_onZoneSwitch(2, room);
        dComIfGs_onZoneSwitch(23, room);
        g_dComIfG_gameInfo.info.onSwitch(16, room);
        g_dComIfG_gameInfo.info.onSwitch(0x3F, room);

        dScnKy_env_light_c* kankyo = dKy_getEnvlight();
        if (kankyo != nullptr) {
            kankyo->wether = 2;
            kankyo->mColpatWeather = 2;
            kankyo->wether_pat0 = 2;
            kankyo->wether_pat1 = 2;
            kankyo->pat_ratio = 1.0f;
            kankyo->mColpatCurrGather = 2;
            kankyo->mColpatPrevGather = 2;
            kankyo->mColPatBlendGather = 1.0f;
            kankyo->raincnt = 250;
            kankyo->base_raincnt = 250;
        }

        Z2GetAudioMgr()->subBgmStop();
        Z2GetAudioMgr()->bgmStart(Z2BGM_DRAGON_BTL02, 0, 0);
        Z2GetAudioMgr()->setDemoName("force_end");

        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            link->cancelOriginalDemo();
            // The tail-hang demo state would keep Link glued to Argorok after
            // the skip; put him back on the arena floor where he last stood.
            if (s_groundValid) {
                link->current.pos = s_groundPos;
                link->old.pos = s_groundPos;
                link->shape_angle.y = s_groundYAngle;
                link->current.angle.y = s_groundYAngle;
            }
            link->speed.set(0.0f, 0.0f, 0.0f);
            link->speedF = 0.0f;
            link->procWaitInit();
            if (link->mEquipItem != 0x103) {
                link->swordEquip(TRUE);
                link->setSwordModel();
            }
            link->mLinkAcch.CrrPos(dComIfG_Bgsp());
        }
        dComIfGp_event_reset();

        fopAcIt_Executor(argorok_peahat_cam_finish, nullptr);

        camera_process_class* cam = boss_rush_get_active_player_camera();
        if (cam != nullptr) {
            cam->mCamera.Reset();
            cam->mCamera.Start();
        }

        s_p2Done = true;
        mDoGph_gInf_c::fadeIn(0.2f);
    }
}

static void* delete_hstop_cb(void* actor, void*) {
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(actor);
    if (a != nullptr && fopAcM_GetName(a) == fpcNm_Tag_Hstop_e) {
        fopAcM_delete(a);
    }
    return nullptr;
}

static void update_horsebackganon_instant_fight() {
    static u32 s_camGen = ~0u;
    static int s_demoEndFrames = 0;
    static int s_entrySkipFrames = 0;

    if (!boss_rush_is_fighting_here() || s_returningToChamber) return;
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN09B") != 0) return;
    if (g_dComIfG_gameInfo.info.getDan().isSwitch(1)) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Horseback Ganon") != 0) {
        return;
    }

    if (instant_fight_rearm(s_camGen)) {
        s_demoEndFrames = 25;
        s_entrySkipFrames = 300;
        s_horsebackRetryLanding = false;
    }

    if (s_entrySkipFrames > 0) {
        --s_entrySkipFrames;
        if (dMsgObject_isTalkNowCheck()) {
            dMsgObject_onKillMessageFlag();
        } else if (dComIfGp_event_runCheck()) {
            dComIfGp_event_reset();
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) {
                link->cancelOriginalDemo();
            }
            if (s_entrySkipFrames > 30) s_entrySkipFrames = 30;
        }
        fpcM_Search(delete_hstop_cb, nullptr);
    }

    if (s_demoEndFrames > 0) {
        --s_demoEndFrames;
        if (dDemo_c::m_object != nullptr && dDemo_c::m_object->mpCamera != nullptr) {
            dDemo_c::m_object->mpCamera->mFlags = 0;
        }
        if (dDemo_c::getMode() != 0) {
            dDemo_c::end();
        }
    }

    // Once Ganondorf is defeated (ACTION_HEND) his own b_gnd_h_end() drives a
    // fall-off-horse animation through mDemoCamMode (30 -> 32 -> 34), which
    // needs its own camera control. Forcing QuickStart()/SetTrimSize(0) every
    // frame here fights that and keeps the normal gameplay camera up,
    // hiding the animation entirely - stop doing that once he's down.
    bool ganondorfDown = false;
    if (fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e)) {
        int gam, gmm, gdcm, ghorse, ghp, gkd;
        bbi::ganondorf_read(gnd, gam, gmm, gdcm, ghorse, ghp, gkd);
        ganondorfDown = (gam == 6);
    }
    if (!ganondorfDown) {
        camera_process_class* cam = boss_rush_get_active_player_camera();
        if (cam != nullptr) {
            cam->mCamera.QuickStart();
            cam->mCamera.SetTrimSize(0);
        }
    }
}

static void update_darknut_instant_fight() {
    static bool s_done = false;
    static u32 s_gen = ~0u;
    if (instant_fight_rearm(s_gen)) s_done = false;
    static int  s_frames = 0;

    if (!is_in_chamber_room()) {
        s_done = false;
        s_frames = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;
    if (!boss_rush_is_fighting_here()) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Darknut") != 0) {
        return;
    }

    daB_TN_c* tn = reinterpret_cast<daB_TN_c*>(fopAcM_SearchByName(fpcNm_B_TN_e));
    if (tn == nullptr) return;
    if (tn->mpModelMorf2 == nullptr) return;

    const int a1 = tn->mActionMode1;
    if (a1 == daB_TN_c::ACT_ROOMDEMO || a1 == daB_TN_c::ACT_OPENING) {
        dComIfGp_event_reset();
        Z2GetAudioMgr()->bgmStreamStop(0x1e);
        Z2GetAudioMgr()->subBgmStart(Z2BGM_TN_MBOSS);
        tn->setActionMode(daB_TN_c::ACT_WAITH, daB_TN_c::ACTION2_0_e);
        tn->mUpdateNeckAngle = true;
        tn->mBlendStatus = 2;
        tn->mBlend = 1.0f;
        fopAcM_OffStatus(tn, fopAcStts_UNK_0x4000_e);
        dComIfGs_onOneZoneSwitch(14, fopAcM_GetRoomNo(tn));
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) link->cancelOriginalDemo();
        if (++s_frames < 4) return;
    } else if (s_done) {
        return;
    }
    s_done = true;
}

static int s_armogohmaCamArmFrames = 0;

static void update_armogohma_camera() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN06A") != 0) {
        s_armogohmaCamArmFrames = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) {
        s_armogohmaCamArmFrames = 0;
        return;
    }
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Armogohma") != 0) {
        s_armogohmaCamArmFrames = 0;
        return;
    }

    if (s_armogohmaCamArmFrames <= 0) return;
    --s_armogohmaCamArmFrames;

    daAlink_c* link = daAlink_getAlinkActorClass();
    camera_process_class* cam = boss_rush_get_active_player_camera();
    if (link == nullptr || cam == nullptr) return;

    static const s16 kArmogohmaFightAngle = static_cast<s16>(0x8000);
    const cXyz& p = link->current.pos;
    const f32 fx = cM_ssin(kArmogohmaFightAngle), fz = cM_scos(kArmogohmaFightAngle);
    cXyz center(p.x + fx * 200.0f, p.y + 120.0f, p.z + fz * 200.0f);
    cXyz eye(p.x - fx * 250.0f, p.y + 100.0f, p.z - fz * 250.0f);
    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    cam->mCamera.SetTrimSize(0);
    fopCamM_SetAngleY(cam, kArmogohmaFightAngle);
}

static void update_armogohma_instant_fight() {
    static bool s_done = false;
    static int  s_frames = 0;
    static u32  s_gen = ~0u;
    static u32  s_concludedId = 0;
    static u32  s_seenId = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN06A") != 0) {
        s_done = false;
        s_frames = 0;
        s_concludedId = 0;
        s_seenId = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Armogohma") != 0) {
        return;
    }

    if (instant_fight_rearm(s_gen)) { s_done = false; s_frames = 0; }
    if (s_done) return;

    b_gm_class* gm = reinterpret_cast<b_gm_class*>(fopAcM_SearchByName(fpcNm_B_GM_e));
    if (gm == nullptr) return;
    if (gm->mpModelMorf == nullptr) return;

    const u32 gmId = gm->id;
    if (gmId == s_concludedId) return;
    if (gmId != s_seenId) {
        s_seenId = gmId;
        s_frames = 0;
        s_done = false;
    }

    const s16 dm = gm->mDemoMode;
    if (dm == 0 || dm >= 10) {
        Z2GetAudioMgr()->subBgmStop();
        Z2GetAudioMgr()->bgmStart(Z2BGM_GOMA_BTL01, 0, 0);
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_process_class* camera = boss_rush_get_active_player_camera();
        if (link != nullptr && camera != nullptr) {
            link->current.angle.y = static_cast<s16>(0x8000);
            link->shape_angle.y = static_cast<s16>(0x8000);
            const cXyz& p = link->current.pos;
            const s16 ang = static_cast<s16>(0x8000);
            const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
            cXyz center(p.x + fx * 200.0f, p.y + 120.0f, p.z + fz * 200.0f);
            cXyz eye(p.x - fx * 250.0f, p.y + 100.0f, p.z - fz * 250.0f);
            camera->mCamera.Reset(center, eye);
            camera->mCamera.Start();
            camera->mCamera.SetTrimSize(0);
            fopCamM_SetAngleY(camera, ang);
        }
        s_concludedId = gmId;
        s_done = true;
        return;
    }
    if (dm == 1) {
        camera_process_class* camera = boss_rush_get_active_player_camera();
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (camera != nullptr) camera->mCamera.Stop();
        gm->mDemoMode = 2;
        gm->mDemoModeTimer = 0;
        gm->mDemoCamFovy = 55.0f;
        if (link != nullptr) {
            link->changeOriginalDemo();
            link->changeDemoMode(daPy_demo_c::DEMO_LOOK_AROUND_e, 0, 0, 0);
            const cXyz teleportPos(0.0f, 0.0f, 2391.84f);
            link->setPlayerPosAndAngle(&teleportPos, static_cast<s16>(0x8000), 1);
            link->current.angle.y = static_cast<s16>(0x8000);
            link->shape_angle.y = static_cast<s16>(0x8000);
        }
        dComIfGp_getEvent()->startCheckSkipEdge(gm);
        if (camera != nullptr) {
            camera->mCamera.SetTrimSize(3);
            if (link != nullptr) {
                const cXyz& p = link->current.pos;
                const s16 ang = static_cast<s16>(0x8000);
                const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
                cXyz center(p.x + fx * 200.0f, p.y + 120.0f, p.z + fz * 200.0f);
                cXyz eye(p.x - fx * 250.0f, p.y + 100.0f, p.z - fz * 250.0f);
                camera->mCamera.Reset(center, eye);
                camera->mCamera.Start();
                camera->mCamera.SetTrimSize(0);
                fopCamM_SetAngleY(camera, ang);
            }
        }
        s_armogohmaCamArmFrames = 90;
        return;
    }
    if (dm >= 2) {
        dComIfGp_getEvent()->onFlag2(8);
    }
    if (++s_frames > 240) s_done = true;
}

static void update_zant_instant_fight() {
    static bool s_warpDone = false;
    static u8   s_outfit = dItemNo_WEAR_KOKIRI_e;
    static int  s_rcFromPhase = -1;
    static bool s_oiBootsDone = false;
    static int  s_rcFrames = 0;
    static bool s_leftOiCleaned = false;
    static fpc_ProcID s_zantId = 0;

    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "D_MN08D") != 0) {
        s_warpDone = false; s_outfit = dItemNo_WEAR_KOKIRI_e; s_rcFromPhase = -1;
        s_oiBootsDone = false; s_rcFrames = 0; s_leftOiCleaned = false; s_zantId = 0;
        return;
    }
    if (!is_boss_rush_active() || s_returningToChamber) return;

    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount ||
        std::strcmp(g_bossGalleryTable[t].displayName, "Zant") != 0) {
        return;
    }

    daB_ZANT_c* z = reinterpret_cast<daB_ZANT_c*>(fopAcM_SearchByName(fpcNm_B_ZANT_e));
    if (z == nullptr) return;
    const fpc_ProcID zid = fopAcM_GetID(z);
    if (zid != s_zantId) {
        s_zantId = zid;
        s_warpDone = false; s_outfit = dItemNo_WEAR_KOKIRI_e; s_rcFromPhase = -1;
        s_oiBootsDone = false; s_rcFrames = 0; s_leftOiCleaned = false;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();

    if (!s_warpDone && z->mFightPhase == daB_ZANT_c::PHASE_BB && z->mFightCycle == 0) {
        if (z->mAction != daB_ZANT_c::ACT_WARP) {
            s_warpDone = true;
        } else {
            z->field_0x70b = 1;
            if ((z->mMode == 4 || z->mMode == 5) && z->mModeTimer > 12) {
                z->mModeTimer = 12;
            }
        }
    }

    if (z->mAction == daB_ZANT_c::ACT_ROOM_CHANGE) {
        if (s_rcFromPhase < 0) {
            s_rcFromPhase = z->mFightPhase;
        }
        ++s_rcFrames;
        const int destPhase = s_rcFromPhase + 1;

        const bool toWater = (destPhase == daB_ZANT_c::PHASE_OI);
        const u8   want     = toWater ? static_cast<u8>(dItemNo_WEAR_ZORA_e)
                                      : static_cast<u8>(dItemNo_WEAR_KOKIRI_e);
        const bool timeOk   = toWater ? (s_rcFrames >= 60) : true;

        if (g_configBossRushSuggestedItems && want != s_outfit && timeOk) {
            s_outfit = want;
            dMeter2Info_setCloth(want, false);
            dComIfGs_setSelectEquipClothes(want);
            dComIfGp_setSelectEquipClothes(want);
            if (link != nullptr) {
                link->setClothesChange(0);
                link->setSelectEquipItem(FALSE);
                if (toWater) {
                    assign_select_item(SELECT_ITEM_X, SLOT_3);
                    assign_select_item(SELECT_ITEM_Y, SLOT_10);
                    link->setHeavyBoots(1);
                }
            }
        }

        if (s_rcFromPhase == daB_ZANT_c::PHASE_OI && link != nullptr && !s_leftOiCleaned) {
            link->offNoResetFlg0(daPy_py_c::daPy_FLG0(
                daPy_py_c::FLG0_WATER_IN_MOVE | daPy_py_c::FLG0_SWIM_UP));
            if (link->checkEquipHeavyBoots()) link->setHeavyBoots(0);
            s_oiBootsDone = false;
            s_leftOiCleaned = true;
        }
    } else {
        s_rcFromPhase = -1;
        s_rcFrames = 0;
        s_leftOiCleaned = false;
    }

    if (g_configBossRushSuggestedItems && link != nullptr && !link->checkEventRun() &&
        z->mAction != daB_ZANT_c::ACT_ROOM_CHANGE) {
        const bool inWater = link->checkModeFlg(0x40000) ||
                             link->mWaterY > link->current.pos.y + 20.0f;
        if (z->mFightPhase == daB_ZANT_c::PHASE_OI) {
            if (!link->checkEquipHeavyBoots()) {
                link->setHeavyBoots(1);
            }
        } else if (z->mFightPhase == daB_ZANT_c::PHASE_MK && link->checkEquipHeavyBoots() && !inWater) {
            link->setHeavyBoots(0);
            s_oiBootsDone = false;
        }
    }

    static bool s_prevRoomChange = false;
    static int  s_oiCamFrames = 0;
    const bool nowRoomChange = (z->mAction == daB_ZANT_c::ACT_ROOM_CHANGE);
    if (s_prevRoomChange && !nowRoomChange && z->mFightPhase == daB_ZANT_c::PHASE_OI) {
        s_oiCamFrames = 10;
    }
    s_prevRoomChange = nowRoomChange;
    if (s_oiCamFrames > 0 && link != nullptr) {
        --s_oiCamFrames;
        camera_class* cam = static_cast<camera_class*>(dComIfGp_getCamera(0));
        if (cam != nullptr) {
            const cXyz& p = link->current.pos;
            cam->view.lookat.center.set(p.x, p.y + 100.0f, p.z + 200.0f);
            cam->view.lookat.eye.set(p.x, p.y + 170.0f, p.z - 450.0f);
            fopCamM_SetAngleY(cam, static_cast<s16>(0));
        }
    }
}

static bool boss_starts_with_sword_drawn(const BossGalleryEntry& boss) {
    const char* n = boss.displayName;
    if (std::strcmp(n, "Ook") == 0) return true;
    if (std::strcmp(n, "Dangoro") == 0) return true;
    if (std::strcmp(n, "Deku Toad") == 0) return true;
    if (std::strcmp(n, "Darkhammer") == 0) return true;
    if (std::strcmp(n, "Darknut") == 0) return true;
    if (std::strcmp(n, "Puppet Zelda") == 0) return true;
    if (std::strcmp(n, "Ganondorf") == 0) return true;
    return false;
}

static int gauntlet_next_phase_index(const char* currentName) {
    const char* want = nullptr;
    if (std::strcmp(currentName, "Ganondorf") == 0) want = "Puppet Zelda";
    else if (std::strcmp(currentName, "Puppet Zelda") == 0) want = "Beast Ganon";
    else if (std::strcmp(currentName, "Beast Ganon") == 0) want = "Horseback Ganon";
    else if (std::strcmp(currentName, "Horseback Ganon") == 0) want = "Ganondorf";
    if (want == nullptr) return -1;
    for (size_t i = 0; i < g_bossGalleryCount; ++i) {
        if (std::strcmp(g_bossGalleryTable[i].displayName, want) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static bool fight_starts_with_sword_drawn() {
    const int t = boss_rush_target_index();
    if (t < 0 || static_cast<size_t>(t) >= g_bossGalleryCount) return false;
    return boss_starts_with_sword_drawn(g_bossGalleryTable[t]);
}

static void* settle_freeze_judge(void* actor, void*) {
    if (actor == nullptr || actor == static_cast<void*>(daAlink_getAlinkActorClass())) {
        return nullptr;
    }
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(actor);
    fopAcM_OnStatus(a, fopAcStts_NOEXEC_e);
    fopAcM_OnCondition(a, fopAcCnd_NODRAW_e);
    return nullptr;
}

static void* settle_unfreeze_judge(void* actor, void*) {
    if (actor == nullptr) {
        return nullptr;
    }
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(actor);
    fopAcM_OffStatus(a, fopAcStts_NOEXEC_e);
    fopAcM_OffCondition(a, fopAcCnd_NODRAW_e);
    return nullptr;
}

static void show_statue_fight_a_status();
static HookAction on_boss_rush_alink_execute_pre(ModContext*, void*, void*, void*) {
    if (!is_boss_rush_active() || boss_rush_target_index() < 0 || s_returningToChamber) {
        s_arenaSettle = 0;
        return HOOK_CONTINUE;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    const bool inArena = !is_in_boss_rush_chamber();
    JUTFader* fader = mDoGph_gInf_c::getFader();
    const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
    const bool isBlackScreen = (faderStatus == JUTFader::None || fopOvlpM_IsPeek());

    if (inArena && fight_starts_with_sword_drawn() && !link->checkWolf() && isBlackScreen) {
        if (link->mEquipItem == 0x103) {
            s_swordDrawnLatched = true;
        } else if (!s_swordDrawnLatched) {
            link->swordEquip(TRUE);
            link->setSwordModel();
            s_swordDrawnLatched = true;
        }
    }

    if (inArena && isBlackScreen && !link->checkEquipHeavyBoots()) {
        const int t = boss_rush_target_index();
        if (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount &&
            std::strcmp(g_bossGalleryTable[t].displayName, "Morpheel") == 0) {
            link->setHeavyBoots(1);
        }
    }

    static bool s_zeldaPhaseSwordLatched = false;
    static int s_zeldaPhaseFrames = -1;
    static int s_zeldaPhaseDrawStable = 0;
    {
        fopAc_ac_c* hzelda = fopAcM_SearchByName(fpcNm_E_HZELDA_e);
        if (hzelda == nullptr) {
            s_zeldaPhaseSwordLatched = false;
            s_zeldaPhaseFrames = -1;
            s_zeldaPhaseDrawStable = 0;
        } else if (inArena && !s_zeldaPhaseSwordLatched && !link->checkWolf()) {
            const int t = boss_rush_target_index();
            const bool zeldaFight =
                (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount &&
                 (!g_configBossRushSeparateGanon
                      ? std::strcmp(g_bossGalleryTable[t].displayName, "Ganondorf") == 0
                      : std::strcmp(g_bossGalleryTable[t].displayName, "Puppet Zelda") == 0));
            if (zeldaFight) {
                if (s_zeldaPhaseFrames < 0) {
                    s_zeldaPhaseFrames = 0;
                }
                const bool quiet = !link->checkEventRun() && !dComIfGp_event_runCheck();
                if (quiet && s_zeldaPhaseFrames <= 600 && link->mEquipItem != 0x103) {
                    link->swordEquip(TRUE);
                    link->setSwordModel();
                    boss_rush_debug_log("[zelda] draw fired frame=%d", s_zeldaPhaseFrames);
                }
                if (quiet && link->mEquipItem == 0x103) {
                    ++s_zeldaPhaseDrawStable;
                    if (s_zeldaPhaseDrawStable >= 30) {
                        s_zeldaPhaseSwordLatched = true;
                        boss_rush_debug_log("[zelda] sword stable, latch on frame=%d",
                                            s_zeldaPhaseFrames);
                    }
                } else {
                    s_zeldaPhaseDrawStable = 0;
                }
                ++s_zeldaPhaseFrames;
            }
        }
    }

    static bool s_duelPhaseSwordLatched = false;
    static bool s_duelPhaseSwordFired = false;
    {
        fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e);
        const bool isGroundMode = (s_gauntletPhase == 4 ||
                                   g_dComIfG_gameInfo.info.getDan().isSwitch(1) ||
                                   dComIfGs_isSaveDunSwitch(1));
        if (gnd == nullptr || !isGroundMode) {
            s_duelPhaseSwordLatched = false;
            s_duelPhaseSwordFired = false;
        } else if (inArena && !s_duelPhaseSwordLatched && !link->checkWolf()) {
            if (link->mEquipItem == 0x103) {
                s_duelPhaseSwordLatched = true;
            } else if (!s_duelPhaseSwordFired) {
                const bool quiet = !link->checkEventRun() && !dComIfGp_event_runCheck();
                if (quiet) {
                    link->swordEquip(TRUE);
                    link->setSwordModel();
                    s_duelPhaseSwordFired = true;
                    boss_rush_debug_log("[ganondorf] duel sword draw fired once");
                }
            }
        }
    }

    const int targetIdx = boss_rush_target_index();
    const bool isMorpheelTarget = (targetIdx >= 0 && static_cast<size_t>(targetIdx) < g_bossGalleryCount &&
                                  std::strcmp(g_bossGalleryTable[targetIdx].displayName, "Morpheel") == 0);
    if (isMorpheelTarget && !is_in_boss_rush_chamber() && !link->checkWolf()) {
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        const bool isBlackScreen = (faderStatus == JUTFader::None || fopOvlpM_IsPeek());
        if (isBlackScreen) {
            fopAc_ac_c* ob = fopAcM_SearchByName(fpcNm_B_OB_e);
            const bool phase2 = (ob != nullptr && bbi::morpheel_is_phase2(ob));
            if (!phase2) {
                if (dComIfGs_getSelectEquipClothes() != dItemNo_WEAR_ZORA_e) {
                    dMeter2Info_setCloth(dItemNo_WEAR_ZORA_e, false);
                    dComIfGs_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
                    dComIfGp_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
                    link->setClothesChange(0);
                    link->setSelectEquipItem(FALSE);
                }
                if (!link->checkEquipHeavyBoots()) {
                    link->setHeavyBoots(1);
                }
            }

            link->current.pos = kMorpheelFightSpawnPos;
            link->old.pos = kMorpheelFightSpawnPos;
            link->current.angle.set(0, kMorpheelFightAngle, 0);
            link->shape_angle.set(0, kMorpheelFightAngle, 0);
            link->speed.set(0.0f, 0.0f, 0.0f);
            link->speedF = 0.0f;

            if (dDemo_c::m_object != nullptr && dDemo_c::m_object->mpCamera != nullptr) {
                dDemo_c::m_object->mpCamera->mFlags = 0;
            }
            if (dDemo_c::getMode() != 0) {
                dDemo_c::end();
            }

            camera_process_class* cam = boss_rush_get_active_player_camera();
            if (cam != nullptr) {
                const f32 fx = cM_ssin(kMorpheelFightAngle), fz = cM_scos(kMorpheelFightAngle);
                cXyz center(kMorpheelFightSpawnPos.x + fx * 200.0f, kMorpheelFightSpawnPos.y + 100.0f, kMorpheelFightSpawnPos.z + fz * 200.0f);
                cXyz eye(kMorpheelFightSpawnPos.x - fx * 450.0f, kMorpheelFightSpawnPos.y + 170.0f, kMorpheelFightSpawnPos.z - fz * 450.0f);
                cam->mCamera.Reset(center, eye);
                cam->mCamera.Start();
                cam->mCamera.QuickStart();
                cam->mCamera.SetTrimSize(0);
                cam->view.lookat.center.set(center.x, center.y, center.z);
                cam->view.lookat.eye.set(eye.x, eye.y, eye.z);
                fopCamM_SetAngleY(cam, kMorpheelFightAngle);
            }
            s_morpheelPosPinFrames = 0;
            s_morpheelCamArmFrames = 90;
        }
    }

    if (s_arenaSettle > 0) {
        s_arenaSettle--;

        const int t = boss_rush_target_index();
        const bool isMorpheel = (t >= 0 && static_cast<size_t>(t) < g_bossGalleryCount &&
                                 std::strcmp(g_bossGalleryTable[t].displayName, "Morpheel") == 0);
        if (!isMorpheel && link->checkEquipHeavyBoots()) {
            link->setHeavyBoots(0);
        }

        if (s_arenaSettle > s_arenaFreezeUntil) {
            fopAcIt_Judge(settle_freeze_judge, nullptr);
        } else if (s_arenaSettle == s_arenaFreezeUntil) {
            fopAcIt_Judge(settle_unfreeze_judge, nullptr);
        }
    }
    return HOOK_CONTINUE;
}

static void on_boss_rush_alink_execute_post(ModContext*, void*, void*, void*) {
    daAlink_c* link = daAlink_getAlinkActorClass();

    update_boss_rush_master_sword_effects();

    show_statue_fight_a_status();

    update_morpheel_pos_pin();
    update_deathsword_auto_wolf();
    update_beastganon_instant_fight(true);

    update_armogohma_camera();
    update_dangoro_camera();
    update_ganon_ground_duel();
    update_morpheel_iron_boots();
    update_darkhammer_instant_fight();
    update_dangoro_instant_fight();
    update_ook_instant_fight();
    update_diababa_instant_fight();
    update_fyrus_instant_fight();
    update_fyrus_intro_watchdog();
    update_dekutoad_instant_fight();
    update_morpheel_instant_fight();
    update_deathsword_instant_fight();
    update_blizzeta_instant_fight();
    update_stallord_instant_fight();
    update_stallord_phase_transition_skip();
    update_argorok_phase_transition_skip();
    update_horsebackganon_instant_fight();
    update_darknut_instant_fight();
    update_armogohma_instant_fight();
    update_zant_instant_fight();
    fix_boss_rush_miniboss_bgm();
    boss_rush_timer_update();


    if (link == nullptr || (!s_bossRushModeActive && !s_exitingBossRush) || !is_in_boss_rush_chamber()) {
        if (any_ring_flames_lit()) clear_ring_flames();
        return;
    }

    update_ring_flames();

    if (s_bossRushModeActive && !s_exitingBossRush && !s_pendingInitialInventory &&
        s_pendingFightIndex == -1) {
        const u16 chamberFull = full_life_for_max(kChamberFullLife);
        if (dComIfGs_getMaxLife() != kChamberFullLife || dComIfGs_getLife() != chamberFull) {
            dComIfGs_setMaxLife(static_cast<u8>(kChamberFullLife));
            dComIfGs_setLife(chamberFull);
            sync_life_meter_instant(chamberFull, kChamberFullLife);
        }
    }
}

static void on_boss_rush_meter_draw_post(ModContext*, void* args, void*, void*) {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return;
    }

    draw_boss_rush_fight_timer();

    if (is_ui_or_menu_active()) {
        return;
    }

    const bool inChamberForText = is_in_boss_rush_chamber();
    if (!kBossGalleryTextsEnabled || !inChamberForText || s_returningToChamber) {
        return;
    }

    draw_boss_rush_texts(kBossChamberFloorY);
}

static bool link_near_gallery_statue() {
    if (!is_in_boss_rush_chamber()) {
        return false;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return false;
    }
    const size_t activeCount = boss_rush_get_active_gallery_count();
    for (size_t circleSlot = 0; circleSlot < activeCount; ++circleSlot) {
        const size_t i = boss_rush_get_active_gallery_table_index(circleSlot);
        cXyz bossPos;
        csXyz bossAngle;
        boss_rush_get_slot_transform(circleSlot, activeCount, kBossChamberFloorY, bossPos, bossAngle);
        const f32 dx = link->current.pos.x - bossPos.x;
        const f32 dz = link->current.pos.z - bossPos.z;
        if (dx * dx + dz * dz < kBossInteractRadius * kBossInteractRadius) {
            return true;
        }
    }
    return false;
}

static void show_statue_fight_a_status() {
    // Master sword spawn/prompt disabled for now.
    if (link_near_gallery_statue() /* || boss_rush_master_sword_near() */) {
        g_dComIfG_gameInfo.play.setDoStatus(BUTTON_STATUS_OPEN, BUTTON_STATUS_FLAG_NONE);
    }
}

DEFINE_HOOK(&dMeter2Draw_c::getActionString, BossRushActionStringHook);

static void on_action_string_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || !is_in_boss_rush_chamber()) {
        return;
    }
    if (dComIfGp_getDoStatus() != BUTTON_STATUS_OPEN) {
        return;
    }
    if (mods::arg<u8>(args, 1) != BUTTON_STATUS_OPEN) {
        return;
    }

    static char fight[] = "Fight";
    // static char startBossRush[] = "Start boss rush";
    static char leave[] = "Leave Boss Rush";
    if (link_near_gallery_statue()) {
        *static_cast<char**>(retval) = fight;
    // } else if (boss_rush_master_sword_near()) {
    //     *static_cast<char**>(retval) = startBossRush;
    } else {
        *static_cast<char**>(retval) = leave;
    }
}

static HookAction on_boss_rush_meter_draw_pre(ModContext*, void* args, void*, void*) {
    if (is_ui_or_menu_active()) {
        return HOOK_CONTINUE;
    }
    dMeter2Draw_c* meterDraw = args ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;

    if (is_boss_rush_ganon_fight()) {
        dComIfGs_offEventBit(dSv_event_flag_c::F_0800);
        dComIfGs_onEventBit(dSv_event_flag_c::M_067);
        dComIfGs_onEventBit(0x0540);
        dMeter2Info_onUseButton(METER2_USEBUTTON_Z);
        if (meterDraw != nullptr) {
            meterDraw->mButtonZAlpha = 1.0f;
            meterDraw->field_0x724 = 1.0f;
            if (meterDraw->mpButtonMidona != nullptr) {
                meterDraw->mpButtonMidona->show();
            }
        }
    }

    return HOOK_CONTINUE;
}

}

bool boss_rush_is_hud_menu_blocking() {
    return is_ui_or_menu_active();
}

bool boss_rush_settle_window_active() {
    return s_arenaSettle > 0;
}

void boss_rush_request_retry() {
    if (!is_boss_rush_active() || !boss_rush_is_fight_engaged()) return;
    if (!is_boss_rush_transition_in_flight()) {
        boss_rush_retry_current_fight(s_logSvc, s_modCtx);
        return;
    }
}

static bool s_bossRushDeathHandledTriggered = false;

static HookAction on_proc_co_dead_pre(ModContext*, void* args, void* retval, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || retval == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!is_boss_rush_active()) {
        s_bossRushDeathHandledTriggered = false;
        return HOOK_CONTINUE;
    }
    if (!s_bossRushDeathHandledTriggered && !boss_rush_is_fight_engaged()) {
        return HOOK_CONTINUE;
    }
    if (!s_bossRushDeathHandledTriggered) {
        if (is_boss_rush_mod_warp_in_flight()) {
            return HOOK_CONTINUE;
        }
        s_bossRushDeathHandledTriggered = true;
        return_to_boss_rush_chamber(s_logSvc, s_modCtx, "Died");
    }
    *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK_SYMBOL("canAutoSave", bool(), BossRushCanAutoSaveHook);

static HookAction on_boss_rush_can_auto_save_pre(ModContext*, void*, void* retval, void*) {
    if (!s_bossRushModeActive && !s_exitingBossRush) return HOOK_CONTINUE;
    if (retval != nullptr) *static_cast<bool*>(retval) = false;
    return HOOK_SKIP_ORIGINAL;
}

static bool* s_engineShouldAutoSave = nullptr;
static bool s_shouldAutoSaveCaptured = false;
static bool s_shouldAutoSaveOriginal = false;

static void update_boss_rush_autosave_suppression() {
    if (s_engineShouldAutoSave == nullptr) {
        return;
    }
    if (s_bossRushModeActive || s_exitingBossRush) {
        if (!s_shouldAutoSaveCaptured) {
            s_shouldAutoSaveCaptured = true;
            s_shouldAutoSaveOriginal = *s_engineShouldAutoSave;
        }
        *s_engineShouldAutoSave = false;
    } else if (s_shouldAutoSaveCaptured) {
        *s_engineShouldAutoSave = s_shouldAutoSaveOriginal;
        s_shouldAutoSaveCaptured = false;
    }
}

using EngineTriggerAutoSaveFn = void (*)();
static EngineTriggerAutoSaveFn s_engineTriggerAutoSave = nullptr;
static u8* s_engineAutoSaveProc = nullptr;

using BossRushGetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
static BossRushGetConfigVarFn s_bossRushGetConfigVar = nullptr;
static dusk::config::ConfigVarBase* s_bossRushAutoSaveVar = nullptr;

static void init_boss_rush_qol_overrides(const HookService* hook_svc) {
    if (hook_svc == nullptr) return;

    mods::hook::add_pre<BossRushCanAutoSaveHook>(hook_svc, on_boss_rush_can_auto_save_pre);

    void* addr = nullptr;
    const bool canAutoSaveResolved =
        hook_svc->resolve(s_modCtx, "canAutoSave", &addr, nullptr) == MOD_OK;
    if (hook_svc->resolve(s_modCtx, "dusk::config::GetConfigVar", &addr, nullptr) == MOD_OK) {
        s_bossRushGetConfigVar = reinterpret_cast<BossRushGetConfigVarFn>(addr);
        s_bossRushAutoSaveVar = s_bossRushGetConfigVar("game.autoSave");
        s_bossRushFastTransitionsVar =
            static_cast<dusk::config::ConfigVar<bool>*>(s_bossRushGetConfigVar("game.fastTransitions"));
    }

    addr = nullptr;
    if (hook_svc->resolve(s_modCtx, "triggerAutoSave", &addr, nullptr) == MOD_OK) {
        s_engineTriggerAutoSave = reinterpret_cast<EngineTriggerAutoSaveFn>(addr);
    }
    addr = nullptr;
    if (hook_svc->resolve(s_modCtx, "mAutoSaveProc", &addr, nullptr) == MOD_OK) {
        s_engineAutoSaveProc = static_cast<u8*>(addr);
    }
    addr = nullptr;
    if (hook_svc->resolve(s_modCtx, "shouldAutoSave", &addr, nullptr) == MOD_OK) {
        s_engineShouldAutoSave = static_cast<bool*>(addr);
    }
}

DEFINE_HOOK_SYMBOL("dusk::AchievementSystem::tick", void(void*), BossRushAchievementTickHook);

static HookAction on_boss_rush_achievement_tick_pre(ModContext*, void*, void*, void*) {
    if (s_bossRushModeActive || s_exitingBossRush) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

bool boss_rush_is_fight_engaged() {
    if (!is_boss_rush_active() || s_returningToChamber) {
        return false;
    }
    return boss_rush_current_target_index() >= 0;
}

bool is_in_boss_rush_chamber() {
    if (s_activeFightIndex != -1 || s_returningToChamber || s_pendingFightFromArena) {
        return false;
    }
    if (s_pendingFightIndex != -1) {
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        if (faderStatus == JUTFader::None || fopOvlpM_IsPeek()) {
            return false;
        }
    }
    // The chamber room (D_MN06B room 51) is reachable in vanilla progression
    // (Temple of Time darknut hall), so only treat it as the boss rush chamber
    // while a boss rush session is actually running.
    if (!s_bossRushModeActive && !s_exitingBossRush) {
        return false;
    }
    return is_in_chamber_room();
}

bool is_boss_rush_active() {
    if (s_pendingInitialInventory || s_exitingBossRush) {
        return false;
    }
    return s_bossRushModeActive;
}

static void clear_boss_dungeon_clear_flags(const char* stage) {
    if (stage == nullptr) return;
    struct Entry { const char* stage; int saveTbl; int sw; u16 clearFlag; };
    static const Entry kTbl[] = {
        {"D_MN05A", 2,  0x01, dSv_event_flag_c::M_022},
        {"D_MN04A", 3,  0x7c, dSv_event_flag_c::M_031},
        {"D_MN01A", 4,  0x0e, dSv_event_flag_c::M_045},
        {"D_MN10A", 10, 0x0a, dSv_event_flag_c::F_0265},
        {"D_MN11A", 8,  0x19, dSv_event_flag_c::F_0266},
        {"D_MN06A", 7,  0x18, dSv_event_flag_c::F_0267},
        {"D_MN07A", 22, 0x25, dSv_event_flag_c::F_0268},
    };
    for (const Entry& e : kTbl) {
        if (std::strcmp(stage, e.stage) != 0) continue;
        dComIfGs_offStageSwitch(e.saveTbl, e.sw);
        dComIfGs_offEventBit(e.clearFlag);
        return;
    }
}

bool boss_rush_scene_load_stable() {
    const char* cur = dComIfGp_getStartStageName();
    const char* next = dComIfGp_getNextStageName();
    if (next != nullptr && next[0] != '\0' &&
        (cur == nullptr || std::strcmp(next, cur) != 0)) {
        return false;
    }

    static char s_stableStage[16] = {0};
    static s32 s_stableRoom = -1;
    static int s_stableFrames = 0;

    const char* stage = (cur != nullptr) ? cur : "";
    daAlink_c* link = daAlink_getAlinkActorClass();
    s32 room = (link != nullptr) ? fopAcM_GetRoomNo(link) : -1;

    if (std::strncmp(s_stableStage, stage, sizeof(s_stableStage) - 1) != 0 ||
        s_stableRoom != room) {
        std::strncpy(s_stableStage, stage, sizeof(s_stableStage) - 1);
        s_stableStage[sizeof(s_stableStage) - 1] = '\0';
        s_stableRoom = room;
        s_stableFrames = 0;
        return false;
    }
    if (s_stableFrames < 2) {
        s_stableFrames++;
        return false;
    }
    return true;
}

static void prepare_boss_rush_state() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    const char* curStage = dComIfGp_getStartStageName();
    if (!is_in_chamber_room() && curStage != nullptr && curStage[0] != '\0') {
        std::strncpy(s_savedLocation.stage, curStage, sizeof(s_savedLocation.stage) - 1);
        s_savedLocation.stage[sizeof(s_savedLocation.stage) - 1] = '\0';
        s_savedLocation.room = static_cast<s8>(dComIfGp_roomControl_getStayNo());
        s_savedLocation.layer = static_cast<s8>(dComIfG_play_c::getLayerNo(0));
        if (link != nullptr) {
            s_savedLocation.pos = link->current.pos;
            s_savedLocation.angle = link->shape_angle.y;
        } else {
            s_savedLocation.pos.set(0.0f, 0.0f, 0.0f);
            s_savedLocation.angle = 0;
        }
        s_hasSavedLocation = true;
        persist_saved_location_to_disk();

        s_chamberEquipsPending = true;

    }

    persist_boss_rush_session_marker();
    s_bossRushModeActive = true;
    force_boss_rush_fast_transitions();
    s_activeFightIndex = -1;
    s_pendingFightIndex = -1;
    s_returningToChamber = false;
    s_returnSawFadeOut = false;
    s_exitingBossRush = false;
    s_needsChamberSpawn = true;
    s_chamberSpawnFrames = 0;
    s_chamberCamArmFrames = 0;
    s_pendingInitialInventory = true;
    reset_boss_rush_save_flags();
}

enum class BossRushEntryGate { None, Menu, Portal };
static BossRushEntryGate s_entryGate = BossRushEntryGate::None;
static int s_entryGateFrames = 0;
static bool s_entrySawSaveBusy = false;
static bool s_entrySaveForcedOverride = false;
static constexpr int kEntrySaveNeverStartedFrames = 10;
static constexpr int kEntrySaveNoPollGraceFrames = 90;
static constexpr int kEntrySaveTimeoutFrames = 600;

static bool boss_rush_begin_pre_entry_save();
static void update_boss_rush_entry_gate();
static void start_boss_rush_entry_warp();
static void start_boss_rush_dungeon_warp_entry();

void start_boss_rush() {
    if (!s_bossRushModeActive && s_entryGate == BossRushEntryGate::None) {
        if (boss_rush_begin_pre_entry_save()) {
            s_entryGate = BossRushEntryGate::Menu;
            return;
        }
    }
    start_boss_rush_entry_warp();
}

static void start_boss_rush_entry_warp() {
    if (daAlink_getAlinkActorClass() != nullptr) {
        start_boss_rush_dungeon_warp_entry();
        return;
    }

    prepare_boss_rush_state();
    custom_equip_set_suppressed(true);

    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);

    s_chamberEquipsPending = true;

    cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
    dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
    dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
    dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                          kBossRushChamberLayer, 0.0f, 0, 1, 0, cM_deg2s(180.0f), 0, 0);

}

static HookAction on_dungeon_return_warp_pre(ModContext*, void*, void*, void*) {
    if (!s_dungeonClearWarpPending) {
        return HOOK_CONTINUE;
    }

    if (s_exitWarpViaDissolve) {
        s_dungeonClearWarpPending = false;
        s_dungeonClearWarpFrames  = 0;
        if (s_portalCineWasHuman) {
            human_warp_cinematic_end();
            s_portalCineWasHuman = false;
        }
        return HOOK_SKIP_ORIGINAL;
    }

    s_dungeonClearWarpPending = false;
    s_dungeonClearWarpFrames  = 0;

    s_equipSuppressWaitBlack = true;
    s_equipSuppressWaitBlackFrames = 0;
    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);

    s_chamberEquipsPending = true;

    cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
    dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
    dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
    dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                          kBossRushChamberLayer, 0.0f, 0, 1, 0, cM_deg2s(180.0f), 1, 0);

    return HOOK_SKIP_ORIGINAL;
}

static HookAction on_skip_portal_obj_warp_pre(ModContext*, void*, void*, void*) {
    if (!s_dungeonClearWarpPending) {
        return HOOK_CONTINUE;
    }

    if (s_exitWarpViaDissolve) {
        s_dungeonClearWarpPending = false;
        s_dungeonClearWarpFrames  = 0;
        if (s_portalCineWasHuman) {
            human_warp_cinematic_end();
            s_portalCineWasHuman = false;
        }
        return HOOK_SKIP_ORIGINAL;
    }

    s_dungeonClearWarpPending = false;
    s_dungeonClearWarpFrames  = 0;

    if (s_portalCineWasHuman) {
        human_warp_cinematic_end();
    }

    s_equipSuppressWaitBlack = true;
    s_equipSuppressWaitBlackFrames = 0;
    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);

    s_chamberEquipsPending = true;

    cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
    dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
    dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
    dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                          kBossRushChamberLayer, 0.0f, 0, 1, 0, cM_deg2s(180.0f), 1, 0);


    return HOOK_SKIP_ORIGINAL;
}

static void start_boss_rush_dungeon_warp_entry() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        start_boss_rush_entry_warp();
        return;
    }

    s_portalCineWasHuman = true;

    s_portalArrivalAnimPending = true;

    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);

    prepare_boss_rush_state();

    s_chamberEquipsPending = true;

    cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
    dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
    dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));

    g_meter2_info.setWarpInfo(kBossRushChamberStage, spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom, 0, 0);

    s_dungeonClearWarpPending = true;
    s_dungeonClearWarpFrames  = 0;

    link->allUnequip(0);
    link->mNormalSpeed = 0.0f;
    link->speed.set(0.0f, 0.0f, 0.0f);

    if (link->procCoWarpInit(0, 1)) {
        link->field_0x347c = 4.6f;
        if (daPy_py_c::checkNowWolf()) {
            daMidna_c* midna = daPy_py_c::getMidnaActor();
            if (midna != nullptr) {
                midna->changeDemoMode(9);
            }
        }
        if (s_portalCineWasHuman) {
            human_warp_cinematic_departure();
        }
        return;
    }

    s_dungeonClearWarpPending = false;
    start_boss_rush_entry_warp();
}

void start_boss_rush_dungeon_warp() {
    if (!s_bossRushModeActive && s_entryGate == BossRushEntryGate::None) {
        if (boss_rush_begin_pre_entry_save()) {
            s_entryGate = BossRushEntryGate::Portal;
            return;
        }
    }
    start_boss_rush_dungeon_warp_entry();
}

static bool boss_rush_begin_pre_entry_save() {
    if (s_engineTriggerAutoSave == nullptr) {
        return false;
    }
    auto* var = static_cast<dusk::config::ConfigVar<bool>*>(s_bossRushAutoSaveVar);
    s_entrySaveForcedOverride = false;
    if (var != nullptr && !var->getValue()) {
        var->setOverrideValue(true);
        s_entrySaveForcedOverride = true;
    }
    s_entryGateFrames = 0;
    s_entrySawSaveBusy = false;
    s_engineTriggerAutoSave();
    return true;
}

static void boss_rush_finish_entry_gate() {
    if (s_entrySaveForcedOverride) {
        auto* var = static_cast<dusk::config::ConfigVar<bool>*>(s_bossRushAutoSaveVar);
        if (var != nullptr) {
            var->clearOverride();
        }
        s_entrySaveForcedOverride = false;
    }
    const BossRushEntryGate gate = s_entryGate;
    s_entryGate = BossRushEntryGate::None;
    if (gate == BossRushEntryGate::Menu) {
        start_boss_rush_entry_warp();
    } else if (gate == BossRushEntryGate::Portal) {
        start_boss_rush_dungeon_warp_entry();
    }
}

static void update_boss_rush_entry_gate() {
    ++s_entryGateFrames;
    bool done = false;
    if (s_engineAutoSaveProc != nullptr) {
        if (*s_engineAutoSaveProc != 0) {
            s_entrySawSaveBusy = true;
        }
        done = (s_entrySawSaveBusy && (*s_engineAutoSaveProc == 0 || *s_engineAutoSaveProc >= 3)) ||
               (!s_entrySawSaveBusy && s_entryGateFrames >= kEntrySaveNeverStartedFrames) ||
               s_entryGateFrames >= kEntrySaveTimeoutFrames;
    } else {
        done = s_entryGateFrames >= kEntrySaveNoPollGraceFrames;
    }
    if (done) {
        boss_rush_finish_entry_gate();
    }
}

void start_boss_rush_map_portal_warp() {
    start_boss_rush_dungeon_warp();
}

static bool s_exitWarpArmed = false;

static void install_boss_rush_exit_save() {
    const u8 slot = dComIfGs_getDataNum();
    dComIfGs_setCardToMemory(s_exitCardBuf, slot);

    for (int i = 0; i < 4; ++i) {
        dComIfGp_setSelectItem(i);
    }
    dComIfGp_setSelectEquipClothes(dComIfGs_getSelectEquipClothes());
    dComIfGp_setSelectEquipSword(dComIfGs_getSelectEquipSword());
    dComIfGp_setSelectEquipShield(dComIfGs_getSelectEquipShield());

    custom_equip_set_suppressed(false);
    custom_equip_restore_from_save();
}

static void update_boss_rush_exit_save_reload() {
    if (!s_exitSaveReloadPending) {
        return;
    }

    if (!s_exitCardDataReady && ++s_exitSaveReloadFrames >= kExitSaveReloadTimeoutFrames) {
        s_exitSaveReloadPending = false;
        s_exitCardLoadArmed = false;
        s_exitingBossRush = false;
        s_bossRushModeActive = true;
        return;
    }

    if (!s_exitCardDataReady) {
        if (!s_exitCardLoadArmed) {
            mDoMemCd_Load();
            s_exitCardLoadArmed = true;
        }

        const s32 ret = g_mDoMemCd_control.LoadSync(s_exitCardBuf, sizeof(s_exitCardBuf), 0);
        if (ret == 1) {
            s_exitCardDataReady = true;
        } else if (ret == 2) {
            mDoMemCd_Load();
        }
    }
    if (s_exitWarpViaDissolve) {
        if (!s_dungeonClearWarpPending || s_exitSaveReloadFrames >= 600) {
            s_exitWarpViaDissolve = false;
            s_dungeonClearWarpPending = false;
            human_warp_arm_arrival_replay();
        } else {
            return;
        }
    }

    if (s_exitCardDataReady && !s_exitWarpArmed) {
        SavedPlayerLocation loc;
        bool haveLoc = s_hasSavedLocation;
        if (haveLoc) {
            loc = s_savedLocation;
        } else if (svc_save != nullptr && s_modCtx != nullptr) {
            size_t blobSize = sizeof(loc);
            if (svc_save->get_blob(s_modCtx, kBossRushLocationBlobName, &loc, &blobSize) == MOD_OK &&
                blobSize == sizeof(loc)) {
                haveLoc = true;
            }
        }

        if (haveLoc) {
            loc.stage[sizeof(loc.stage) - 1] = '\0';
            dComIfGs_setRestartRoom(loc.pos, loc.angle, loc.room);
            dComIfGs_setRestartRoomParam((loc.room & 0x3F) | (0xFF << 24));
            dComIfGp_setNextStage(loc.stage, -1, loc.room, loc.layer, 0.0f, 0, 1, 0, 0, 0, 0);
            s_exitWarpArmed = true;
        } else {
            const u8 slot = dComIfGs_getDataNum();
            dSv_save_c save;
            std::memcpy(&save, s_exitCardBuf + slot * QUEST_LOG_SIZE, sizeof(dSv_save_c));
            dSv_player_return_place_c& rp = save.getPlayer().getPlayerReturnPlace();

            dComIfGp_setNextStage(rp.getName(), rp.getPlayerStatus(), rp.getRoomNo(), -1,
                                  0.0f, 0, 1, 0, 0, 0, 0);
        }
    }

    if (s_exitWarpArmed) {
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        if (faderStatus == JUTFader::None || !is_in_chamber_room()) {
            install_boss_rush_exit_save();
            s_exitSaveReloadPending = false;
        }
    }
}

static bool is_title_or_menu_stage(const char* stage) {
    if (stage == nullptr) return false;
    return (std::strcmp(stage, "name") == 0 ||
            std::strcmp(stage, "Name") == 0 ||
            std::strcmp(stage, "title") == 0 ||
            std::strcmp(stage, "opening") == 0 ||
            std::strcmp(stage, "F_SP102") == 0);
}

static bool is_game_resetting_or_title() {
    if (mDoRst::getResetData() != nullptr) {
        if (mDoRst::isReset() || mDoRst::isReturnToMenu() ||
            mDoRst::is3ButtonReset() || mDoRst::isShutdown()) {
            return true;
        }
    }

    if (JUTGamePad::C3ButtonReset::sResetSwitchPushing ||
        JUTGamePad::C3ButtonReset::sResetOccurred) {
        return true;
    }

    if (is_title_or_menu_stage(dComIfGp_getStartStageName()) ||
        is_title_or_menu_stage(dComIfGp_getNextStageName())) {
        return true;
    }

    if (fpcM_SearchByName(fpcNm_LOGO_SCENE_e) != nullptr ||
        fpcM_SearchByName(fpcNm_OPENING_SCENE_e) != nullptr ||
        fpcM_SearchByName(fpcNm_NAME_SCENE_e) != nullptr ||
        fpcM_SearchByName(fpcNm_NAMEEX_SCENE_e) != nullptr ||
        fopAcM_SearchByName(fpcNm_TITLE_e) != nullptr) {
        return true;
    }

    return false;
}

static void close_boss_rush_session() {
    if (!s_bossRushModeActive && !s_exitingBossRush && !boss_rush_session_marker_present()) {
        return;
    }

    rush_debug_logf("[rush] close_boss_rush_session: closing session on reset / return to title");

    puppet_zelda_walls_end_track();

    s_bossRushModeActive = false;
    s_exitingBossRush = false;
    s_activeFightIndex = -1;
    s_pendingFightIndex = -1;
    s_rushRunActive = false;
    s_gauntletLadderActive = false;
    s_gauntletPhase = 0;
    s_pendingInitialInventory = false;
    s_returningToChamber = false;
    s_returnSawFadeOut = false;
    s_needsChamberSpawn = false;
    s_chamberCamArmFrames = 0;
    s_chamberSpawnFrames = 0;
    s_portalArrivalAnimPending = false;
    s_ignitedStatue = -1;
    s_recordReturnFrames = -1;
    s_recordReturnReason = nullptr;
    s_pendingFightFromArena = false;
    s_dungeonClearWarpPending = false;
    s_dungeonClearWarpFrames = 0;
    s_exitWarpViaDissolve = false;
    s_exitSaveReloadPending = false;
    s_exitCardLoadArmed = false;
    s_exitCardDataReady = false;
    s_exitWarpArmed = false;
    s_exitSaveReloadFrames = 0;
    s_morpheelPosPinFrames = 0;
    s_morpheelCamArmFrames = 0;
    s_horsebackGanonKoTimer = -1;
    s_horsebackGanonSawHorse = false;
    s_horsebackRetryLanding = false;
    s_beastGanonKoTimer = -1;
    s_arenaSettle = 0;
    s_arenaFreezeUntil = 0;
    s_swordDrawnLatched = false;
    s_sawSwordDrawnAtCommit = false;
    s_pendingGearSaveApply = false;
    s_pendingGearSaveKind = 0;
    s_pendingGearBoss = nullptr;
    s_retryWarpActive = false;
    s_chamberEquipsPending = false;
    s_chamberEquipsFrames = 0;
    s_equipSuppressWaitBlack = false;
    s_equipSuppressWaitBlackFrames = 0;

    boss_rush_timer_end_chain_run();
    boss_rush_timer_end_all_phases();
    boss_rush_timer_reset_run();

    reset_boss_rush_midna_flow();
    clear_boss_rush_session_marker();
    s_hasSavedLocation = false;
    if (svc_save != nullptr && s_modCtx != nullptr) {
        svc_save->delete_blob(s_modCtx, kBossRushLocationBlobName);
    }

    if (s_shouldAutoSaveCaptured && s_engineShouldAutoSave != nullptr) {
        *s_engineShouldAutoSave = s_shouldAutoSaveOriginal;
    }
    s_shouldAutoSaveCaptured = false;

    restore_boss_rush_fast_transitions();
    clear_ring_flames();
    unload_boss_rush_models();
    boss_rush_texts_reset_fade();
    boss_rush_master_sword_reset_fade();

    custom_equip_set_suppressed(false);
}

void exit_boss_rush() {
    if (!s_bossRushModeActive && !s_exitingBossRush) return;
    if (s_exitSaveReloadPending) return;


    s_bossRushModeActive = false;
    s_exitingBossRush = true;
    s_activeFightIndex = -1;
    s_pendingFightIndex = -1;
    s_rushRunActive = false;
    boss_rush_timer_end_chain_run();
    s_pendingInitialInventory = false;
    s_returningToChamber = false;
    s_returnSawFadeOut = false;
    s_needsChamberSpawn = false;
    s_chamberCamArmFrames = 0;
    reset_boss_rush_midna_flow();

    s_exitSaveReloadPending = true;
    s_exitCardLoadArmed = false;
    s_exitCardDataReady = false;
    s_exitWarpArmed = false;
    s_exitSaveReloadFrames = 0;

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
        link->allUnequip(0);
        link->mNormalSpeed = 0.0f;
        link->speed.set(0.0f, 0.0f, 0.0f);

        const char* warpStage =
            g_dComIfG_gameInfo.info.getPlayer().getPlayerReturnPlace().getName();
        g_meter2_info.setWarpInfo(warpStage, cXyz(0.0f, 0.0f, 0.0f), 0,
                                  g_dComIfG_gameInfo.info.getPlayer().getPlayerReturnPlace().getRoomNo(),
                                  0, 0);
        s_dungeonClearWarpPending = true;
        s_dungeonClearWarpFrames  = 0;
        s_exitWarpViaDissolve = true;
        if (link->procCoWarpInit(0, 1)) {
            link->field_0x347c = 4.6f;
            if (daPy_py_c::checkNowWolf()) {
                daMidna_c* midna = daPy_py_c::getMidnaActor();
                if (midna != nullptr) {
                    midna->changeDemoMode(9);
                }
            }
            s_portalCineWasHuman = true;
            human_warp_cinematic_departure();
        } else {
            s_dungeonClearWarpPending = false;
            s_exitWarpViaDissolve = false;
        }
    }
}

static void commit_boss_rush_fight_warp(size_t i, daAlink_c* link,
                                        const LogService* log_svc, ModContext* mod_ctx) {
    const BossGalleryEntry& boss = g_bossGalleryTable[i];

    s_pendingFightFromArena = !is_in_chamber_room() || s_activeFightIndex >= 0 || boss_rush_is_fighting_here();
    s_pendingWarpSawEnableNextStage = false;
    s_warpWatchdogFrames = 0;
    s_activeFightIndex = -1;
    rush_debug_logf("[rush] commit '%s' stage=%s room=%d arenaFrom=%d",
                    boss.displayName, boss.stage, (int)boss.room,
                    (int)s_pendingFightFromArena);

    s_needsChamberSpawn = false;
    s_chamberCamArmFrames = 0;
    s_chamberSpawnFrames = 0;
    s_swordDrawnLatched = false;

    if (s_pendingFightFromArena) {
        unload_boss_rush_models();
        clear_ring_flames();
        s_ignitedStatue = -1;
        if (link != nullptr) {
            link->cancelOriginalDemo();
        }
    }

    s_pendingGearSaveApply = true;
    s_pendingGearSaveKind = 1;
    s_pendingGearBoss = &boss;

    if (link != nullptr) {
        link->speed.set(0.0f, 0.0f, 0.0f);
        link->speedF = 0.0f;
        if (std::strcmp(boss.displayName, "Morpheel") != 0) {
            if (link->checkEquipHeavyBoots()) {
                link->setHeavyBoots(0);
            }
            if (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e ||
                dComIfGs_getSelectEquipClothes() == dItemNo_ARMOR_e) {
                dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
                dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
            }
        }
    }
    s_sawSwordDrawnAtCommit = (link != nullptr && link->checkSwordDraw());

    if (!s_retryWarpActive) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_WARP_FADE, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }

    if (!s_pendingFightFromArena || s_retryWarpActive) {
        reset_boss_rush_save_flags();
    }
    g_dComIfG_gameInfo.info.getMemory().getBit().onStageBossDemo();

    const bool isGanonGauntlet = !g_configBossRushSeparateGanon &&
                                 !s_gauntletLadderActive &&
                                 (std::strcmp(boss.displayName, "Ganondorf") == 0);

    if (isGanonGauntlet) {
        g_dComIfG_gameInfo.info.getDan().offSwitch(1);
        dComIfGs_offEventBit(dSv_event_flag_c::M_067);
        dComIfGs_offEventBit(0x0540);
        dComIfGs_onEventBit(dSv_event_flag_c::F_0800);
        if (link != nullptr && link->checkMidnaRide()) {
            link->offMidnaRide();
        }
        s_pendingFightIndex = static_cast<int>(i);
        s_ignitedStatue = static_cast<int>(i);
        u32 lastMode = g_dComIfG_gameInfo.info.getRestart().mLastMode & ~0xFF000000;
        lastMode |= 0x28000000;
        dComIfGp_setNextStage("D_MN09A", 0, 50, 0, 0.0f, lastMode, 1, 0, 0, 0, 0);
        return;
    }


    if (std::strcmp(boss.stage, "D_MN09B") == 0) {
        if (std::strcmp(boss.displayName, "Ganondorf") == 0) {
            g_dComIfG_gameInfo.info.getDan().onSwitch(1);
        } else {
            g_dComIfG_gameInfo.info.getDan().offSwitch(1);
        }
    }

    if (is_boss_rush_ganon_stage(boss.stage)) {
        dComIfGs_offEventBit(dSv_event_flag_c::M_067);
        dComIfGs_offEventBit(0x0540);
        dComIfGs_onEventBit(dSv_event_flag_c::F_0800);
        if (link != nullptr && link->checkMidnaRide()) {
            link->offMidnaRide();
        }
    } else {
        // The chamber holds the Midna availability bits on (HUD flicker fix);
        // fights expect the fresh-save state, so drop them again on the way
        // out. With 0x0540/M_067 still set the engine assumes Midna is
        // already riding and never mounts her on the wolf.
        dComIfGs_offEventBit(dSv_event_flag_c::M_067);
        dComIfGs_offEventBit(0x0540);
        dComIfGs_onEventBit(dSv_event_flag_c::F_0800);
    }

    s_pendingFightIndex = static_cast<int>(i);
    s_ignitedStatue = static_cast<int>(i);
    if (std::strcmp(boss.displayName, "Darkhammer") == 0 ||
        std::strcmp(boss.displayName, "Fyrus") == 0 ||
        std::strcmp(boss.displayName, "Deku Toad") == 0 ||
        std::strcmp(boss.displayName, "Death Sword") == 0 ||
        std::strcmp(boss.displayName, "Blizzeta") == 0 ||
        std::strcmp(boss.displayName, "Stallord") == 0 ||
        std::strcmp(boss.displayName, "Dangoro") == 0) {
        cDmr_SkipInfo = 1;
    } else if (std::strcmp(boss.displayName, "Morpheel") == 0 ||
               std::strcmp(boss.displayName, "Diababa") == 0) {
        cDmr_SkipInfo = 60;
    }

    if (std::strcmp(boss.displayName, "Dangoro") == 0) {
        dComIfGs_onZoneSwitch(5, 51);
        dComIfGs_onZoneSwitch(5, -1);
    }

    clear_boss_dungeon_clear_flags(boss.stage);

    s_horsebackGanonKoTimer = -1;
    s_horsebackGanonSawHorse = false;
    s_beastGanonKoTimer = -1;
    boss_bar_consume_defeat_event();

    u32 lastMode = g_dComIfG_gameInfo.info.getRestart().mLastMode & ~0xFF000000;
    if (std::strcmp(boss.displayName, "Ook") == 0 ||
        std::strcmp(boss.displayName, "Dangoro") == 0 ||
        std::strcmp(boss.displayName, "Deku Toad") == 0 ||
        std::strcmp(boss.displayName, "Darkhammer") == 0 ||
        std::strcmp(boss.displayName, "Darknut") == 0 ||
        std::strcmp(boss.displayName, "Puppet Zelda") == 0 ||
        std::strcmp(boss.displayName, "Ganondorf") == 0) {
        lastMode |= 0x28000000;
    }
    g_dComIfG_gameInfo.info.getRestart().mLastMode = lastMode;

    if (boss.fightSpawnPos != nullptr) {
        dComIfGs_setRestartRoom(*boss.fightSpawnPos, boss.fightSpawnAngle, boss.room);
        dComIfGs_setRestartRoomParam((boss.room & 0x3F) | (0xFF << 24));
        dComIfGp_setNextStage(boss.stage, -1, boss.room, boss.layer, 0.0f, lastMode, 1, 0, boss.fightSpawnAngle, 0, 0);
    } else {
        dComIfGp_setNextStage(boss.stage, boss.point, boss.room, boss.layer, 0.0f, lastMode, 1, 0, boss.fightSpawnAngle, 0, 0);
    }
}

void boss_rush_retry_current_fight(const LogService* log_svc, ModContext* mod_ctx) {
    int idx = boss_rush_current_target_index();
    if (idx < 0) {
        return;
    }

    s_recordReturnReason = nullptr;
    s_recordReturnFrames = -1;
    s_returningToChamber = false;
    s_returnSawFadeOut = false;
    s_needsChamberSpawn = false;
    boss_rush_texts_reset_fade();
    boss_rush_master_sword_reset_fade();
    reset_boss_rush_midna_flow();
    Z2GetAudioMgr()->subBgmStop();

    const bool inGanonGauntlet = !g_configBossRushSeparateGanon &&
        (s_gauntletLadderActive ||
         (idx >= 0 && static_cast<size_t>(idx) < g_bossGalleryCount &&
          (std::strcmp(g_bossGalleryTable[idx].displayName, "Ganondorf") == 0 ||
           std::strcmp(g_bossGalleryTable[idx].displayName, "Puppet Zelda") == 0 ||
           std::strcmp(g_bossGalleryTable[idx].displayName, "Beast Ganon") == 0 ||
           std::strcmp(g_bossGalleryTable[idx].displayName, "Horseback Ganon") == 0)));

    if (inGanonGauntlet) {
        for (size_t i = 0; i < g_bossGalleryCount; ++i) {
            if (std::strcmp(g_bossGalleryTable[i].displayName, "Ganondorf") == 0) {
                idx = static_cast<int>(i);
                break;
            }
        }
        s_gauntletLadderActive = false;
        s_gauntletPhase = 0;
        boss_rush_timer_end_all_phases();
    }

    reset_boss_rush_save_flags();
    clear_boss_dungeon_clear_flags(g_bossGalleryTable[idx].stage);
    g_dComIfG_gameInfo.info.getMemory().getBit().onStageBossDemo();

    s_retryWarpActive = true;
    s_horsebackRetryLanding =
        (!inGanonGauntlet && std::strcmp(g_bossGalleryTable[idx].displayName, "Horseback Ganon") == 0);
    s_horsebackGanonKoTimer = -1;
    s_horsebackGanonSawHorse = false;
    s_beastGanonKoTimer = -1;
    mDoGph_gInf_c::offFade();
    s_morpheelPosPinFrames = 0;
    s_morpheelCamArmFrames = 0;
    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        link->cancelOriginalDemo();
        if (link->checkMidnaRide()) {
            link->offMidnaRide();
        }
        link->mNormalSpeed = 0.0f;
        link->speed.set(0.0f, 0.0f, 0.0f);
        if (link->checkEquipHeavyBoots()) {
            if (std::strcmp(g_bossGalleryTable[idx].displayName, "Morpheel") != 0) {
                link->setHeavyBoots(0);
            }
        }
    }
    dComIfGp_event_reset();

    if (std::strcmp(g_bossGalleryTable[idx].displayName, "Dangoro") == 0) {
        dComIfGs_onZoneSwitch(5, 51);
        dComIfGs_onZoneSwitch(5, -1);
    }

    if (std::strcmp(g_bossGalleryTable[idx].displayName, "Stallord") == 0) {
        for (int z = 0; z < dSv_info_c::ZONE_MAX; ++z) {
            dSv_zone_c& zone = g_dComIfG_gameInfo.info.getZone(z);
            if (zone.getRoomNo() == 50 || zone.getRoomNo() == -1) {
                zone.getBit().offSwitch(6);
                zone.getBit().offSwitch(7);
                zone.getBit().offSwitch(8);
            }
        }
    }

    if (s_fightStartLife > 0) {
        dComIfGs_setLife(s_fightStartLife);
        sync_life_meter_instant(s_fightStartLife, dComIfGs_getMaxLife());
    }

    boss_rush_timer_reset_run();
    boss_bar_force_reset();
    boss_bar_consume_defeat_event();

    commit_boss_rush_fight_warp(static_cast<size_t>(idx), daAlink_getAlinkActorClass(),
                                log_svc, mod_ctx);
}

static void start_boss_rush_full_run(const LogService* log_svc, ModContext* mod_ctx) {
    if (s_rushRunActive || s_pendingFightIndex != -1 || s_returningToChamber) {
        return;
    }

    s_rushRunActive = true;
    s_rushRunPos = 0;
    s_rushRunCount = 0;
    const size_t activeCount = boss_rush_get_active_gallery_count();
    for (size_t s = 0; s < activeCount && s_rushRunCount < kMaxBossGalleryEntries; ++s) {
        s_rushRunOrder[s_rushRunCount++] = boss_rush_get_active_gallery_table_index(s);
    }
    if (s_rushRunCount == 0) {
        s_rushRunActive = false;
        return;
    }

    apply_boss_rush_loadout(false);
    reset_boss_rush_save_flags();
    dComIfGs_setLife(full_life_for_max(dComIfGs_getMaxLife()));
    sync_life_meter_instant(full_life_for_max(dComIfGs_getMaxLife()), dComIfGs_getMaxLife());
    boss_rush_timer_begin_chain_run();

    commit_boss_rush_fight_warp(s_rushRunOrder[0], daAlink_getAlinkActorClass(),
                               log_svc, mod_ctx);
}

static void advance_boss_rush_run(const LogService* log_svc, ModContext* mod_ctx,
                                  const char* reason) {
    rush_debug_logf("[hb-dbg] advance_boss_rush_run reason=%s rushRunActive=%d pendingFight=%d returning=%d",
                    reason ? reason : "?", (int)s_rushRunActive, s_pendingFightIndex,
                    (int)s_returningToChamber);
    if (s_pendingFightIndex != -1 || s_returningToChamber) {
        return;
    }
    if (s_rushRunActive && s_rushRunPos + 1 < s_rushRunCount) {
        ++s_rushRunPos;
        boss_rush_timer_notify_defeat();
        commit_boss_rush_fight_warp(s_rushRunOrder[s_rushRunPos], daAlink_getAlinkActorClass(),
                                   log_svc, mod_ctx);
        return;
    }

    s_rushRunActive = false;
    boss_rush_timer_commit_chain_total();
    boss_rush_timer_end_chain_run();
    return_to_boss_rush_chamber(log_svc, mod_ctx, reason);
}

static fopAc_ac_c* boss_rush_find_current_boss_actor() {
    const int idx = boss_rush_target_index();
    if (idx < 0 || static_cast<size_t>(idx) >= g_bossGalleryCount) {
        return nullptr;
    }

    const char* name = g_bossGalleryTable[idx].displayName;
    int profile = -1;
    if (std::strcmp(name, "Ook") == 0) profile = fpcNm_E_MK_e;
    else if (std::strcmp(name, "Diababa") == 0) profile = fpcNm_B_BQ_e;
    else if (std::strcmp(name, "Dangoro") == 0) profile = fpcNm_E_GOB_e;
    else if (std::strcmp(name, "Fyrus") == 0) profile = fpcNm_E_FM_e;
    else if (std::strcmp(name, "Deku Toad") == 0) profile = fpcNm_E_DT_e;
    else if (std::strcmp(name, "Morpheel") == 0) profile = fpcNm_B_OB_e;
    else if (std::strcmp(name, "Death Sword") == 0) profile = fpcNm_E_VT_e;
    else if (std::strcmp(name, "Stallord") == 0) profile = fpcNm_B_DS_e;
    else if (std::strcmp(name, "Darkhammer") == 0) profile = fpcNm_E_TH_e;
    else if (std::strcmp(name, "Blizzeta") == 0) profile = fpcNm_B_YO_e;
    else if (std::strcmp(name, "Darknut") == 0) profile = fpcNm_B_TN_e;
    else if (std::strcmp(name, "Armogohma") == 0) profile = fpcNm_B_GM_e;
    else if (std::strcmp(name, "Aeralfos") == 0) profile = fpcNm_B_GG_e;
    else if (std::strcmp(name, "Argorok") == 0) profile = fpcNm_B_DR_e;
    else if (std::strcmp(name, "Zant") == 0) profile = fpcNm_B_ZANT_e;
    else if (std::strcmp(name, "Puppet Zelda") == 0) profile = fpcNm_E_PH_e;
    else if (std::strcmp(name, "Beast Ganon") == 0) profile = fpcNm_B_MGN_e;
    else if (std::strcmp(name, "Horseback Ganon") == 0 ||
             std::strcmp(name, "Ganondorf") == 0) profile = fpcNm_B_GND_e;
    if (profile < 0) {
        return nullptr;
    }

    return fopAcM_SearchByName(static_cast<s16>(profile));
}

void boss_rush_debug_kill_current_boss() {
    if (!boss_rush_is_fighting_here()) {
        return;
    }

    fopAc_ac_c* boss = boss_rush_find_current_boss_actor();
    if (boss != nullptr) {
        boss->health = 0;
    }

    s_killWatchdogFrames = 0;
    advance_boss_rush_run(s_logSvc, s_modCtx, "Debug kill boss");
}

static void apply_pending_gear_save_if_covered() {
    if (!s_pendingGearSaveApply) {
        return;
    }

    JUTFader* fader = mDoGph_gInf_c::getFader();
    const s32 status = (fader != nullptr) ? fader->getStatus() : -1;
    if (status != JUTFader::None) {
        return;
    }

    if (s_pendingGearSaveKind == 1 && s_pendingGearBoss != nullptr) {
        if (g_configBossRushVanillaGear) {
            apply_boss_rush_loadout(false);
            reset_boss_rush_save_flags();
            clear_boss_dungeon_clear_flags(s_pendingGearBoss->stage);
            apply_boss_rush_equipment_restriction(*s_pendingGearBoss);
        }
        if (std::strcmp(s_pendingGearBoss->displayName, "Morpheel") == 0) {
            dMeter2Info_setCloth(dItemNo_WEAR_ZORA_e, false);
            dComIfGs_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
            dComIfGp_setSelectEquipClothes(dItemNo_WEAR_ZORA_e);
            assign_select_item(SELECT_ITEM_X, SLOT_3);
            assign_select_item(SELECT_ITEM_Y, SLOT_10);
        }
        if (std::strcmp(s_pendingGearBoss->displayName, "Death Sword") == 0) {
            dComIfGs_setTransformStatus(TF_STATUS_WOLF);
        } else {
            dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
        }
    } else if (s_pendingGearSaveKind == 2) {
        clear_all_select_items();
        dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
        dMeter2Info_setCloth(dItemNo_WEAR_KOKIRI_e, false);
        dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
        dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
        dComIfGs_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
        dComIfGp_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
        dComIfGs_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
        dComIfGp_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
    }

    s_pendingGearSaveApply = false;
    s_pendingGearSaveKind = 0;
    s_pendingGearBoss = nullptr;
}

int boss_rush_gauntlet_phase() {
    return s_gauntletPhase;
}

void update_boss_rush(const LogService* log_svc, ModContext* mod_ctx) {
    if (is_game_resetting_or_title()) {
        if (s_bossRushModeActive || s_exitingBossRush || boss_rush_session_marker_present()) {
            close_boss_rush_session();
        }
        return;
    }

    apply_pending_gear_save_if_covered();

    if (!g_configBossRushSeparateGanon && boss_rush_is_fighting_here() && !s_returningToChamber) {
        const int gt = boss_rush_target_index();
        const bool inGauntletChain =
            gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            (std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Puppet Zelda") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Beast Ganon") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Horseback Ganon") == 0);
        if (inGauntletChain) {
            const char* tn = g_bossGalleryTable[gt].displayName;
            const char* stage = dComIfGp_getStartStageName();
            const bool dan1 = stage != nullptr && std::strcmp(stage, "D_MN09B") == 0 &&
                              (g_dComIfG_gameInfo.info.getDan().isSwitch(1) ||
                               dComIfGs_isSaveDunSwitch(1) ||
                               s_gauntletPhase == 4 ||
                               std::strcmp(tn, "Ganondorf") == 0);
            int phase;
            if (stage != nullptr && std::strcmp(stage, "D_MN09B") == 0) {
                phase = dan1 ? 4 : 1;
            } else if (stage != nullptr && std::strcmp(stage, "D_MN09A") == 0) {
                const bool isBeast = (dComIfG_play_c::getLayerNo(0) == 1 ||
                                      fopAcM_SearchByName(fpcNm_B_MGN_e) != nullptr);
                phase = isBeast ? 3 : 2;
            } else if (std::strcmp(tn, "Puppet Zelda") == 0) {
                phase = 2;
            } else if (std::strcmp(tn, "Beast Ganon") == 0) {
                phase = 3;
            } else if (std::strcmp(tn, "Horseback Ganon") == 0) {
                phase = 1;
            } else {
                phase = dan1 ? 4 : 1;
            }

            if (phase != s_gauntletPhase) {
                if (phase == 2 || phase == 3 || phase == 1 || phase == 4) {
                    const char* want = (phase == 2) ? "Puppet Zelda"
                                     : (phase == 3) ? "Beast Ganon"
                                     : (phase == 1) ? "Horseback Ganon"
                                     : "Ganondorf";
                    for (size_t i = 0; i < g_bossGalleryCount; ++i) {
                        if (std::strcmp(g_bossGalleryTable[i].displayName, want) == 0) {
                            apply_boss_suggested_items(g_bossGalleryTable[i]);
                            break;
                        }
                    }
                }
                boss_rush_debug_log("[gauntlet] phase %d -> %d", s_gauntletPhase, phase);
                s_gauntletPhase = phase;
            }
        } else {
            s_gauntletPhase = 0;
        }
    } else {
        s_gauntletPhase = 0;
    }

    static bool s_ladderBeastSeen = false;
    if (!g_configBossRushSeparateGanon && s_gauntletLadderActive && boss_rush_is_fighting_here() &&
        !s_returningToChamber && s_pendingFightIndex == -1) {
        const int gt = boss_rush_target_index();
        if (gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            (std::strcmp(g_bossGalleryTable[gt].displayName, "Beast Ganon") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0)) {
            fopAc_ac_c* mgn = fopAcM_SearchByName(fpcNm_B_MGN_e);
            if (mgn != nullptr) {
                s_ladderBeastSeen = true;
            } else if (s_ladderBeastSeen) {
                s_ladderBeastSeen = false;
                boss_rush_debug_log("[gauntlet] beast gone -> horseback");
                const int hbIdx = gauntlet_next_phase_index("Beast Ganon");
                if (hbIdx >= 0) {
                    commit_boss_rush_fight_warp(static_cast<size_t>(hbIdx),
                                                daAlink_getAlinkActorClass(), log_svc,
                                                mod_ctx);
                    return;
                }
            }
        } else {
            s_ladderBeastSeen = false;
        }
    }

    if (s_bossRushModeActive) {
        force_boss_rush_fast_transitions();
    }

    if (s_spuriousReloadBlockLogTimer > 0) {
        --s_spuriousReloadBlockLogTimer;
    }

    if (s_entryGate != BossRushEntryGate::None) {
        update_boss_rush_entry_gate();
        return;
    }

    update_boss_rush_autosave_suppression();

    if (s_pendingFightIndex != -1) {
        const size_t idx = static_cast<size_t>(s_pendingFightIndex);
        if (idx < g_bossGalleryCount) {
            const char* name = g_bossGalleryTable[idx].displayName;
            if (std::strcmp(name, "Darkhammer") == 0 ||
                std::strcmp(name, "Fyrus") == 0 ||
                std::strcmp(name, "Deku Toad") == 0 ||
                std::strcmp(name, "Death Sword") == 0 ||
                std::strcmp(name, "Blizzeta") == 0 ||
                std::strcmp(name, "Stallord") == 0 ||
                std::strcmp(name, "Dangoro") == 0) {
                cDmr_SkipInfo = 1;
            } else if (std::strcmp(name, "Morpheel") == 0 ||
                       std::strcmp(name, "Diababa") == 0) {
                cDmr_SkipInfo = 60;
            }
        }
    }

    if (s_dungeonClearWarpPending) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link && link->mProcID == daAlink_c::PROC_WARP) {
            if (link->field_0x347c > -0.5f) {
                cLib_addCalc(&link->field_0x347c, -0.5f, 0.1f, 0.05f, 0.001f);
            }
        }

        if (dComIfGp_isEnableNextStage() || fopOvlpM_IsPeek()) {
            s_dungeonClearWarpPending = false;
            s_dungeonClearWarpFrames  = 0;
        } else if (++s_dungeonClearWarpFrames >= 180) {
            s_dungeonClearWarpPending = false;
            s_dungeonClearWarpFrames  = 0;
            start_boss_rush();
        }
    }

    if (s_recordReturnFrames >= 0) {
        if (--s_recordReturnFrames < 0) {
            return_to_boss_rush_chamber(log_svc, mod_ctx,
                s_recordReturnReason ? s_recordReturnReason : "Boss defeated");
        }
        return;
    }

    if (process_pending_boss_rush_midna_action(log_svc, mod_ctx)) {
        return;
    }

    if (is_boss_rush_active()) {
        refresh_boss_rush_midna_flow();
        update_boss_rush_midna(log_svc, mod_ctx);
        puppet_zelda_walls_keep();
    }

    update_boss_rush_exit_save_reload();

    if (s_pendingInitialInventory) {
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        const bool enteredChamber = is_in_chamber_room();

        static int s_pendingInitialInventoryFrames = 0;
        if (!enteredChamber) {
            s_pendingInitialInventoryFrames = 0;
        }

        if (faderStatus == JUTFader::None || (enteredChamber && ++s_pendingInitialInventoryFrames > 60)) {
            apply_boss_rush_loadout(false);
            s_pendingInitialInventoryFrames = 0;
            s_pendingInitialInventory = false;
        }
    }

    if (s_exitingBossRush) {
        if (!is_in_chamber_room() && !dComIfGp_isEnableNextStage()) {
            s_exitingBossRush = false;
            s_hasSavedLocation = false;
            if (svc_save != nullptr && s_modCtx != nullptr) {
                svc_save->delete_blob(s_modCtx, kBossRushLocationBlobName);
                clear_boss_rush_session_marker();
            }
            restore_boss_rush_fast_transitions();
        }
    }

    if (s_pendingFightIndex != -1) {
        ++s_warpWatchdogFrames;
        if (s_warpWatchdogFrames % 60 == 0) {
            const char* st = dComIfGp_getStartStageName();
            JUTFader* fader = mDoGph_gInf_c::getFader();
            const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
            rush_debug_logf("[rush] warp pending f=%d ens=%d peek=%d fader=%d stage=%s room=%d",
                            s_warpWatchdogFrames, (int)dComIfGp_isEnableNextStage(),
                            (int)fopOvlpM_IsPeek(), (int)faderStatus,
                            st ? st : "?", (int)dComIfGp_roomControl_getStayNo());
        }

        if (dComIfGp_isEnableNextStage()) {
            s_pendingWarpSawEnableNextStage = true;
        }

        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        if (faderStatus == JUTFader::None || fopOvlpM_IsPeek()) {
            unload_boss_rush_models();
        }

        const bool isGanonGauntlet = !g_configBossRushSeparateGanon && !s_gauntletLadderActive &&
                                     (std::strcmp(g_bossGalleryTable[s_pendingFightIndex].displayName, "Ganondorf") == 0);
        const char* expStage = isGanonGauntlet ? "D_MN09A" : g_bossGalleryTable[s_pendingFightIndex].stage;
        const char* curStage = dComIfGp_getStartStageName();
        const bool atTargetStage = (curStage != nullptr && std::strcmp(curStage, expStage) == 0);

        const bool pendingTargetIsChamber =
            std::strcmp(g_bossGalleryTable[s_pendingFightIndex].stage, kBossRushChamberStage) == 0 &&
            g_bossGalleryTable[s_pendingFightIndex].room == kBossRushChamberRoom;

        const s8 curRoom = static_cast<s8>(dComIfGp_roomControl_getStayNo());
        const bool roomMatchesLanding = pendingTargetIsChamber
            ? is_in_chamber_room()
            : (!is_in_chamber_room() && curRoom >= 0 &&
               (isGanonGauntlet
                    ? (curRoom == 50 || curRoom == 51)
                    : boss_rush_room_matches_target(g_bossGalleryTable[s_pendingFightIndex],
                                                    curRoom)));

        const bool transitionFinished = !fopOvlpM_IsPeek() &&
                                        (faderStatus == JUTFader::Wait || faderStatus == JUTFader::FadeIn);

        const bool landingInFight = s_pendingWarpSawEnableNextStage &&
                                    !dComIfGp_isEnableNextStage() &&
                                    roomMatchesLanding &&
                                    atTargetStage &&
                                    transitionFinished;

        if (landingInFight) {
            rush_debug_logf("[rush] landed '%s' stage=%s room=%d arena=%d",
                            g_bossGalleryTable[s_pendingFightIndex].displayName,
                            g_bossGalleryTable[s_pendingFightIndex].stage,
                            (int)curRoom,
                            (int)s_pendingFightFromArena);
            s_warpWatchdogFrames = 0;
            s_pendingFightFromArena = false;
            s_pendingWarpSawEnableNextStage = false;
            s_activeFightIndex = s_pendingFightIndex;
            s_pendingFightIndex = -1;
            ++s_fightWarpGen;

            if (pendingTargetIsChamber) {
                rush_debug_logf("[rush] landed chamber boss '%s'",
                                g_bossGalleryTable[s_activeFightIndex].displayName);
            }

            s_arenaSettle = arena_settle_frames_for(g_bossGalleryTable[s_activeFightIndex]);
            s_arenaFreezeUntil = s_arenaSettle;
            s_bossRushDeathHandledTriggered = false;
            s_needsChamberSpawn = false;
            s_chamberCamArmFrames = 0;
            s_chamberSpawnFrames = 0;
            unload_boss_rush_models();

            if (isGanonGauntlet) {
                if (!s_gauntletLadderActive) {
                    boss_rush_timer_begin_all_phases();
                }
                s_gauntletLadderActive = true;
            }

            if (s_activeFightIndex >= 0 && s_activeFightIndex < static_cast<int>(g_bossGalleryCount)) {
                const BossGalleryEntry& boss = g_bossGalleryTable[s_activeFightIndex];

                if (std::strcmp(boss.displayName, "Puppet Zelda") == 0 ||
                    std::strcmp(boss.displayName, "Beast Ganon") == 0 || isGanonGauntlet) {
                    puppet_zelda_walls_begin_track();
                }

                clear_boss_dungeon_clear_flags(boss.stage);
                g_dComIfG_gameInfo.info.getMemory().getBit().onStageBossDemo();

                const char* curStage = dComIfGp_getStartStageName();
                if (curStage != nullptr && std::strcmp(curStage, "D_MN09B") == 0) {
                    if (std::strcmp(boss.displayName, "Ganondorf") == 0) {
                        g_dComIfG_gameInfo.info.getDan().onSwitch(1);
                    } else {
                        g_dComIfG_gameInfo.info.getDan().offSwitch(1);
                    }
                } else if (curStage != nullptr && std::strcmp(curStage, "D_MN09A") == 0) {
                    g_dComIfG_gameInfo.info.getDan().offSwitch(1);
                }
                if (std::strcmp(boss.displayName, "Dangoro") == 0) {
                    dComIfGs_onZoneSwitch(5, 51);
                    dComIfGs_onZoneSwitch(5, -1);
                }
                if (is_boss_rush_ganon_stage(boss.stage)) {
                    dComIfGs_offEventBit(dSv_event_flag_c::M_067);
                    dComIfGs_offEventBit(0x0540);
                    dComIfGs_onEventBit(dSv_event_flag_c::F_0800);
                }

                if (g_configBossRushVanillaGear) {
                    reset_boss_rush_save_flags();
                    clear_boss_dungeon_clear_flags(boss.stage);
                    g_dComIfG_gameInfo.info.getMemory().getBit().onStageBossDemo();
                    apply_boss_rush_loadout(false);
                    apply_boss_rush_equipment_restriction(boss);
                }

                if (!g_configBossRushSeparateGanon && std::strcmp(boss.displayName, "Ganondorf") == 0) {
                    if (g_configBossRushSuggestedItems) {
                        for (size_t pz = 0; pz < g_bossGalleryCount; ++pz) {
                            if (std::strcmp(g_bossGalleryTable[pz].displayName, "Puppet Zelda") == 0) {
                                apply_boss_suggested_items(g_bossGalleryTable[pz]);
                                break;
                            }
                        }
                    }
                } else if (g_configBossRushSuggestedItems) {
                    apply_boss_suggested_items(boss);
                }

                if (std::strcmp(boss.displayName, "Death Sword") == 0) {
                    dComIfGs_setTransformStatus(TF_STATUS_WOLF);
                } else {
                    dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
                }

                if (s_rushRunActive) {
                    dComIfGs_setLife(full_life_for_max(dComIfGs_getMaxLife()));
                    sync_life_meter_instant(full_life_for_max(dComIfGs_getMaxLife()),
                                            dComIfGs_getMaxLife());
                }

                const char* curSt2 = dComIfGp_getStartStageName();
                if (curSt2 != nullptr && std::strcmp(curSt2, "D_MN09B") == 0 &&
                    g_dComIfG_gameInfo.info.getDan().isSwitch(1)) {
                    boss_rush_screen_hold_black();
                }
            }

            s_fightStartLife = dComIfGs_getLife();

            s_retryWarpActive = false;
        }
    }

    if (s_returningToChamber) {
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;

        if (faderStatus == JUTFader::FadeOut || faderStatus == JUTFader::None || !is_in_chamber_room()) {
            s_returnSawFadeOut = true;
        }

        if (s_returnSawFadeOut && is_in_chamber_room() && !fopOvlpM_IsPeek() &&
            !dComIfGp_isEnableNextStage()) {
            s_returningToChamber = false;
            s_returnSawFadeOut = false;
            s_activeFightIndex = -1;
            s_pendingFightIndex = -1;
            s_pendingFightFromArena = false;
            s_pendingWarpSawEnableNextStage = false;
            reset_boss_rush_save_flags();
            dComIfGs_setTransformStatus(TF_STATUS_HUMAN);
            {
                char spot[8];
                std::strncpy(spot, kBossRushChamberStage, sizeof(spot) - 1);
                spot[sizeof(spot) - 1] = '\0';
                mDoAud_setSceneName(spot, kBossRushChamberRoom, kBossRushChamberLayer);
                Z2GetAudioMgr()->bgmStart(Z2BGM_DUNGEON_LV6, 0, 0);
                Z2GetAudioMgr()->unMuteSceneBgm(0);
            }
            if (g_configBossRushSuggestedItems) {
                clear_all_select_items();
                dMeter2Info_setCloth(dItemNo_WEAR_KOKIRI_e, false);
                dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
                dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
            }
        }
    }

    static int s_lastZantPhase = -1;
    const char* curStage = dComIfGp_getStartStageName();
    if (curStage != nullptr && std::strcmp(curStage, "D_MN08D") == 0 && is_boss_rush_active() &&
        !s_returningToChamber && !is_boss_rush_transition_in_flight()) {
        int curPhase = -1;
        daB_ZANT_c* zant = reinterpret_cast<daB_ZANT_c*>(fopAcM_SearchByName(fpcNm_B_ZANT_e));
        if (zant != nullptr) {
            curPhase = zant->mFightPhase;
        } else {
            const s8 r = static_cast<s8>(dComIfGp_roomControl_getStayNo());
            if (r == 50 || r == 53) curPhase = 1;
            else if (r == 54) curPhase = 2;
            else if (r == 55) curPhase = 3;
            else if (r == 56) curPhase = 4;
            else if (r == 57) curPhase = 5;
            else if (r == 60) curPhase = 6;
        }

        if (curPhase != s_lastZantPhase && curPhase >= 0) {
            s_lastZantPhase = curPhase;
            static const s8 kZantPhaseToRoom[] = {50, 53, 54, 55, 56, 57, 60};
            s8 curRoom = (curPhase >= 0 && curPhase <= 6) ? kZantPhaseToRoom[curPhase] : 53;
            if (g_configBossRushSuggestedItems) {
                apply_zant_room_suggested_items(curRoom);
            }
        }
    } else {
        s_lastZantPhase = -1;
    }

    update_darkhammer_instant_fight();
    update_dangoro_instant_fight();
    update_fyrus_instant_fight();
    update_fyrus_intro_watchdog();
    update_dekutoad_instant_fight();
    update_morpheel_instant_fight();
    update_deathsword_instant_fight();
    update_blizzeta_instant_fight();
    update_stallord_instant_fight();
    update_stallord_phase_transition_skip();
    update_horsebackganon_instant_fight();
    update_darknut_instant_fight();
    update_armogohma_instant_fight();
    update_zant_instant_fight();
    update_beastganon_instant_fight(false);

    if (s_activeFightIndex >= 0 && static_cast<size_t>(s_activeFightIndex) < g_bossGalleryCount &&
        !is_in_chamber_room()) {
        static int s_activeFightLogTimer = 0;
        JUTFader* fader = mDoGph_gInf_c::getFader();
        const s32 faderStatus = (fader != nullptr) ? fader->getStatus() : -1;
        if (++s_activeFightLogTimer % 60 == 0) {
            fopAc_ac_c* boss = boss_rush_find_current_boss_actor();
            rush_debug_logf("[rush] fight '%s' st=%s rm=%d fader=%d peek=%d ev=%d boss=%p",
                            g_bossGalleryTable[s_activeFightIndex].displayName,
                            dComIfGp_getStartStageName() ? dComIfGp_getStartStageName() : "?",
                            (int)dComIfGp_roomControl_getStayNo(),
                            (int)faderStatus, (int)fopOvlpM_IsPeek(),
                            (int)dComIfGp_event_runCheck(), boss);
        }

        static int s_blackScreenWatchdog = 0;
        if (faderStatus == JUTFader::None && !fopOvlpM_IsPeek() && !dComIfGp_event_runCheck()) {
            if (++s_blackScreenWatchdog >= 180) {
                if (fader != nullptr) {
                    fader->setStatus(JUTFader::Wait, 0);
                }
                mDoGph_gInf_c::offFade();
                s_blackScreenWatchdog = 0;
            }
        } else {
            s_blackScreenWatchdog = 0;
        }
    }

    static bool s_wasInChamber = false;
    const bool nowInChamber = is_in_boss_rush_chamber();
    if (s_rushRunActive && nowInChamber && s_pendingFightIndex == -1 && !s_returningToChamber) {
        s_rushRunActive = false;
        boss_rush_timer_end_chain_run();
    }
    if (!s_wasInChamber && nowInChamber) {
        s_needsChamberSpawn = true;
        s_chamberSpawnFrames = 0;
        s_chamberCamArmFrames = 0;
        s_chamberCleanupFrames = kChamberCleanupWindow;
        s_ignitedStatue = -1;
        boss_rush_texts_reset_fade();
        boss_rush_master_sword_reset_fade();
    }
    if (s_wasInChamber && !nowInChamber) {
        s_chamberCamArmFrames = 0;
        unload_boss_rush_models();
    }
    s_wasInChamber = nowInChamber;

    {
        bool capeVisible = false;
        cXyz gndPos;
        csXyz gndAngle;
        if (kBossGalleryModelsEnabled && nowInChamber) {
            for (size_t bi = 0; bi < g_bossGalleryCount; ++bi) {
                if (std::strcmp(g_bossGalleryTable[bi].displayName, "Ganondorf") != 0) continue;
                const size_t slot = boss_rush_get_circle_slot_for_table_index(bi);
                boss_rush_get_slot_transform(slot, boss_rush_get_active_gallery_count(),
                                             kBossChamberFloorY, gndPos, gndAngle);
                gndPos.y += g_bossGalleryTable[bi].yOffset;
                capeVisible = true;
                update_ganondorf_cape(true, gndPos, gndAngle.y, g_bossGalleryTable[bi].scale);
                break;
            }
        }
        if (!capeVisible) {
            update_ganondorf_cape(false, cXyz(0.0f, 0.0f, 0.0f), 0, 1.0f);
        }
    }

    if (s_chamberEquipsPending) {
        JUTFader* eqFader = mDoGph_gInf_c::getFader();
        const s32 eqFaderStatus = (eqFader != nullptr) ? eqFader->getStatus() : -1;
        ++s_chamberEquipsFrames;
        if (eqFaderStatus == JUTFader::None || s_chamberEquipsFrames >= 600) {
            boss_rush_save_apply_equips_to_savedata();
            s_chamberEquipsPending = false;
            s_chamberEquipsFrames = 0;
        }
    }

    if (s_equipSuppressWaitBlack) {
        JUTFader* supFader = mDoGph_gInf_c::getFader();
        const s32 supFaderStatus = (supFader != nullptr) ? supFader->getStatus() : -1;
        ++s_equipSuppressWaitBlackFrames;
        if (supFaderStatus == JUTFader::None || s_equipSuppressWaitBlackFrames >= 600) {
            custom_equip_set_suppressed(true);
            s_equipSuppressWaitBlack = false;
            s_equipSuppressWaitBlackFrames = 0;
        }
    }

    if (s_needsChamberSpawn && is_in_boss_rush_chamber()) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            link->current.pos.set(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
            link->old.pos.set(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
            link->shape_angle.set(0, cM_deg2s(180.0f), 0);
            link->current.angle.set(0, cM_deg2s(180.0f), 0);
            link->speedF = 0.0f;
            link->speed.set(0.0f, 0.0f, 0.0f);

            camera_process_class* cam = boss_rush_get_active_player_camera();
            if (cam != nullptr) {
                static const s16 kChamberAngle = cM_deg2s(180.0f);
                const cXyz& p = link->current.pos;
                const f32 fx = cM_ssin(kChamberAngle), fz = cM_scos(kChamberAngle);
                cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
                cXyz eye(p.x - fx * 420.0f, p.y + 140.0f, p.z - fz * 420.0f);
                cam->mCamera.Reset(center, eye);
                cam->mCamera.Start();
                cam->mCamera.SetTrimSize(0);
                fopCamM_SetAngleY(cam, kChamberAngle);
            }

            s_chamberSpawnFrames++;
            if (s_chamberSpawnFrames >= 5) {
                s_needsChamberSpawn = false;
                s_chamberSpawnFrames = 0;
                clear_all_select_items();
                dComIfGs_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
                dComIfGp_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
                dComIfGs_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
                dComIfGp_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
            }
        }
    }

    if (is_in_boss_rush_chamber()) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        camera_process_class* cam = boss_rush_get_active_player_camera();
        if (s_chamberCamArmFrames > 0 && link != nullptr && cam != nullptr) {
            --s_chamberCamArmFrames;
            static const s16 kChamberAngle = cM_deg2s(180.0f);
            const cXyz& p = link->current.pos;
            const f32 fx = cM_ssin(kChamberAngle), fz = cM_scos(kChamberAngle);
            cXyz center(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f);
            cXyz eye(p.x - fx * 420.0f, p.y + 140.0f, p.z - fz * 420.0f);
            cam->mCamera.Reset(center, eye);
            cam->mCamera.Start();
            cam->mCamera.SetTrimSize(0);
            fopCamM_SetAngleY(cam, kChamberAngle);
        }
    } else {
        s_chamberCamArmFrames = 0;
    }

    if (s_portalArrivalAnimPending && is_in_boss_rush_chamber() && boss_rush_scene_load_stable() &&
        !s_pendingInitialInventory) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            if (link->mAnmHeap3.mAnimeHeap != nullptr &&
                link->getClothesChangeWaitTimer() == 0 &&
                link->mProcID != daAlink_c::PROC_WARP)
            {
                s_portalArrivalAnimPending = false;
                link->mNormalSpeed = 0.0f;
                link->speed.set(0.0f, 0.0f, 0.0f);
                if (link->procCoWarpInit(1, 0)) {
                    link->mProcVar0.field_0x3008 = 38;
                    link->offPlayerNoDraw();
                    s_chamberCamArmFrames = 122;
                    if (s_portalCineWasHuman) {
                        human_warp_cinematic_arrival();
                        s_portalCineWasHuman = false;
                    }
                }
            } else {
                link->onPlayerNoDraw();
            }
        }
    }

    s_exitSaveReloadFrames = 0;
    if (nowInChamber && s_chamberCleanupFrames > 0 && boss_rush_scene_load_stable()) {
        --s_chamberCleanupFrames;
        struct CleanupParams {
            bool did_delete_any;
        };
        static auto deleteCb = [](void* actor_void, void* data) -> void* {
            fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(actor_void);
            if (!actor || !fopAcM_IsActor(actor)) return nullptr;
            if (fopAcM_GetRoomNo(actor) != kBossRushChamberRoom) return nullptr;
            const s16 name = fopAcM_GetProfName(actor);
            if (name == fpcNm_TBOX_e || name == fpcNm_TBOX2_e) {
                fopAcM_delete(actor);
                CleanupParams* p = static_cast<CleanupParams*>(data);
                if (p) p->did_delete_any = true;
            }
            return nullptr;
        };
        CleanupParams params{false};
        fopAcIt_Judge(reinterpret_cast<fopAcIt_JudgeFunc>(+deleteCb), &params);
        if (params.did_delete_any) {
            s_chamberCleanupFrames = 0;
        }
    }

    if (s_killWatchdogFrames > 0) {
        --s_killWatchdogFrames;
        fopAc_ac_c* dying = boss_rush_find_current_boss_actor();
        if (dying != nullptr) {
            dying->health = 0;
        }
        if (boss_bar_boss_defeated_now()) {
            s_killWatchdogFrames = 0;
        } else if (s_killWatchdogFrames == 0) {
            boss_bar_force_defeat_event();
        }
    }

    if (boss_rush_is_fighting_here() && !s_returningToChamber && s_pendingFightIndex == -1 && boss_bar_consume_defeat_event()) {
        const int gt = boss_rush_target_index();
        {
            const char* dStage = dComIfGp_getStartStageName();
            boss_rush_debug_log("[gauntlet] defeat edge: target=%s stage=%s gphase=%d",
                                (gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount)
                                    ? g_bossGalleryTable[gt].displayName
                                    : "?",
                                dStage ? dStage : "?", boss_rush_gauntlet_phase());
        }
        if (!g_configBossRushSeparateGanon && gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            (std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Puppet Zelda") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Beast Ganon") == 0 ||
             std::strcmp(g_bossGalleryTable[gt].displayName, "Horseback Ganon") == 0)) {
            const char* curStage = dComIfGp_getStartStageName();
            const bool isGroundDuel = (curStage != nullptr && std::strcmp(curStage, "D_MN09B") == 0 &&
                                       (std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0 || s_gauntletPhase == 4) &&
                                       (g_dComIfG_gameInfo.info.getDan().isSwitch(1) || dComIfGs_isSaveDunSwitch(1) || s_gauntletPhase == 4));
            if (isGroundDuel) {
                s_gauntletLadderActive = false;
                advance_boss_rush_run(log_svc, mod_ctx, "Ganondorf defeated");
                return;
            }
            const int phase = boss_rush_gauntlet_phase();
            if (phase == 1 || std::strcmp(g_bossGalleryTable[gt].displayName, "Horseback Ganon") == 0) {
                // In-place transition between Horseback Ganon and Ganondorf Duel; do not commit stage warp
                return;
            }
            const char* want = (phase == 2)   ? "Beast Ganon"
                               : (phase == 3) ? "Horseback Ganon"
                               : nullptr;
            if (want != nullptr) {
                for (size_t i = 0; i < g_bossGalleryCount; ++i) {
                    if (std::strcmp(g_bossGalleryTable[i].displayName, want) == 0) {
                        s_gauntletLadderActive = true;
                        commit_boss_rush_fight_warp(i, daAlink_getAlinkActorClass(),
                                                    log_svc, mod_ctx);
                        return;
                    }
                }
            }
            return;
        }
        if (gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            std::strcmp(g_bossGalleryTable[gt].displayName, "Horseback Ganon") == 0) {
            // Horseback Ganon (separate-Ganon mode) drives its own defeat
            // cutscene/fade timing via s_horsebackGanonKoTimer below, based
            // on Ganondorf's own health/action state - this generic
            // boss-bar fade-out edge fires a bit earlier and would otherwise
            // warp away before the cutscene finishes playing.
            return;
        }
        advance_boss_rush_run(log_svc, mod_ctx, "Boss defeated");
        return;
    }

    if (boss_rush_is_fighting_here() && !s_returningToChamber && s_pendingFightIndex == -1) {
        const int gt = boss_rush_target_index();
        const char* hbStage = dComIfGp_getStartStageName();
        const bool isGroundDuelNow =
            (hbStage != nullptr && std::strcmp(hbStage, "D_MN09B") == 0 &&
             (g_dComIfG_gameInfo.info.getDan().isSwitch(1) ||
              dComIfGs_isSaveDunSwitch(1) ||
              s_gauntletPhase == 4));
        const bool isHorsebackTarget = (gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            !isGroundDuelNow &&
            (std::strcmp(g_bossGalleryTable[gt].displayName, "Horseback Ganon") == 0 ||
             (!g_configBossRushSeparateGanon &&
              std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0 &&
              hbStage != nullptr && std::strcmp(hbStage, "D_MN09B") == 0)));

        if (isHorsebackTarget) {
            static u32 s_hbGen = ~0u;
            static int s_hbLandingFrames = 0;
            static bool s_hbSepWaitingCutscene = false;
            static int s_hbCutsceneWaitFrames = 0;
            if (s_hbGen != s_fightWarpGen) {
                s_hbGen = s_fightWarpGen;
                s_hbLandingFrames = 120;
                s_horsebackGanonKoTimer = -1;
                s_horsebackGanonSawHorse = false;
                s_hbSepWaitingCutscene = false;
                s_hbCutsceneWaitFrames = 0;
            }
            if (s_hbLandingFrames > 0) {
                --s_hbLandingFrames;
            } else {
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (dComIfGp_getHorseActor() != nullptr && link != nullptr && link->checkHorseRide()) {
                s_horsebackGanonSawHorse = true;
            }
            if (s_horsebackGanonKoTimer >= 0) {
                if (!g_configBossRushSeparateGanon) {
                    fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e);
                    if (gnd != nullptr) {
                        b_gnd_class* g = reinterpret_cast<b_gnd_class*>(gnd);
                        g->mDemoCamMode = 0;
                        gnd->eventInfo.offCondition(2);
                    }
                    dComIfGp_event_reset();
                }

                if (!g_configBossRushSeparateGanon || s_horsebackGanonKoTimer <= 25) {
                    boss_rush_screen_fade_out(0.055f);
                }

                if ((s_horsebackGanonKoTimer % 10) == 0) {
                    fopAc_ac_c* gndDbg = fopAcM_SearchByName(fpcNm_B_GND_e);
                    int gam = -1, gmm = -1, gdcm = -1, ghorse = -1, ghp = -1, gkd = -1;
                    if (gndDbg != nullptr) {
                        bbi::ganondorf_read(gndDbg, gam, gmm, gdcm, ghorse, ghp, gkd);
                    }
                    JUTFader* dbgFader = mDoGph_gInf_c::getFader();
                    rush_debug_logf("[hb-dbg] koTimer=%d sep=%d gnd=%p gam=%d ghp=%d ghorse=%d "
                                    "fader=%d ev=%d stage=%s",
                                    s_horsebackGanonKoTimer, (int)g_configBossRushSeparateGanon,
                                    gndDbg, gam, ghp, ghorse,
                                    dbgFader ? (int)dbgFader->getStatus() : -1,
                                    (int)dComIfGp_event_runCheck(),
                                    dComIfGp_getStartStageName() ? dComIfGp_getStartStageName() : "?");
                }

                if (s_horsebackGanonKoTimer > 0) {
                    --s_horsebackGanonKoTimer;
                } else {
                    if (!boss_rush_screen_is_fully_black()) {
                        boss_rush_screen_fade_out(0.08f);
                        return;
                    }
                    s_horsebackGanonKoTimer = -1;
                    s_horsebackGanonSawHorse = false;
                    boss_rush_screen_hold_black();
                    if (!g_configBossRushSeparateGanon) {
                        s_gauntletLadderActive = true;
                        trigger_ganon_ground_duel();
                        return;
                    }
                    advance_boss_rush_run(log_svc, mod_ctx, "Horseback Ganon defeated");
                    return;
                }
            } else if (g_configBossRushSeparateGanon && s_hbSepWaitingCutscene) {
                // Defeat already detected (mActionMode == ACTION_HEND). The
                // fall-off-horse animation isn't an engine event - it's
                // b_gnd_h_end()'s own mMoveMode/mDemoCamMode sub-state
                // machine (camera mode climbs 30 -> 32 -> 34 as the horse-down
                // and Ganondorf-down animations play), which just sits once
                // settled waiting for external code to move on. Wait for
                // mDemoCamMode to reach 34 before starting the short fade.
                // Capped as a safety net in case it never gets there.
                ++s_hbCutsceneWaitFrames;
                fopAc_ac_c* gndWait = fopAcM_SearchByName(fpcNm_B_GND_e);
                int wGam = -1, wMoveMode = -1, wDemoCam = -1, wHorse = -1, wHp = -1, wKd = -1;
                if (gndWait != nullptr) {
                    bbi::ganondorf_read(gndWait, wGam, wMoveMode, wDemoCam, wHorse, wHp, wKd);
                }
                if (gndWait == nullptr || wDemoCam >= 34 || s_hbCutsceneWaitFrames > 900) {
                    rush_debug_logf("[hb-dbg] fall animation done demoCam=%d moveMode=%d frames=%d, "
                                    "starting fade",
                                    wDemoCam, wMoveMode, s_hbCutsceneWaitFrames);
                    s_hbSepWaitingCutscene = false;
                    s_hbCutsceneWaitFrames = 0;
                    s_horsebackGanonKoTimer = 25;
                }
            } else if (s_horsebackGanonSawHorse) {
                fopAc_ac_c* gnd = fopAcM_SearchByName(fpcNm_B_GND_e);
                if (gnd != nullptr) {
                    int gam, gmm, gdcm, ghorse, ghp, gkd;
                    bbi::ganondorf_read(gnd, gam, gmm, gdcm, ghorse, ghp, gkd);
                    if (gam == 6 || ghp <= 0) {
                        rush_debug_logf("[hb-dbg] defeat edge sep=%d gam=%d gmm=%d gdcm=%d "
                                        "ghorse=%d ghp=%d gkd=%d",
                                        (int)g_configBossRushSeparateGanon, gam, gmm, gdcm,
                                        ghorse, ghp, gkd);
                        if (!g_configBossRushSeparateGanon) {
                            s_horsebackGanonKoTimer = 22;
                            boss_rush_screen_fade_out(0.055f);
                            b_gnd_class* g = reinterpret_cast<b_gnd_class*>(gnd);
                            g->mDemoCamMode = 0;
                            gnd->eventInfo.offCondition(2);
                            dComIfGp_event_reset();
                        } else {
                            boss_rush_timer_notify_defeat();
                            s_hbSepWaitingCutscene = true;
                        }
                    }
                }
            }
            }
        } else {
            s_horsebackGanonKoTimer = -1;
            s_horsebackGanonSawHorse = false;
        }
    } else {
        s_horsebackGanonKoTimer = -1;
        s_horsebackGanonSawHorse = false;
    }

    if (boss_rush_is_fighting_here() && !s_returningToChamber && s_pendingFightIndex == -1) {
        const int gt = boss_rush_target_index();
        const char* bgStage = dComIfGp_getStartStageName();
        const bool isBeastGanonFight = (gt >= 0 && static_cast<size_t>(gt) < g_bossGalleryCount &&
            (std::strcmp(g_bossGalleryTable[gt].displayName, "Beast Ganon") == 0 ||
             (!g_configBossRushSeparateGanon &&
              std::strcmp(g_bossGalleryTable[gt].displayName, "Ganondorf") == 0 &&
              bgStage != nullptr && std::strcmp(bgStage, "D_MN09A") == 0)));
        if (isBeastGanonFight) {
            if (s_beastGanonKoTimer > 0) {
                if (--s_beastGanonKoTimer <= 0) {
                    s_beastGanonKoTimer = -1;
                    if (!g_configBossRushSeparateGanon) {
                        const int hbIdx = gauntlet_next_phase_index("Beast Ganon");
                        if (hbIdx >= 0) {
                            s_gauntletLadderActive = true;
                            commit_boss_rush_fight_warp(static_cast<size_t>(hbIdx),
                                                        daAlink_getAlinkActorClass(),
                                                        log_svc, mod_ctx);
                            return;
                        }
                    }
                    advance_boss_rush_run(log_svc, mod_ctx, "Beast Ganon defeated");
                    return;
                }
            } else if (s_beastGanonKoTimer < 0) {
                fopAc_ac_c* mgn = fopAcM_SearchByName(fpcNm_B_MGN_e);
                if (mgn != nullptr) {
                    int mam, mhp;
                    bool misDown;
                    bbi::beastganon_read(mgn, mam, mhp, misDown);
                    int aff, downTimer;
                    bool down;
                    bbi::beastganon_progress(mgn, aff, down, downTimer);
                    if (mam == daB_MGN_c::ACTION_DEATH_e || aff >= 6) {
                        s_beastGanonKoTimer = 30;
                    }
                }
            }
        } else {
            s_beastGanonKoTimer = -1;
        }
    } else {
        s_beastGanonKoTimer = -1;
    }

    if (!nowInChamber || s_pendingFightIndex != -1) {
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return;
    }

    const size_t activeCount = boss_rush_get_active_gallery_count();
    for (size_t circleSlot = 0; circleSlot < activeCount; ++circleSlot) {
        const size_t i = boss_rush_get_active_gallery_table_index(circleSlot);
        const BossGalleryEntry& boss = g_bossGalleryTable[i];

        cXyz bossPos;
        csXyz bossAngle;
        boss_rush_get_slot_transform(circleSlot, activeCount, kBossChamberFloorY, bossPos, bossAngle);

        const f32 dx = link->current.pos.x - bossPos.x;
        const f32 dz = link->current.pos.z - bossPos.z;
        const f32 distSq = dx * dx + dz * dz;

        if (distSq < kBossInteractRadius * kBossInteractRadius) {
            if (mDoCPd_c::getTrigA(PAD_1)) {
                commit_boss_rush_fight_warp(i, link, log_svc, mod_ctx);
                break;
            }
        }
    }

    // if (boss_rush_master_sword_near() && mDoCPd_c::getTrigA(PAD_1)) {
    //     start_boss_rush_full_run(log_svc, mod_ctx);
    // }
}

ModResult init_boss_rush(const HookService* hook_svc, const LogService* log_svc,
                         const UiService*, const ConfigService*,
                         ModContext* mod_ctx, ModError*) {
    s_modCtx = mod_ctx;
    s_logSvc = log_svc;

    reset_boss_rush_models();
    init_boss_rush_save(log_svc, mod_ctx);
    ensure_system_heap_capacity();

    if (hook_svc) {
        mods::hook::add_post<BossRushDrawHook>(hook_svc, on_boss_rush_draw_post);
        mods::hook::add_post<BossRushAlinkExecuteHook>(hook_svc, on_boss_rush_alink_execute_post);
        mods::hook::add_pre<BossRushAlinkExecuteHook>(hook_svc, on_boss_rush_alink_execute_pre);
        mods::hook::add_pre<BossRushMeterDrawHook>(hook_svc, on_boss_rush_meter_draw_pre);
        mods::hook::add_post<BossRushMeterDrawHook>(hook_svc, on_boss_rush_meter_draw_post);
        mods::hook::add_post<BossRushActionStringHook>(hook_svc, on_action_string_post);
        mods::hook::add_pre<BossRushDefeatOverrideHook>(hook_svc, on_defeat_check_pre);
        mods::hook::add_pre<BossRushSwitchOverrideHook>(hook_svc, on_switch_check_pre);
        mods::hook::add_pre<BossRushDanSwitchHook>(hook_svc, on_dan_switch_check_pre);
        mods::hook::add_pre<BossRushZoneSwitchHook>(hook_svc, on_zone_switch_check_pre);
        mods::hook::add_pre<BossRushChangeSceneHook>(hook_svc, on_change_scene_pre);
        mods::hook::add_pre<BossRushChangeSceneFightGuardHook>(hook_svc, on_change_scene_fight_guard_pre);
        mods::hook::add_pre<BossRushChangeScene4EventHook>(hook_svc, on_change_scene4_event_pre);
        mods::hook::add_pre<BossRushDoorOpenHook>(hook_svc, on_door_open_pre);
        mods::hook::add_pre<DarkhammerArmorExecuteHook>(hook_svc, on_darkhammer_armor_execute_pre);
        mods::hook::add_pre<BossRushBeastGanonArrowHook>(hook_svc, on_beastganon_arrow_hit_pre);
        mods::hook::add_pre<BossRushBeastGanonTransformFreezeHook>(hook_svc, on_beastganon_execute_pre);
        mods::hook::add_pre<BossRushBeastGanonDamageHook>(hook_svc, on_bmg_damage_pre);
        mods::hook::add_post<BossRushBeastGanonDamageHook>(hook_svc, on_bmg_damage_post);

        mods::hook::add_pre<BossRushBossDoorExecuteHook>(hook_svc, on_bossdoor_execute_pre);
        mods::hook::add_pre<BossRushDungeonReturnWarp>(hook_svc, on_dungeon_return_warp_pre);
        mods::hook::add_pre<BossRushSkipPortalObjWarp>(hook_svc, on_skip_portal_obj_warp_pre);
        mods::hook::add_pre<BossRushProcCoDeadHook>(hook_svc, on_proc_co_dead_pre);
    }

    init_boss_rush_qol_overrides(hook_svc);

    if (hook_svc != nullptr) {
        mods::hook::add_pre<BossRushAchievementTickHook>(hook_svc, on_boss_rush_achievement_tick_pre);
    }
    init_boss_rush_midna(hook_svc, log_svc, mod_ctx);
    init_boss_rush_collection(hook_svc, log_svc, mod_ctx);
    init_ganondorf_cape(hook_svc, log_svc, mod_ctx);

    // Only resume boss rush on init when a session was actually started
    // (persisted marker); the chamber room and boss arenas are all reachable
    // through vanilla progression, so the stage check alone is not enough.
    bool resumedBossRush = false;
    if (is_game_resetting_or_title()) {
        clear_boss_rush_session_marker();
    } else if (boss_rush_session_marker_present()) {
        if (is_in_chamber_room()) {
            resumedBossRush = true;
            s_bossRushModeActive = true;
            force_boss_rush_fast_transitions();
            s_needsChamberSpawn = true;
            s_chamberSpawnFrames = 0;
            s_chamberCamArmFrames = 0;
            cXyz spawnPos(0.0f, kBossChamberFloorY, kBossChamberSpawnZ);
            dComIfGs_setRestartRoom(spawnPos, cM_deg2s(180.0f), kBossRushChamberRoom);
            dComIfGs_setRestartRoomParam((kBossRushChamberRoom & 0x3F) | (0xFF << 24));
            daAlink_c* link = daAlink_getAlinkActorClass();
            if (link != nullptr) {
                link->current.pos = spawnPos;
                link->old.pos = spawnPos;
                link->shape_angle.set(0, cM_deg2s(180.0f), 0);
                link->current.angle.set(0, cM_deg2s(180.0f), 0);
                link->speedF = 0.0f;
                link->speed.set(0.0f, 0.0f, 0.0f);

                camera_process_class* cam = boss_rush_get_active_player_camera();
                if (cam != nullptr) {
                    static const s16 kChamberAngle = cM_deg2s(180.0f);
                    const f32 fx = cM_ssin(kChamberAngle), fz = cM_scos(kChamberAngle);
                    cXyz center(spawnPos.x + fx * 200.0f, spawnPos.y + 100.0f, spawnPos.z + fz * 200.0f);
                    cXyz eye(spawnPos.x - fx * 420.0f, spawnPos.y + 140.0f, spawnPos.z - fz * 420.0f);
                    cam->mCamera.Reset(center, eye);
                    cam->mCamera.Start();
                    cam->mCamera.SetTrimSize(0);
                    fopCamM_SetAngleY(cam, kChamberAngle);
                }
            }
            if (dComIfGs_getLife() == 0) {
                dComIfGs_setLife(full_life_for_max(dComIfGs_getMaxLife()));
                sync_life_meter_instant(full_life_for_max(dComIfGs_getMaxLife()),
                                        dComIfGs_getMaxLife());
            }
            // A mod reload re-inits the custom-equip module fresh, dropping
            // its suppression flag even though the boss rush session (and
            // the vanilla loadout it enforces) is still active - reapply it
            // the same way a fresh chamber entry does.
            s_pendingInitialInventory = true;
        } else if (const char* curStage = dComIfGp_getStartStageName()) {
            const s8 curRoom = static_cast<s8>(dComIfGp_roomControl_getStayNo());
            for (size_t i = 0; i < g_bossGalleryCount; ++i) {
                const BossGalleryEntry& boss = g_bossGalleryTable[i];
                if (std::strcmp(curStage, boss.stage) != 0) {
                    continue;
                }
                if (!boss_rush_room_matches_target(boss, curRoom)) {
                    continue;
                }
                if (std::strcmp(curStage, "D_MN09B") == 0) {
                    const bool isGround = (g_dComIfG_gameInfo.info.getDan().isSwitch(1) ||
                                           dComIfGs_isSaveDunSwitch(1) ||
                                           s_gauntletPhase == 4);
                    if (isGround && std::strcmp(boss.displayName, "Horseback Ganon") == 0) {
                        continue;
                    }
                    if (!isGround && std::strcmp(boss.displayName, "Ganondorf") == 0) {
                        continue;
                    }
                }
                if (std::strcmp(curStage, "D_MN09A") == 0) {
                    const s32 curLayer = dComIfG_play_c::getLayerNo(0);
                    if (curLayer == 1 && std::strcmp(boss.displayName, "Puppet Zelda") == 0) {
                        continue;
                    }
                    if (curLayer == 0 && std::strcmp(boss.displayName, "Beast Ganon") == 0) {
                        continue;
                    }
                }
                resumedBossRush = true;
                s_bossRushModeActive = true;
                force_boss_rush_fast_transitions();
                s_activeFightIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (!resumedBossRush) {
        clear_boss_rush_session_marker();
    }


    return MOD_OK;
}

void shutdown_boss_rush() {
    if (is_game_resetting_or_title()) {
        close_boss_rush_session();
    }
    shutdown_boss_rush_save();
    if (!s_bossRushModeActive && !s_exitingBossRush) {
        clear_boss_rush_session_marker();
    }
    if (s_shouldAutoSaveCaptured && s_engineShouldAutoSave != nullptr) {
        *s_engineShouldAutoSave = s_shouldAutoSaveOriginal;
    }
    s_shouldAutoSaveCaptured = false;
    s_chamberEquipsPending = false;
    s_chamberEquipsFrames = 0;
    s_equipSuppressWaitBlack = false;
    s_equipSuppressWaitBlackFrames = 0;
    s_bossRushModeActive = false;
    s_swordDrawnLatched = false;
    s_activeFightIndex = -1;
    s_pendingFightIndex = -1;
    s_returningToChamber = false;
    s_returnSawFadeOut = false;
    s_needsChamberSpawn = false;
    s_chamberSpawnFrames = 0;
    s_chamberCamArmFrames = 0;
    s_portalArrivalAnimPending = false;
    s_ignitedStatue = -1;
    s_recordReturnFrames = -1;
    s_recordReturnReason = nullptr;
    s_pendingFightFromArena = false;
    s_dungeonClearWarpPending = false;
    s_dungeonClearWarpFrames  = 0;
    s_morpheelPosPinFrames = 0;
    s_morpheelCamArmFrames = 0;
    clear_ring_flames();
    unload_boss_rush_models();
    shutdown_boss_rush_midna();
    shutdown_ganondorf_cape();
}
