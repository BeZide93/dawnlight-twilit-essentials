#include "quick_access_itemwheel.hpp"
#include "quick_access.hpp"
#include "quick_access_internal.hpp"
#include "../boss_rush/boss_rush.hpp"
#include "../z_button/z_button.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_window.h"
#include "d/d_meter2_info.h"
#include "d/d_save.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/save.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

extern const LogService* svc_log;

bool g_configQuickAccessHideWheelItems = false;

static ModContext* s_wheelCtx = nullptr;
static const SaveService* s_wheelSaveSvc = nullptr;
static const char* kItemWheelBlobName = "itemWheelUnlockedV1";

struct ItemWheelBlob {
    u8 count;
    u8 slots[MAX_ITEM_SLOTS];
    u8 items[MAX_ITEM_SLOTS];
    u8 pad[3];
};
static_assert(sizeof(ItemWheelBlob) == 52, "blob size");

static ItemWheelBlob s_unlocked{};
static bool s_unlockedValid = false;
static bool s_verifyPending = false;
static bool s_filterApplied = false;
static bool s_restoring = false;

static const u8 kLineupItemSlots[23] = {
    10, 8, 6, 2, 9, 4, 3, 0, 1, 23, 20, 5, 15, 16, 17, 11, 12, 13, 14, 19, 18, 22, 21,
};

bool itemwheel_filter_active() {
    if (s_restoring || is_boss_rush_active()) {
        return false;
    }
    return g_configQuickAccessEnabled && g_configQuickAccessHideWheelItems;
}

static dSv_player_item_c& itemwheel_inventory() {
    return g_dComIfG_gameInfo.info.getPlayer().getItem();
}

static bool itemwheel_ring_open() {
    dMw_c* mw = g_meter2_info.getMenuWindowClass();
    return mw != nullptr && mw->mpMenuRing != nullptr;
}

static void itemwheel_log(const char* fmt, ...) {
    if (svc_log == nullptr || s_wheelCtx == nullptr) return;
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    svc_log->warn(s_wheelCtx, msg);
}

static int itemwheel_build_full(const dSv_player_item_c& item, u8 out[MAX_ITEM_SLOTS]) {
    std::memset(out, 0xFF, MAX_ITEM_SLOTS);
    int n = 0;
    for (u8 slot : kLineupItemSlots) {
        if (item.mItems[slot] != dItemNo_NONE_e) {
            out[n++] = slot;
        }
    }
    return n;
}

static bool itemwheel_slot_on_button(u8 slot) {
    const int lastButton = isNativeZButtonEngine() ? SELECT_ITEM_Y + 1 : SELECT_ITEM_Y;
    for (int b = SELECT_ITEM_X; b <= lastButton; b++) {
        if (dComIfGs_getSelectItemIndex(b) == slot || dComIfGs_getMixItemIndex(b) == slot) {
            return true;
        }
    }
    return false;
}

static void itemwheel_hidden_slots(const dSv_player_item_c& item, bool hidden[MAX_ITEM_SLOTS]) {
    bool claimed[MAX_ITEM_SLOTS] = {};
    for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
        hidden[i] = false;
    }
    if (!itemwheel_filter_active()) {
        return;
    }
    for (u8 slot = SLOT_11; slot <= SLOT_14; slot++) {
        if (item.mItems[slot] != dItemNo_NONE_e) {
            claimed[slot] = true;
            hidden[slot] = !itemwheel_slot_on_button(slot);
        }
    }
    for (int q = 0; q < QA_QUICK_SLOTS; q++) {
        const u8 qaItem = qa_custom_item(q);
        if (qaItem == QA_ITEM_NONE) {
            continue;
        }
        for (u8 slot = 0; slot < MAX_ITEM_SLOTS; slot++) {
            const u8 itemNo = item.mItems[slot];
            if (itemNo == dItemNo_NONE_e || claimed[slot] || !qa_items_same_family(qaItem, itemNo)) {
                continue;
            }
            claimed[slot] = true;
            hidden[slot] = !itemwheel_slot_on_button(slot);
            break;
        }
    }
}

static void itemwheel_build_desired(const dSv_player_item_c& item, u8 out[MAX_ITEM_SLOTS]) {
    u8 full[MAX_ITEM_SLOTS];
    const int n = itemwheel_build_full(item, full);
    bool hidden[MAX_ITEM_SLOTS];
    itemwheel_hidden_slots(item, hidden);
    std::memset(out, 0xFF, MAX_ITEM_SLOTS);
    int w = 0;
    for (int i = 0; i < n; i++) {
        if (!hidden[full[i]]) {
            out[w++] = full[i];
        }
    }
}

int itemwheel_full_lineup(u8 out[MAX_ITEM_SLOTS]) {
    return itemwheel_build_full(itemwheel_inventory(), out);
}

static bool itemwheel_lineup_contains(const dSv_player_item_c& item, u8 slot) {
    for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
        if (item.mItemSlots[i] == slot) {
            return true;
        }
    }
    return false;
}

static bool itemwheel_unlocked_missing(const dSv_player_item_c& item) {
    u8 full[MAX_ITEM_SLOTS];
    const int n = itemwheel_build_full(item, full);
    for (int i = 0; i < n; i++) {
        if (!itemwheel_lineup_contains(item, full[i])) {
            return true;
        }
    }
    if (s_unlockedValid) {
        for (int i = 0; i < s_unlocked.count && i < MAX_ITEM_SLOTS; i++) {
            const u8 slot = s_unlocked.slots[i];
            if (slot < MAX_ITEM_SLOTS && item.mItems[slot] != dItemNo_NONE_e &&
                !itemwheel_lineup_contains(item, slot)) {
                return true;
            }
        }
    }
    return false;
}

