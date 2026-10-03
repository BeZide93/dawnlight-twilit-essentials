#include "auto_zora_armor.hpp"
#include "../collection_menu/collection_menu.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_alink.h"

bool g_configAutoZoraArmor = false;

DEFINE_HOOK(&daAlink_c::setStickData, AutoZoraStickDataHook);

static constexpr int kNoTunic = -1;
static constexpr int kLandFrames = 30;

static bool s_autoEquipped = false;
static int s_prevTunic = kNoTunic;
static int s_landFrames = 0;

static void reset_state() {
    s_autoEquipped = false;
    s_prevTunic = kNoTunic;
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

static void tick_auto_zora_armor(daAlink_c* link) {
    if (link != daAlink_getAlinkActorClass()) return;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause() || link->checkWolf()) return;

    if (s_autoEquipped && !collection_tunic_equipped(COLLECTION_TUNIC_ZORA)) reset_state();

    if (link_in_water(link)) {
        s_landFrames = 0;
        if (s_autoEquipped || !collection_tunic_unlocked(COLLECTION_TUNIC_ZORA) ||
            collection_tunic_equipped(COLLECTION_TUNIC_ZORA)) {
            return;
        }
        const int prev = equipped_tunic();
        if (collection_tunic_equip(COLLECTION_TUNIC_ZORA, true)) {
            s_autoEquipped = true;
            s_prevTunic = prev;
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
    if (collection_tunic_equip(target)) reset_state();
}

static void on_stick_data_post(ModContext*, void* args, void*, void*) {
    if (!g_configAutoZoraArmor || !args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    tick_auto_zora_armor(link);
}

ModResult init_auto_zora_armor(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    return mods::hook::add_post<AutoZoraStickDataHook>(hook_svc, on_stick_data_post);
}

void update_auto_zora_armor() {
    if (!g_configAutoZoraArmor || daAlink_getAlinkActorClass() == nullptr) reset_state();
}

void shutdown_auto_zora_armor() {
    reset_state();
}
