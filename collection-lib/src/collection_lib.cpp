// dusklight-collection-lib - single translation unit.
//
// The library is deliberately built as ONE unity translation unit: the hook
// entry structs (DEFINE_HOOK) live in the internal headers and must exist
// exactly once per binary, which the original in-mod code guaranteed by
// #including all implementation files from one .cpp. This file keeps that
// exact structure so the proven code compiles unmodified.

#include <collection_lib/collection_lib.hpp>
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"

// Implementation units, in the original dependency order.
#include "collection_common.cpp"
#include "custom_equip.cpp"
#include "collection_page.cpp"
#include "collection_layout.cpp"
#include "collection_nav.cpp"
#include "collection_equip.cpp"

// ---------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------

// The library loads icons (.bti) and models (.arc) from the OWNING MOD's
// res/ directory. The owning mod already imports the ResourceService for
// itself - reuse that import instead of declaring a second one with the
// same service_id (the package validator rejects duplicate imports).
extern const ResourceService* svc_resource;

const ResourceService* cl_get_resource_service() {
    return svc_resource;
}

// ---------------------------------------------------------------------------
// Consumer slot registration
//
// The menu screen-build code re-runs slot registration every time the screen
// is built (idempotent per kind+item). The DATA lives in the consuming mod, so
// instead of hardwired register_custom_* functions the library invokes one
// consumer-provided callback - set it via collectionlib_set_register_callback.
// ---------------------------------------------------------------------------
// Vanilla-wired slots (row/column claims + predicates)
// ---------------------------------------------------------------------------

struct VanillaSlotEntry {
    u8 row, item;
    CollectionVanillaSlotDef def;
};
static VanillaSlotEntry s_vanillaSlots[8] = {};
static int s_vanillaSlotCount = 0;

static int cl_add_vanilla_slot(u8 row, const CollectionVanillaSlotDef& def) {
    if (def.unlocked == nullptr) {
        return -1;
    }
    if (s_vanillaSlotCount >= 8) {
        return -1;
    }
    s_vanillaSlots[s_vanillaSlotCount] = {row, 1, def};
    return s_vanillaSlotCount++;
}

int collectionlib_add_vanilla_slot(u8 row, const CollectionVanillaSlotDef& def) { return cl_add_vanilla_slot(row, def); }

int collectionlib_remove_slot(u8 row, u8 item) {
    // 1) drop vanilla-wired claims at this cell
    for (int i = 0; i < s_vanillaSlotCount;) {
        if (s_vanillaSlots[i].row == row && s_vanillaSlots[i].item == item) {
            for (int j = i; j < s_vanillaSlotCount - 1; j++) s_vanillaSlots[j] = s_vanillaSlots[j + 1];
            s_vanillaSlotCount--;
        } else {
            i++;
        }
    }

    // 2) unregister custom slots parked at this cell
    for (int id = custom_equip_count() - 1; id >= 0; id--) {
        const CustomEquipDef* d = custom_equip_get(id);
        if (d == nullptr) continue;
        const u8 r = d->kind == CE_SWORD ? 1 : d->kind == CE_SHIELD ? 2 : 3;
        if (r == row && d->item == item) custom_equip_remove(id);
    }

    // 3) record + suppress
    cl_remove_cell(row, item);
    collectionlib_request_reload();
    return 0;
}

int collectionlib_clear_all_slots() {
    log_collect_info("clear_all_slots: wiping %d vanilla claims + %d custom slots", s_vanillaSlotCount, custom_equip_count());
    // vanilla-wired claims (ordon shield / clothes / wooden sword cells)
    s_vanillaSlotCount = 0;
    // registered custom slots (swords / shields / tunics)
    custom_equip_reset_registry();
    // hide every vanilla gear cell that is not re-registered afterwards
    cl_set_vanilla_layout_hidden(true);
    // NOTE: no collectionlib_request_reload() here. This runs at the top of
    // the register callback, which itself executes on every screen build - a
    // reload request from inside it recreates the screen every build forever
    // (delete/create loop until the menu heap dies).
    return 0;
}

int cl_vanilla_slot_count() { return s_vanillaSlotCount; }
const CollectionVanillaSlotDef* cl_vanilla_slot_get(int i) { return &s_vanillaSlots[i].def; }
u8 cl_vanilla_slot_row(int i) { return s_vanillaSlots[i].row; }
u8 cl_vanilla_slot_item(int i) { return s_vanillaSlots[i].item; }