static void itemwheel_restore_full() {
    s_restoring = true;
    itemwheel_inventory().setLineUpItem();
    s_restoring = false;
    s_filterApplied = false;
}

static void itemwheel_apply_filter() {
    dSv_player_item_c& item = itemwheel_inventory();
    u8 desired[MAX_ITEM_SLOTS];
    itemwheel_build_desired(item, desired);
    if (std::memcmp(item.mItemSlots, desired, MAX_ITEM_SLOTS) != 0) {
        std::memcpy(item.mItemSlots, desired, MAX_ITEM_SLOTS);
    }
    s_filterApplied = true;
}

static void itemwheel_verify_unlocked(const dSv_player_item_c& item) {
    if (!s_unlockedValid) {
        return;
    }
    for (int i = 0; i < s_unlocked.count && i < MAX_ITEM_SLOTS; i++) {
        const u8 slot = s_unlocked.slots[i];
        if (slot < MAX_ITEM_SLOTS && item.mItems[slot] == dItemNo_NONE_e) {
            itemwheel_log("[QuickAccess] item wheel: item %u in slot %u was unlocked but is no "
                          "longer in the inventory", s_unlocked.items[i], slot);
        }
    }
}

static void itemwheel_record_unlocked(const dSv_player_item_c& item) {
    if (s_wheelSaveSvc == nullptr || s_wheelCtx == nullptr || is_boss_rush_active()) {
        return;
    }
    ItemWheelBlob blob;
    std::memset(&blob, 0xFF, sizeof(blob));
    blob.count = static_cast<u8>(itemwheel_build_full(item, blob.slots));
    for (int i = 0; i < blob.count; i++) {
        blob.items[i] = item.mItems[blob.slots[i]];
    }
    std::memset(blob.pad, 0, sizeof(blob.pad));
    if (s_unlockedValid && std::memcmp(&blob, &s_unlocked, sizeof(blob)) == 0) {
        return;
    }
    if (s_wheelSaveSvc->set_blob(s_wheelCtx, kItemWheelBlobName, &blob, sizeof(blob)) == MOD_OK) {
        s_unlocked = blob;
        s_unlockedValid = true;
    }
}

static void on_wheel_new_save(ModContext*, uint32_t, void*) {
    s_unlockedValid = false;
    s_verifyPending = false;
}

static void on_wheel_save_loaded(ModContext* ctx, uint32_t, void*) {
    s_unlockedValid = false;
    s_verifyPending = false;
    if (s_wheelSaveSvc == nullptr) return;
    ItemWheelBlob blob{};
    size_t size = sizeof(blob);
    if (s_wheelSaveSvc->get_blob(ctx, kItemWheelBlobName, &blob, &size) == MOD_OK &&
        size == sizeof(blob)) {
        s_unlocked = blob;
        s_unlockedValid = true;
        s_verifyPending = true;
    }
}

DEFINE_HOOK(&dSv_player_item_c::setLineUpItem, SvSetLineUpItemQuickAccessHook);

static void on_set_line_up_item_quick_access_post(ModContext*, void* args, void*, void*) {
    dSv_player_item_c* item = mods::arg<dSv_player_item_c*>(args, 0);
    if (item == nullptr || item != &itemwheel_inventory() || !itemwheel_filter_active()) {
        return;
    }
    u8 desired[MAX_ITEM_SLOTS];
    itemwheel_build_desired(*item, desired);
    std::memcpy(item->mItemSlots, desired, MAX_ITEM_SLOTS);
    s_filterApplied = true;
}

void quick_access_itemwheel_refresh() {
    if (itemwheel_ring_open()) {
        return;
    }
    if (itemwheel_filter_active()) {
        itemwheel_apply_filter();
    } else {
        itemwheel_restore_full();
    }
}

static bool s_ringArchiveInUse = false;

void quick_access_itemwheel_sync_ring_archive() {
    if (itemwheel_ring_open()) {
        s_ringArchiveInUse = true;
        return;
    }
    if (s_ringArchiveInUse) {
        s_ringArchiveInUse = false;
        quick_access_radial_reset();
    }
}

ModResult init_quick_access_itemwheel(const HookService* hook_svc, const SaveService* save_svc,
                                      ModContext* ctx, ModError*) {
    s_wheelCtx = ctx;
    s_wheelSaveSvc = save_svc;
    if (hook_svc) {
        mods::hook::add_post<SvSetLineUpItemQuickAccessHook>(hook_svc,
            on_set_line_up_item_quick_access_post);
    }
    if (save_svc != nullptr && ctx != nullptr) {
        save_svc->observe_saves(ctx, on_wheel_new_save, on_wheel_save_loaded, nullptr, nullptr,
                                nullptr);
        on_wheel_save_loaded(ctx, 0, nullptr);
    }
    return MOD_OK;
}

void update_quick_access_itemwheel() {
    quick_access_itemwheel_sync_ring_archive();
    if (itemwheel_ring_open()) {
        return;
    }
    dSv_player_item_c& item = itemwheel_inventory();
    if (itemwheel_filter_active()) {
        itemwheel_apply_filter();
    } else if (s_filterApplied || itemwheel_unlocked_missing(item)) {
        itemwheel_restore_full();
    }
    if (dComIfGp_getPlayer(0) == nullptr) {
        return;
    }
    if (s_verifyPending) {
        s_verifyPending = false;
        itemwheel_verify_unlocked(item);
    }
    itemwheel_record_unlocked(item);
}

void shutdown_quick_access_itemwheel() {
    itemwheel_restore_full();
    s_wheelCtx = nullptr;
    s_wheelSaveSvc = nullptr;
    s_unlockedValid = false;
    s_verifyPending = false;
}
