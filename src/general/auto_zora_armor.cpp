#include "auto_zora_armor.hpp"
#include "zora_armor_fx.hpp"
#include "../collection_menu/collection_menu.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_alink.h"

#include <algorithm>

bool g_configAutoZoraArmor = false;

DEFINE_HOOK(&daAlink_c::setStickData, AutoZoraStickDataHook);
DEFINE_HOOK(&daAlink_c::changeLink, AutoZoraChangeLinkHook);

static constexpr int kNoTunic = -1;
static constexpr int kLandFrames = 30;
static constexpr s16 kZoraDiveTimer = 30;
static constexpr int kDiveEntryWindow = 2;
static constexpr int kLateDiveWindow = 8;

static bool s_autoEquipped = false;
static int s_prevTunic = kNoTunic;
static int s_prevFxType = ZORA_FX_NONE;
static int s_landFrames = 0;
static int s_ticksSinceDive = 99;
static bool s_diveSwap = false;
static bool s_keepPoseBlend = false;
static bool s_restorePoseBlend = false;

static void reset_state() {
    s_autoEquipped = false;
    s_prevTunic = kNoTunic;
    s_prevFxType = ZORA_FX_NONE;
    s_landFrames = 0;
}

static int equipped_tunic() {
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        if (collection_tunic_equipped(t)) return t;
    }
    return kNoTunic;
}

static bool link_in_water(daAlink_c* link) {
    return link->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
           link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE);
}

static bool safe_to_revert(daAlink_c* link) {
    if (link->mProcID != daAlink_c::PROC_WAIT && link->mProcID != daAlink_c::PROC_MOVE) return false;
    return link->mLinkAcch.ChkGroundHit();
}

static void preload_fx(daAlink_c* link) {
    if (!collection_tunic_unlocked(COLLECTION_TUNIC_ZORA)) return;
    zora_armor_fx_preload(ZORA_FX_ZORA);
    zora_armor_fx_preload(zora_armor_fx_current_type(link));
    if (s_autoEquipped) zora_armor_fx_preload(s_prevFxType);
}

static void tick_auto_zora_armor(daAlink_c* link) {
    if (link != daAlink_getAlinkActorClass()) return;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause() || link->checkWolf()) return;

    const bool zoraOn = collection_tunic_equipped(COLLECTION_TUNIC_ZORA);
    const bool pending = collection_tunic_change_pending();
    if (s_autoEquipped && !zoraOn && !pending) reset_state();
    if (s_keepPoseBlend && link->getClothesChangeWaitTimer() == 0) s_keepPoseBlend = false;
    preload_fx(link);

    const bool cameFromDive = s_ticksSinceDive <= kDiveEntryWindow;
    s_ticksSinceDive = link->mProcID == daAlink_c::PROC_DIVE_JUMP ? 0 : std::min(s_ticksSinceDive + 1, 99);

    if (s_diveSwap && (zoraOn || !pending)) {
        s_diveSwap = false;
        if (zoraOn && link->mProcID == daAlink_c::PROC_SWIM_MOVE && s_ticksSinceDive <= kLateDiveWindow) {
            link->field_0x3000 = kZoraDiveTimer;
        }
    }

    if (link->mProcID == daAlink_c::PROC_DIVE_JUMP && !s_autoEquipped && !zoraOn &&
        collection_tunic_unlocked(COLLECTION_TUNIC_ZORA)) {
        const int prev = equipped_tunic();
        const int fromType = zora_armor_fx_current_type(link);
        if (collection_tunic_equip(COLLECTION_TUNIC_ZORA,
                                   TUNIC_EQUIP_SEAMLESS | TUNIC_EQUIP_SILENT | TUNIC_EQUIP_ANY_POSE)) {
            s_autoEquipped = true;
            s_prevTunic = prev;
            s_prevFxType = fromType;
            s_diveSwap = true;
            zora_armor_fx_begin(link, fromType, ZORA_FX_ZORA);
        }
        return;
    }

    if (link_in_water(link)) {
        s_landFrames = 0;
        if (s_autoEquipped || !collection_tunic_unlocked(COLLECTION_TUNIC_ZORA) ||
            collection_tunic_equipped(COLLECTION_TUNIC_ZORA)) {
            return;
        }
        const int prev = equipped_tunic();
        const int fromType = zora_armor_fx_current_type(link);
        if (collection_tunic_equip(COLLECTION_TUNIC_ZORA, TUNIC_EQUIP_SWIMMING | TUNIC_EQUIP_SILENT)) {
            s_autoEquipped = true;
            s_prevTunic = prev;
            s_prevFxType = fromType;
            s_keepPoseBlend = true;
            if (cameFromDive && link->mProcID == daAlink_c::PROC_SWIM_MOVE) link->field_0x3000 = kZoraDiveTimer;
            zora_armor_fx_begin(link, fromType, ZORA_FX_ZORA);
        }
        return;
    }

    if (!s_autoEquipped) return;
    if (s_landFrames < kLandFrames) {
        s_landFrames++;
        return;
    }
    if (!safe_to_revert(link)) return;

    const int target = s_prevTunic == kNoTunic ? COLLECTION_TUNIC_ZORA : s_prevTunic;
    if (!collection_tunic_unlocked(target)) {
        reset_state();
        return;
    }
    const int fromType = zora_armor_fx_current_type(link);
    if (collection_tunic_equip(target, TUNIC_EQUIP_SEAMLESS | TUNIC_EQUIP_SILENT)) {
        zora_armor_fx_begin(link, fromType, s_prevFxType);
        reset_state();
    }
}

static void on_stick_data_post(ModContext*, void* args, void*, void*) {
    if (!g_configAutoZoraArmor || !args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    tick_auto_zora_armor(link);
}

static HookAction on_change_link_pre(ModContext*, void* args, void*, void*) {
    s_restorePoseBlend = false;
    if (!s_keepPoseBlend || !args) return HOOK_CONTINUE;
    s_keepPoseBlend = false;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const int rebuild = mods::arg<int>(args, 1);
    if (link == nullptr || link != daAlink_getAlinkActorClass() || rebuild == 0 || link->checkWolf()) {
        return HOOK_CONTINUE;
    }
    if (link->mProcID == daAlink_c::PROC_METAMORPHOSE || link->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY) {
        return HOOK_CONTINUE;
    }
    s_restorePoseBlend = link->field_0x2060 != nullptr && link->field_0x2060->getOldFrameFlg();
    return HOOK_CONTINUE;
}

static void on_change_link_post(ModContext*, void* args, void*, void*) {
    if (!s_restorePoseBlend || !args) return;
    s_restorePoseBlend = false;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && link->field_0x2060 != nullptr && !link->checkWolf()) {
        link->field_0x2060->onOldFrameFlg();
    }
}

ModResult init_auto_zora_armor(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    ModResult result = mods::hook::add_post<AutoZoraStickDataHook>(hook_svc, on_stick_data_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<AutoZoraChangeLinkHook>(hook_svc, on_change_link_pre);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<AutoZoraChangeLinkHook>(hook_svc, on_change_link_post);
}

void update_auto_zora_armor() {
    if (g_configAutoZoraArmor && daAlink_getAlinkActorClass() != nullptr) return;
    reset_state();
    s_ticksSinceDive = 99;
    s_diveSwap = false;
    s_keepPoseBlend = false;
}

void shutdown_auto_zora_armor() {
    reset_state();
    s_ticksSinceDive = 99;
    s_diveSwap = false;
    s_keepPoseBlend = false;
    s_restorePoseBlend = false;
}