bool cl_column_claimed(u8 row, u8 item) {
    for (int i = 0; i < s_vanillaSlotCount; ++i) {
        if (s_vanillaSlots[i].row == row && s_vanillaSlots[i].item == item) return true;
    }
    return false;
}

bool cl_column_occupied(u8 row, u8 item) {
    if (cl_column_claimed(row, item)) return true;
    for (int id = 0; id < custom_equip_count(); ++id) {
        const CustomEquipDef* d = custom_equip_get(id);
        if (d == nullptr) continue;
        const u8 r = d->kind == CE_SWORD ? 1 : d->kind == CE_SHIELD ? 2 : 3;
        if (r == row && d->item == item) return true;
    }
    return false;
}

// Thunks: resolve the vanilla slot at a grid cell and defer to its callbacks.
bool cl_vanilla_slot_unlocked(u8 x, u8 y) {
    for (int i = 0; i < s_vanillaSlotCount; ++i) {
        const SlotCell c = grid_cell(s_vanillaSlots[i].row, s_vanillaSlots[i].item);
        if (c.x == x && c.y == y) return s_vanillaSlots[i].def.unlocked != nullptr && s_vanillaSlots[i].def.unlocked();
    }
    return false;
}

bool cl_vanilla_slot_equipped(u8 x, u8 y) {
    for (int i = 0; i < s_vanillaSlotCount; ++i) {
        const SlotCell c = grid_cell(s_vanillaSlots[i].row, s_vanillaSlots[i].item);
        if (c.x == x && c.y == y) return s_vanillaSlots[i].def.equipped != nullptr && s_vanillaSlots[i].def.equipped();
    }
    return false;
}

// ---------------------------------------------------------------------------

static void (*s_registerSlotsFn)() = nullptr;

void collectionlib_set_register_callback(void (*fn)()) {
    s_registerSlotsFn = fn;
}

void collectionlib_run_slot_registration() {
    s_vanillaSlotCount = 0;   // re-claimed by the consumer's callback
    if (s_registerSlotsFn != nullptr) {
        s_registerSlotsFn();
    }
}

// ---------------------------------------------------------------------------
// Feature switches (see collection_common.hpp)
// ---------------------------------------------------------------------------

static bool (*s_unequipPolicy)() = nullptr;
static bool (*s_keepOrdonShieldPolicy)() = nullptr;
static bool (*s_starterSlotsPolicy)() = nullptr;

void collectionlib_set_unequip_policy(bool (*fn)()) { s_unequipPolicy = fn; }
void collectionlib_set_keep_ordon_shield_policy(bool (*fn)()) { s_keepOrdonShieldPolicy = fn; }

bool cl_unequip_enabled() { return s_unequipPolicy != nullptr && s_unequipPolicy(); }
bool cl_keep_ordon_shield_enabled() { return s_keepOrdonShieldPolicy != nullptr && s_keepOrdonShieldPolicy(); }


// ---------------------------------------------------------------------------
// Slot utilities
// ---------------------------------------------------------------------------

CollectionSlot collectionlib_get_slot(u8 row, u8 item) {
    if (row < 1 || row > 3 || item < 1 || item > 4) return CollectionSlot{};
    return CollectionSlot{row, item};
}

// Recorded slot moves: replayed at the end of every apply_collect_shifts, so
// they survive screen rebuilds (the game rebuilds all panes each open) and
// widescreen relayouts. Bounded, no heap.
struct SlotMoveOp {
    u8 row, fromItem, toItem;
    bool active;
};
static SlotMoveOp s_slotMoves[16] = {};

void collectionlib_reset_layout() {
    for (int i = 0; i < 16; ++i) s_slotMoves[i].active = false;
}

SlotMoveOp* cl_slot_moves() { return s_slotMoves; }

bool collectionlib_move_slot(CollectionSlot from, u8 newItem) {
    if (from.row == 0 || from.item == 0 || newItem < 1 || newItem > 4) return false;
    if (newItem == from.item) return true;
    for (int i = 0; i < 16; ++i) {
        if (s_slotMoves[i].active && s_slotMoves[i].row == from.row &&
            s_slotMoves[i].fromItem == from.item) {
            s_slotMoves[i].toItem = newItem;   // update an existing op
            return true;
        }
    }
    for (int i = 0; i < 16; ++i) {
        if (!s_slotMoves[i].active) {
            s_slotMoves[i] = {from.row, from.item, newItem, true};
            return true;
        }
    }
    return false;
}

