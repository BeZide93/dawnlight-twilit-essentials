#include "quick_access_itemwheel.hpp"
#include "quick_access.hpp"
#include "quick_access_internal.hpp"
#include "../boss_rush/boss_rush.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "mods/svc/hook.hpp"

#include <cstdio>

bool g_configQuickAccessHideWheelItems = false;

bool itemwheel_filter_active() {
    if (is_boss_rush_active()) {
        return false;
    }
    return g_configQuickAccessEnabled && g_configQuickAccessHideWheelItems;
}

static bool itemwheel_is_hidden_item(u8 itemNo) {
    return qa_custom_contains_family(itemNo);
}

DEFINE_HOOK(&dSv_player_item_c::setLineUpItem, SvSetLineUpItemQuickAccessHook);

static void on_set_line_up_item_quick_access_post(ModContext*, void* args, void*, void*) {
    if (!itemwheel_filter_active()) {
        return;
    }
    dSv_player_item_c* item = mods::arg<dSv_player_item_c*>(args, 0);
    if (item == nullptr) {
        return;
    }

    u8 w = 0;
    for (u8 r = 0; r < MAX_ITEM_SLOTS; r++) {
        u8 slot = item->mItemSlots[r];
        if (slot == 0xFF) {
            continue;
        }
        u8 itemNo = item->mItems[slot];
        if (itemNo != dItemNo_NONE_e && !itemwheel_is_hidden_item(itemNo)) {
            item->mItemSlots[w++] = slot;
        }
    }
    for (u8 i = w; i < MAX_ITEM_SLOTS; i++) {
        item->mItemSlots[i] = 0xFF;
    }
}

void quick_access_itemwheel_refresh() {
    g_dComIfG_gameInfo.info.getPlayer().getItem().setLineUpItem();
}

static bool itemwheel_lineup_has_hidden_item() {
    const dSv_player_item_c& item = g_dComIfG_gameInfo.info.getPlayer().getItem();
    for (u8 i = 0; i < MAX_ITEM_SLOTS; i++) {
        u8 slot = item.mItemSlots[i];
        if (slot != 0xFF && itemwheel_is_hidden_item(item.mItems[slot])) {
            return true;
        }
    }
    return false;
}

static const u8 kLineupItemSlots[23] = {
    10, 8, 6, 2, 9, 4, 3, 0, 1, 23, 20, 5, 15, 16, 17, 11, 12, 13, 14, 19, 18, 22, 21,
};

static int itemwheel_lineup_missing_count() {
    const dSv_player_item_c& item = g_dComIfG_gameInfo.info.getPlayer().getItem();
    const bool filter = itemwheel_filter_active();
    int expected = 0;
    for (u8 i = 0; i < 23; i++) {
        u8 itemNo = item.mItems[kLineupItemSlots[i]];
        if (itemNo != dItemNo_NONE_e && !(filter && itemwheel_is_hidden_item(itemNo))) {
            expected++;
        }
    }
    int linedUp = 0;
    for (u8 i = 0; i < MAX_ITEM_SLOTS; i++) {
        if (item.mItemSlots[i] != 0xFF) {
            linedUp++;
        }
    }
    return expected - linedUp;
}

ModResult init_quick_access_itemwheel(const HookService* hook_svc, ModError*) {
    if (hook_svc) {
        mods::hook::add_post<SvSetLineUpItemQuickAccessHook>(hook_svc,
            on_set_line_up_item_quick_access_post);
    }
    return MOD_OK;
}

void update_quick_access_itemwheel() {
    if (itemwheel_lineup_missing_count() > 0) {
        quick_access_itemwheel_refresh();
    } else if (itemwheel_filter_active() && itemwheel_lineup_has_hidden_item()) {
        quick_access_itemwheel_refresh();
    }
}

void shutdown_quick_access_itemwheel() {}