int collectionlib_add_sword_slot(u8 item, const CustomEquipDef& def) {
    CustomEquipDef d = def;
    d.kind = CE_SWORD;
    d.item = item;
    return custom_equip_register(d);
}

int collectionlib_add_shield_slot(u8 item, const CustomEquipDef& def) {
    CustomEquipDef d = def;
    d.kind = CE_SHIELD;
    d.item = item;
    return custom_equip_register(d);
}

int collectionlib_add_tunic_slot(u8 item, const CustomEquipDef& def) {
    CustomEquipDef d = def;
    d.kind = CE_TUNIC;
    d.item = item;
    return custom_equip_register(d);
}

// First column of `row` AFTER the hardcoded vanilla prefix (ordon sword /
// master sword always occupy the visual slots the base layout hardcodes, so
// custom slots append behind them). row 1/2 prefix = 3 columns, row 3 = 4.
// Returns 0 when the row is full.
static u8 cl_next_free_column(u8 row) {
    // blank layout: vanilla columns are gone - custom slots start flush left
    const u8 first = cl_vanilla_layout_hidden() ? 1 : ((row == 3) ? 5 : 4);
    for (u8 col = first; col <= 12; ++col) {
        if (!cl_item_exists(row, col)) return col;
    }
    return 0;
}

int collectionlib_add_next_sword_slot(const CustomEquipDef& def) {
    const u8 col = cl_next_free_column(1);
    const int id = (col != 0) ? collectionlib_add_sword_slot(col, def) : -1;
    return id;
}

int collectionlib_add_next_shield_slot(const CustomEquipDef& def) {
    const u8 col = cl_next_free_column(2);
    const int id = (col != 0) ? collectionlib_add_shield_slot(col, def) : -1;
    return id;
}

int collectionlib_add_next_tunic_slot(const CustomEquipDef& def) {
    const u8 col = cl_next_free_column(3);
    const int id = (col != 0) ? collectionlib_add_tunic_slot(col, def) : -1;
    return id;
}



CollectionSlotRef collectionlib_get_slot_ref(u8 row, u8 item) {
    return CollectionSlotRef{row, item};
}

bool CollectionSlotRef::remove() const {
    if (row == 0 || item == 0) return false;
    collectionlib_remove_slot(row, item);
    return true;
}

bool CollectionSlotRef::move(u8 newRow, u8 newItem) const {
    if (row == 0 || item == 0) return false;
    if (newRow != row) return false;   // rows are independent layouts for now
    return collectionlib_move_slot(CollectionSlot{row, item}, newItem);
}

bool CollectionSlotRef::exists() const {
    return cl_item_exists(row, item);
}

int CollectionSlotRef::replace(const CustomEquipDef& def) const {
    if (row == 0 || item == 0) return -1;
    return collectionlib_add_slot_override(row, item, def);
}

int collectionlib_add_slot_override(u8 row, u8 item, const CustomEquipDef& def) {
    if (row < 1 || row > 3 || item == 0) return -1;
    // An override occupies the cell - it must not stay suppressed by an
    // earlier (or stray) .remove() at the same position.
    cl_clear_removed_cell(row, item);
    CustomEquipDef d = def;
    d.kind = (row == 1) ? CE_SWORD : (row == 2) ? CE_SHIELD : CE_TUNIC;
    d.item = item;
    return custom_equip_register(d);
}


Slot get_slot(u8 row, u8 item) {
    return Slot{row, item};
}
void collectionlib_request_reload() {
    s_needReloadCollect = true;
}

// ---------------------------------------------------------------------------
// System heap headroom
//
// TP's m_Do_machine.cpp carves zeldaHeap out of systemHeap and leaves
// systemHeap with only 0x10000 (64 KB) free. On PC Dusklight, read_anm_resource
// (status window) and ARAM decompression buffers allocate from
// JKRAllocFromSysHeap - when archives or background models load, systemHeap is
// easily exhausted, which is an instant fatal crash (JKRExpHeap::do_alloc
// failure / OSPanic) when opening the pause collection menu. Expand it once
// with a generous 32 MB heap carved from rootHeap (~230 MB free).
// ---------------------------------------------------------------------------

static void ensure_collection_heap_capacity() {
    static bool s_done = false;
    if (s_done) return;

    JKRHeap* sysHeap = JKRHeap::getSystemHeap();
    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    if (sysHeap == nullptr || rootHeap == nullptr) {
        return;
    }

    if (sysHeap->getFreeSize() < 4 * 1024 * 1024) {
        u32 targetSize = 32 * 1024 * 1024;
        u32 rootFree = rootHeap->getFreeSize();
        if (rootFree < targetSize + 2 * 1024 * 1024) {
            targetSize = (rootFree > 4 * 1024 * 1024) ? (rootFree - 2 * 1024 * 1024) : 0;
        }

        if (targetSize >= 4 * 1024 * 1024) {
            JKRExpHeap* newSysHeap = JKRExpHeap::create(targetSize, rootHeap, false);
            if (newSysHeap != nullptr) {
                newSysHeap->setName("ExpandedSysHeap");
                JKRHeap::setSystemHeap(newSysHeap);
                s_done = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HUD B-button icon: while a custom sword is equipped, the game keeps showing
// the backing vanilla sword's icon (drawButtonB derives it from
// dComIfGs_getSelectEquipSword). Override the texture right after the game
// refreshes it.
// ---------------------------------------------------------------------------

DEFINE_HOOK(&dMeter2Draw_c::changeTextureItemB, CollectionLibItemBTextureHook);

static void cl_item_b_texture_post(ModContext*, void* args, void*, void*) {
    if (!custom_equip_active(CE_SWORD)) return;
    const int id = custom_equip_active_id(CE_SWORD);
    if (id < 0) return;
    ResTIMG* icon = custom_equip_icon(id);
    if (icon == nullptr) return;

    dMeter2Draw_c* draw = args ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;
    if (draw == nullptr) return;

    if (draw->mpItemB != nullptr && draw->mpItemB->getPanePtr() != nullptr) {
        static_cast<J2DPicture*>(draw->mpItemB->getPanePtr())->changeTexture(icon, 0);
    }
    if (draw->mpItemBPane != nullptr) {
        draw->mpItemBPane->hide();
    }
}

ModResult collectionlib_init(const HookService* hook_svc, const LogService* log_svc,
                             const SaveService* save_svc, ModContext* mod_ctx, ModError*) {
    ensure_collection_heap_capacity();
    g_modCtx = mod_ctx;
    g_saveSvc = save_svc;
    g_logSvc = log_svc;

    // Run the consumer's slot registration once (the screen-build code re-runs
    // it on every collection screen build via collectionlib_run_slot_registration),
    // then restore the previously equipped custom gear from the save.
    collectionlib_run_slot_registration();
    custom_equip_restore_from_save();

    if (hook_svc != nullptr) {
        // Area transition & spawn preservation
        mods::hook::add_pre<DaAlinkCreateHook>(hook_svc, on_da_alink_create_pre);
        mods::hook::add_post<DaAlinkCreateHook>(hook_svc, on_da_alink_create_post);
        mods::hook::add_pre<DaAlinkChangeLinkHook>(hook_svc, on_da_alink_change_link_pre);
        mods::hook::add_post<DaAlinkChangeLinkHook>(hook_svc, on_da_alink_change_link_post);
        mods::hook::add_post<DaAlinkChangeWolfHook>(hook_svc, on_da_alink_change_wolf_post);
        mods::hook::add_pre<SetSelectEquipClothesHook>(hook_svc, on_set_select_equip_clothes_pre);
        mods::hook::add_pre<Meter2InfoSetShieldHook>(hook_svc, on_meter2_info_set_shield_pre);
        mods::hook::add_pre<MsgFlowGetCheckHook>(hook_svc, on_msg_flow_get_check_pre);

        // Screen layout & lifecycle
        mods::hook::add_post<MenuCollect2DCreateHook>(hook_svc, on_menu_collect_2d_create_post);
        mods::hook::add_pre<MenuCollect2DDeleteHook>(hook_svc, on_menu_collect_2d_delete_pre);
        mods::hook::add_pre<ScreenSetHook>(hook_svc, on_screen_set_pre);
        mods::hook::add_post<ScreenSetHook>(hook_svc, on_screen_set_post);
        mods::hook::add_post<MenuCollectWideHook>(hook_svc, on_menu_collect_wide_post);
        mods::hook::add_post<MenuCollect2DMoveHook>(hook_svc, on_menu_collect_2d_move_post);
        mods::hook::add_post<MwExecuteHook>(hook_svc, on_mw_execute_post);

        // Menu navigation & item description strings
        // getItemTag() installs with MOD_ERROR on this build (too small a
        // function for the detour mechanism) - on_get_item_tag_pre's callback
        // below silently never runs as a result. Logged once at startup so a
        // future engine/SDK update that changes this doesn't go unnoticed.
        ModResult r_getItemTag = mods::hook::add_pre<GetItemTagHook>(hook_svc, on_get_item_tag_pre);
        ModResult r_pointerWait = mods::hook::add_pre<PointerWaitHook>(hook_svc, on_pointer_wait_pre);

        // Resolve dusk::menu_pointer's hit_pane/set_hover_target/peek_click
        // addresses directly (symbol lookup only, no detour/patch attempt) so
        // the pointerWait replacement below can call them as plain function
        // pointers - see collection_nav.cpp for why these can't be normal
        // linked C++ calls. Logged once at startup for the same reason as above.
        if (hook_svc->resolve) {
            void* hitPaneAddr = nullptr;
            ModResult r_resolve = hook_svc->resolve(mod_ctx, kHitPaneMangledName, &hitPaneAddr, nullptr);
            if (r_resolve == MOD_OK && hitPaneAddr) {
                g_hitPaneFn = reinterpret_cast<bool (*)(CPaneMgr*, f32)>(hitPaneAddr);
            }

            void* setHoverTargetAddr = nullptr;
            ModResult r_resolve3 = hook_svc->resolve(mod_ctx, kSetHoverTargetMangledName, &setHoverTargetAddr, nullptr);
            if (r_resolve3 == MOD_OK && setHoverTargetAddr) {
                g_setHoverTargetFn = reinterpret_cast<void (*)(u16)>(setHoverTargetAddr);
            }

            void* peekClickAddr = nullptr;
            ModResult r_resolve4 = hook_svc->resolve(mod_ctx, kPeekClickMangledName, &peekClickAddr, nullptr);
            if (r_resolve4 == MOD_OK && peekClickAddr) {
                g_peekClickFn = reinterpret_cast<bool (*)()>(peekClickAddr);
            }
        }
        mods::hook::add_pre<CursorPosSetHook>(hook_svc, on_cursor_pos_set_pre);
        mods::hook::add_pre<CursorMoveHook>(hook_svc, on_cursor_move_pre);
        // getItemTag() can't be hooked on this build (MOD_ERROR above) - vanilla
        // pointerWait() therefore never finds Hylian Shield/Reinforced Shield/
        // Magic Armor. Replace the whole function: run the real original first
        // (unchanged for every other cell), fall back to our own hit_pane() check
        // against those 3 known-good panes only if it found nothing.
        ModResult r_pointerWaitReplace = mods::hook::replace<PointerWaitHook>(hook_svc, on_pointer_wait_replace);
        mods::hook::add_post<PointerWaitHook>(hook_svc, on_pointer_wait_post);
        mods::hook::add_pre<SetItemNameStringHook>(hook_svc, on_set_item_name_string_pre);
        mods::hook::add_pre<GetStringKanjiHook>(hook_svc, on_get_string_kanji_pre);
        mods::hook::add_pre<MsgStringGetStringLocalHook>(hook_svc, on_get_string_local_pre);

        // Equipment actions & frame highlights
        mods::hook::add_pre<WaitProcHook>(hook_svc, on_wait_proc_pre);
        mods::hook::add_post<WaitProcHook>(hook_svc, on_wait_proc_post);
        mods::hook::add_pre<PointerActivateCurrentHook>(hook_svc, on_pointer_activate_current_pre);
        mods::hook::add_pre<ChangeSwordHook>(hook_svc, on_change_sword_pre);
        mods::hook::add_pre<ChangeShieldHook>(hook_svc, on_change_shield_pre);
        mods::hook::add_pre<ChangeClotheHook>(hook_svc, on_change_clothes_pre);
        mods::hook::add_pre<SetEquipFrameColorSwordHook>(hook_svc, on_set_equip_frame_sword_pre);
        mods::hook::add_pre<SetEquipFrameColorShieldHook>(hook_svc, on_set_equip_frame_shield_pre);
        mods::hook::add_pre<SetEquipFrameColorClothesHook>(hook_svc, on_set_equip_frame_clothes_pre);

        // Custom sword/shield/tunic model swap on Link (world + doll).
        custom_equip_init_hooks(hook_svc, g_saveSvc);

        // HUD B-button icon override for equipped custom swords.
        mods::hook::add_post<CollectionLibItemBTextureHook>(hook_svc, cl_item_b_texture_post);
    }
    return MOD_OK;
}

void collectionlib_update() {
    collection_page_update();
    custom_equip_update();
}

void collectionlib_shutdown() {
    collection_page_teardown();
    custom_equip_shutdown();
    slot_registry_clear();   // drops the mod slots' pane pointers
    s_picTunagiKen2 = nullptr;
    s_picTunagiTate2 = nullptr;
    s_picTunagiFuku3 = nullptr;
    s_cachedScreen = nullptr;
    s_capturedScreen = nullptr;
    s_currentCollect2D = nullptr;
}


// ---------------------------------------------------------------------------
// Slot move replay (called from apply_collect_shifts, layout.cpp)
//
// Positions per row (1-based, matching apply_collect_shifts):
//   row 1 (swords):  1 = ken_n0,  2 = registry, 3 = ken_n1, 4 = heart_n
//   row 2 (shields): 1 = tate_n0, 2 = registry, 3 = tate_n1
//   row 3 (clothes): 1 = registry, 2 = fuku_n0, 3 = fuku_n1, 4 = fuku_n2
// "registry" = a SlotSpec placed by addSlot at that row position (its panes
// come from the slot registry). Frames follow the icon containers; vanilla
// frame panes are the matching 'ken_g_0' style tags.
// ---------------------------------------------------------------------------

struct RowPanePair {
    u64 iconTag;   // vanilla icon container pane (0 = registry slot)
    u64 frameTag;
};

static RowPanePair cl_vanilla_pane(u8 row, u8 item) {
    static const RowPanePair kRow1[5] = {{0, 0}, {MULTI_CHAR('ken_n0'), MULTI_CHAR('ken_g_0')}, {0, 0},
                                         {MULTI_CHAR('ken_n1'), MULTI_CHAR('ken_g_1')}, {MULTI_CHAR('heart_n'), 0}};
    static const RowPanePair kRow2[5] = {{0, 0}, {MULTI_CHAR('tate_n0'), MULTI_CHAR('tate_g_0')}, {0, 0},
                                         {MULTI_CHAR('tate_n1'), MULTI_CHAR('tate_g_1')}, {0, 0}};
    static const RowPanePair kRow3[5] = {{0, 0}, {0, 0}, {MULTI_CHAR('fuku_n0'), MULTI_CHAR('fuku_g_0')},
                                         {MULTI_CHAR('fuku_n1'), MULTI_CHAR('fuku_g_1')}, {MULTI_CHAR('fuku_n2'), MULTI_CHAR('fuku_g_2')}};
    if (row < 1 || row > 3 || item < 1 || item > 4) return {0, 0};
    if (row == 1) return kRow1[item];
    if (row == 2) return kRow2[item];
    return kRow3[item];
}

void cl_apply_slot_moves(J2DScreen* screen, f32 baseX, f32 dx) {
    if (screen == nullptr || dx <= 0.0f) return;

    const f32 frameDx = -24.5f;
    for (int i = 0; i < 16; ++i) {
        if (!s_slotMoves[i].active) continue;
        const u8 row = s_slotMoves[i].row;
        const u8 from = s_slotMoves[i].fromItem;
        const u8 to = s_slotMoves[i].toItem;
        if (from == 0 || to == 0 || from == to) continue;

        const f32 toX = baseX + static_cast<f32>(to - 1) * dx;
        const f32 toFrameX = toX + frameDx;
        const f32 rowY = collection_row_icon_y(static_cast<u8>(row - 1));

        // Source panes: registry slot first, then the vanilla pane table.
        const SlotCell srcCell = grid_cell(row, from);
        const SlotSpec* src = slot_at(srcCell.x, srcCell.y);
        J2DPane* icon = nullptr;
        J2DPane* frame = nullptr;
        if (src != nullptr && src->icon != nullptr) {
            icon = src->icon;
            frame = src->frame;
        } else {
            const RowPanePair vp = cl_vanilla_pane(row, from);
            if (vp.iconTag != 0) icon = screen->search(vp.iconTag);
            if (vp.frameTag != 0) frame = screen->search(vp.frameTag);
        }
        if (icon == nullptr) continue;

        set_pane_pos(icon, toX, rowY);
        if (frame != nullptr) set_pane_pos(frame, toFrameX, rowY);

        // Anything parked at the target column (registry slot) shifts aside to
        // the source column so the two swap without overlapping.
        const SlotCell dstCell = grid_cell(row, to);
        const SlotSpec* dst = slot_at(dstCell.x, dstCell.y);
        if (dst != nullptr && dst->icon != nullptr) {
            const f32 fromX = baseX + static_cast<f32>(from - 1) * dx;
            set_pane_pos(dst->icon, fromX, rowY);
            if (dst->frame != nullptr) set_pane_pos(dst->frame, fromX + frameDx, rowY);
        }
    }
}