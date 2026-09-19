#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"



#include "d/d_menu_collect.h"
#include "d/d_menu_window.h"
#include "d/d_select_cursor.h"
#include "d/d_pane_class.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_string_base.h"
#include "d/d_msg_string.h"
#include "d/d_msg_out_font.h"
#include "d/d_meter_HIO.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/d_lib.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_ext.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "mods/svc/hook.hpp"
#include "mods/svc/resource.h"

// --- library features -----------------------------------------------------------
// The "Unequip" action and the keep-ordon-shield behavior are lib features and
// always available. The consumer decides WHICH slots exist and WHERE via
// collectionlib_add_*_slot / collectionlib_move_slot.

// --- vanilla-wired slots -------------------------------------------------------
// The first column of each row hosts a VANILLA item cell (wooden sword, ordon
// shield, ordon clothes): A-press runs the game's own equip path for that cell
// and the pane is the game's own. To make such a cell exist, claim it and
// provide the visibility/equipped predicates - the LIBRARY has no opinion on
// WHEN that is the case, that is entirely the consuming mod's decision.
//
// Claimed columns also control the row layout: a claimed first column makes the
// row use all 4 columns; an unclaimed one lets the row shift one slot left so
// the remaining slots sit flush.
struct CollectionVanillaSlotDef {
    bool (*unlocked)();          // required: owns the cell's visibility/selectability
    bool (*equipped)();          // optional: nullptr = vanilla per-cell equipped check

    // Only used for cells WITHOUT an existing vanilla pane (row 3, column 1):
    const char* name = nullptr;          // literal label, or
    u16 nameMsgId = 0;                   // game message id (used when name == nullptr)
    const char* description = nullptr;
    u16 descMsgId = 0;
    ResTIMG* icon = nullptr;             // icon texture (required for built cells)
};

// --- blank-layout mode ---------------------------------------------------------
// When set, every vanilla collection cell is hidden and only registered mod slots
// (plus re-registered vanilla-wired cells) decide what exists. Used by
// collectionlib_clear_all_slots(); pass false to bring the vanilla cells back.
void cl_set_vanilla_layout_hidden(bool hidden);
bool cl_vanilla_layout_hidden();

// Hide every vanilla gear pane + clear the vanilla cursor flags. Called on
// screen build AND after the game's per-frame waitProc update (the game would
// otherwise re-show the panes it considers unlocked).*/
void cl_apply_blank_layout(struct dMenu_Collect2D_c* collect2D);

// --- gezieltes Entfernen einzelner Zellen -------------------------------------
// Eine Zelle (Reihe, Spalte - 1-basiert) dauerhaft entfernen: die Vanilla-Panes
// der Zelle werden jeden Frame versteckt + vom Screen geparkt und die Cursor-
// Flags genullt. Placement ueberspringt entfernte Zellen nicht - nachfolgende
// Slots ruecken nach links nach.
void cl_remove_cell(u8 row, u8 item);
bool cl_cell_removed(u8 row, u8 item);


// Query: is this human {row, item} cell currently removed/suppressed?
bool cl_cell_removed(u8 row, u8 item);
void cl_add_removed_cell(u8 row, u8 item);
// Undo a previous cl_remove_cell / CollectionSlotRef::remove() at this
// position. Used by slot overrides, which occupy the cell instead of leaving
// it suppressed - without this, a stale removal recorded earlier (or by
// mistake) keeps zeroing the override's cursor/selectable flags every frame.
void cl_clear_removed_cell(u8 row, u8 item);

int collectionlib_add_vanilla_slot(u8 row, const CollectionVanillaSlotDef& def);

// --- item-2/item-3 column swap (sword/shield rows only) -----------------------
// Item 3's native pane (Master Sword / Hylian Shield - the always-on cell with
// no claim hook, unlike item 1) has its own HARDCODED visual column (always the
// row's 3rd), completely independent of the {row,item} placement system. When
// swapped, that native pane renders in item 2's column instead (its own unlock
// condition, identity and cursor hit-box are untouched - only its on-screen
// position moves), and item 2's own native pane (Ordon Sword / Wooden Shield)
// is force-hidden so nothing else shares that column. Register a custom
// override at item 3 (its column is now free) for whatever should occupy the
// row's 3rd visual slot instead - see collectionlib_add_slot_override /
// Slot::replace. Call before the collection screen is first built.
void cl_set_item23_swapped(u8 row, bool swapped);
bool cl_item23_swapped(u8 row);

// Is the column claimed by a VANILLA-WIRED slot? Gates the vanilla cell's
// unlock - a custom slot filling the column must NOT unlock the vanilla item.
bool cl_column_claimed(u8 row, u8 item);
// Is the column occupied by anything (vanilla-wired claim or custom slot)?
// Drives the row-shift decision: occupied column 1 -> the row keeps all
// columns; free column 1 -> the row shifts one slot left.
bool cl_column_occupied(u8 row, u8 item);
// Does the row/column hold anything (vanilla item, vanilla-wired claim or
// registered custom slot)? Backs the auto-placement search.
bool cl_item_exists(u8 row, u8 item);
// Static thunks the layout code installs as SlotSpec unlock/equipped fns for
// built vanilla slots (resolve the def by grid cell).
bool cl_vanilla_slot_unlocked(u8 x, u8 y);
bool cl_vanilla_slot_equipped(u8 x, u8 y);
int cl_vanilla_slot_count();
const CollectionVanillaSlotDef* cl_vanilla_slot_get(int index);   // index + row/item below
u8 cl_vanilla_slot_row(int index);
u8 cl_vanilla_slot_item(int index);

// Consumer-provided slot registration (set via collection_lib.hpp's
// collectionlib_set_register_callback); invoked by the screen-build code.
void collectionlib_run_slot_registration();

// --- internal (used by the layout code) ---
bool cl_unequip_enabled();
bool cl_keep_ordon_shield_enabled();
// Re-apply the consumer's recorded slot moves onto the freshly built screen.
// Called from apply_collect_shifts with the row layout in scope.
void cl_apply_slot_moves(J2DScreen* screen, f32 baseX, f32 dx);

// --- slot utilities -----------------------------------------------------------
// Human slot position inside a row: 1-based {row, item}, item counts the row's
// columns left-to-right (rows are up to 4 wide). row 1 = swords, 2 = shields,
// 3 = tunics.
struct CollectionSlot { u8 row = 0, item = 0; };   // {0,0} = invalid

// Resolve a slot position to a handle. Returns {0,0} if the position is out of
// range. The handle stays valid across screen rebuilds; panes are looked up
// fresh per build.
CollectionSlot collectionlib_get_slot(u8 row, u8 item);

// Move a slot to another column of the SAME row (rows cannot be crossed - the
// three equip rows are independent). Visually translates the slot's panes; the
// move is recorded and re-applied on every collection screen build, so it only
// needs to be done once at startup. Fails (returns false, no op recorded) if
// the source position has no slot or the target column is out of range.
bool collectionlib_move_slot(CollectionSlot from, u8 newItem);

// Drop every recorded slot move (the layout returns to its default state on
// the next screen build).
void collectionlib_reset_layout();

// Force the collection screen to rebuild on next open (call after changing
// switches, moves or registered slots).
void collectionlib_request_reload();

extern const ResourceService* cl_get_resource_service();

// Globals & Module Context
struct SaveService;
extern ModContext* g_modCtx;
extern const LogService* g_logSvc;
extern const SaveService* g_saveSvc;
extern J2DScreen* s_cachedScreen;
extern J2DScreen* s_capturedScreen;
extern dMenu_Collect2D_c* s_currentCollect2D;
extern bool s_needReloadCollect;

void log_collect_info(const char* fmt, ...);

// Connectors (tunagi)
extern J2DPicture* s_picTunagiKen2;
extern J2DPicture* s_picTunagiTate2;
extern J2DPicture* s_picTunagiFuku3;

// Dynamic connectors between adjacent custom slots (up to 6 pairs)
extern J2DPicture* s_customConnectors[6];
extern int         s_customConnectorCount;
extern J2DPane*    s_customConnectorParent[3];
extern J2DPicture* s_customConnectorTemplate[3];

// Custom equipment kind (swords / shields / tunics) and active check
enum CustomEquipKind : u8 { CE_SWORD = 0, CE_SHIELD = 1, CE_TUNIC = 2 };
bool custom_equip_active(CustomEquipKind kind);

// One text slot on a grid cell: either a literal string (used verbatim) or a
// game message id (the game's own string for that id is used). Written in the
// brace-initializer as just `"My label"` or just `0x18d`.
struct SlotText {
    const char* str = nullptr;
    u16 msgID = 0;
    SlotText() = default;
    SlotText(const char* s) : str(s) {}
    SlotText(u32 id) : msgID(static_cast<u16>(id)) {}
};

// Called when the player presses A on a slot that carries one. A slot with an
// onEquip is fully mod-driven: the vanilla changeSword/Shield/Clothe path is
// skipped for it. nullptr == use the vanilla equip path for this cell.
using SlotEquipFn = void (*)(dMenu_Collect2D_c* collect2D);

// Optional per-slot unlock test. When set, it OWNS whether the slot's icon shows
// and whether the cell is a cursor target - is_collect_item_unlocked defers to it
// instead of the hardcoded per-cell logic (needed for auto-layout slots that get
// parked on an arbitrary free grid cell). nullptr == use the per-cell logic.
// (x,y) is the resolved grid cell, so one generic fn can serve many slots.
using SlotUnlockFn = bool (*)(u8 x, u8 y);

// A resolved internal grid cell; {0xFF, 0xFF} == "none".
struct SlotCell { u8 x = 0xFF, y = 0xFF; };
inline bool slot_cell_set(SlotCell c) { return c.x != 0xFF; }

// A human 1-based {row, item} position; {0, 0} == "unset".
struct GridPos { u8 row = 0, item = 0; };
inline bool grid_pos_set(GridPos p) { return p.row != 0; }

// Opt-in "just works" layout for a mod slot: when `on`, generic loops in
// collection_layout / collection_nav position it, show/hide it by unlock state,
// make it selectable, and route the cursor to/from it - no hand-written code.
struct SlotAuto {
    bool on = false;
    f32  posX = 0.0f, posY = 0.0f;   // icon-container translate (frame derives from it)
    GridPos navLeft, navRight, navUp, navDown;   // human {row,item} nav targets; unset == generic
};

// A mod-added collection-grid slot. Everything about one slot is passed to
// addSlot() in a single brace-initializer; addSlot builds the panes and records
// the spec in the registry below, so the rest of the menu code (nav, layout,
// string hooks) never hardcodes a tag or a string - it queries the registry.
//
//   at            human 1-based position: {row, item}. row 1/2/3 = swords /
//                 shields / clothes; item counts left-to-right from 1. addSlot
//                 translates this to the real internal grid cell (see grid_cell)
//                 - an item past the row's width (e.g. {3, 5}) is parked on a
//                 free hidden cell, so place it visually via autoLayout.
//   enabled       false = don't build the panes or register the slot (feature
//                 off); addSlot still clears the out-handles so nothing dangles.
//   iconTag       tag for the new icon container pane   (e.g. 'ken_mid')
//   iconPicTag    tag for the new icon picture          (e.g. 'ken_im')
//   frameTag      tag for the new frame/highlight picture(e.g. 'ken_gm')
//   texOverride   icon texture, or nullptr for the row's default icon
//   name          slot name: a literal string, or a game message id
//   description   slot description: a literal string, or a game message id
//   onEquip       callback for A-press (nullptr == vanilla equip path)
//   autoLayout    set `.on` for a freely-placed slot: generic loops then handle
//                 position / visibility / selectability / navigation - no other
//                 code. The three vanilla mid-slots leave it off and keep their
//                 bespoke, row-shift-entangled handling.
//   outIcon/outIconPic/outFrame  OPTIONAL trailing - external globals to mirror
//                 the created panes into (only the vanilla mid-slots need this;
//                 a modular slot omits them and reads slot->icon / iconPic /
//                 frame from the registry instead).
//
// Everything after `outFrame` is filled in by addSlot - leave it unset.
struct SlotSpec {
    GridPos at;
    bool enabled;
    u64 iconTag, iconPicTag, frameTag;
    ResTIMG* texOverride;
    SlotText name;
    SlotText description;
    SlotEquipFn onEquip;
    SlotAuto autoLayout;
    SlotUnlockFn unlockFn = nullptr;     // OPTIONAL: owns show/selectable when set
    SlotUnlockFn equippedFn = nullptr;   // OPTIONAL: owns the "equipped" ring/string
    J2DPane**    outIcon    = nullptr;
    J2DPicture** outIconPic = nullptr;
    J2DPicture** outFrame   = nullptr;

    u8 x = 0, y = 0;                 // resolved internal grid cell
    J2DPane*    icon    = nullptr;   // the created panes (always populated)
    J2DPicture* iconPic = nullptr;
    J2DPicture* frame   = nullptr;
};

int cl_removed_cell_count();
CollectionSlot cl_removed_cell_at(int i);
// Per Frame: versteckt/parkt alle entfernten Zellen (nach dem Spiel-Update).
void cl_suppress_removed_cells(struct dMenu_Collect2D_c* collect2D);

// Translate a human {row, item} (1-based) to the real internal grid cell.
// Rows 1..3 -> internal y 0..2; items 1..4 -> internal x 3..6. An item beyond a
// row's 4-wide extent (or a cell the vanilla menu owns) is parked on the first
// free hidden cell so it still gets a real cursor slot.
SlotCell grid_cell(u8 row, u8 item);

// Slot registry - filled by addSlot(), queried everywhere else.
void            slot_registry_clear();
void            slot_registry_add(const SlotSpec& s);
int             slot_count();               // registered slots (built this screen)
const SlotSpec* slot_get(int i);            // 0..slot_count()-1
const SlotSpec* slot_at(u8 x, u8 y);        // exact grid cell, nullptr if none
const SlotSpec* slot_in_row(u8 y);          // the mod slot in row y, nullptr if none

// Last texture pushed to slot i's icon picture via changeTexture() - lets
// per-frame layout code skip a redundant changeTexture call (and the GPU
// texture reinit it triggers) when the texture hasn't actually changed.
const ResTIMG* slot_applied_tex(int i);
void           slot_set_applied_tex(int i, const ResTIMG* tex);
const SlotSpec* slot_by_msgid(u32 msgID);   // slot whose name/description id is msgID

// The panes a mod slot built, by internal grid cell (nullptr if no slot / not
// built). These replace the old s_paneKenMid / s_picTateMidFrame / ... globals.
inline J2DPane*    slot_icon(u8 x, u8 y)    { const SlotSpec* s = slot_at(x, y); return s ? s->icon    : nullptr; }
inline J2DPicture* slot_iconPic(u8 x, u8 y) { const SlotSpec* s = slot_at(x, y); return s ? s->iconPic : nullptr; }
inline J2DPicture* slot_frame(u8 x, u8 y)   { const SlotSpec* s = slot_at(x, y); return s ? s->frame   : nullptr; }

// Cursor target for pressing `dir` (0=left 1=right 2=up 3=down) from cell (x,y),
// derived from auto-managed slots' navLeft/navRight/navUp/navDown (both the slot
// itself and the reverse direction from its neighbour). {0xFF,0xFF} == no override.
SlotCell slot_nav_target(u8 x, u8 y, int dir);

// The message ids the game must be handed for a slot's name / description.
// A literal-string SlotText gets a synthetic id (reserved 0xE000+ range) that the
// string hooks intercept; a message-id SlotText passes its id straight through.
u16 slot_name_id(const SlotSpec* s);
u16 slot_desc_id(const SlotSpec* s);

extern ResourceBuffer s_ordonClothesBtiBuf;

// Original pristine translations of vanilla panes (immutable constants from vanilla .blo)
static constexpr f32 s_ken_n0_origX = -34.0f;
static constexpr f32 s_ken_n0_origY = -96.0f;
static constexpr f32 s_ken_g0_origY = -44.0f;
static constexpr f32 s_ken_g1_origY = -44.0f;
static constexpr f32 s_ken_n1_origX = 20.0f;
static constexpr f32 s_tate_n0_origX = -34.0f;
static constexpr f32 s_tate_n0_origY = -39.0f;
static constexpr f32 s_tate_g0_origY = 13.0f;
static constexpr f32 s_tate_g1_origY = 13.0f;
static constexpr f32 s_tate_n1_origX = 20.0f;
static constexpr f32 s_fuku_n0_origX = -34.0f;
static constexpr f32 s_fuku_n0_origY = 18.0f;
static constexpr f32 s_fuku_g0_origY = 70.0f;
static constexpr f32 s_fuku_n1_origX = 20.0f;
static constexpr f32 s_fuku_n2_origX = 74.0f;
static constexpr f32 s_heart_n_origX = 74.0f;
static constexpr f32 s_heart_n_origY = -72.0f;
static constexpr f32 s_kamen_n_origX = 181.0f;
static constexpr f32 s_kamen_n_origY = -28.0f;
static constexpr f32 s_modelbgn_origX = 189.0f;
static constexpr f32 s_modelbgn_origY = -53.0f;
static constexpr f32 s_col_dx = 54.0f;

// X translate for a grid column, `col` counted 0-based from a row's left edge
// (so the vanilla item columns 3..6 are col 0..3 here). Matches the `baseX +
// N*dx` spacing used throughout apply_collect_shifts.
inline f32 collection_slot_x(f32 col) { return s_ken_n0_origX + col * (s_col_dx + 5.0f); }
// Row Y for the icon container / the frame picture (frame sits 52px below).
inline f32 collection_row_icon_y(u8 row) {
    return row == 0 ? s_ken_n0_origY : row == 1 ? s_tate_n0_origY : s_fuku_n0_origY;
}

// Subclass to access J2DPicture internals safely for texture vertex mapping
class CustomPicture : public J2DPicture {
public:
    void copyVisualsFrom(const J2DPicture* src) {
        if (!src) return;
        const CustomPicture* s = static_cast<const CustomPicture*>(src);
        mKind = s->mKind;
        field_0x109 = s->field_0x109;
        for (int i = 0; i < 4; i++) {
            field_0x10a[i] = s->field_0x10a[i];
            mCornerColor[i] = s->mCornerColor[i];
        }
        mBlack = s->mBlack;
        mWhite = s->mWhite;
        mBlendKonstColor = s->mBlendKonstColor;
        mBlendKonstAlpha = s->mBlendKonstAlpha;
    }
};

ResTIMG* get_ordon_clothes_texture();
ResTIMG* get_ordon_hero_texture();
ResTIMG* get_reinforced_shield_texture();
const ResTIMG* safe_get_tex_info(J2DPane* pane);
void set_pane_pos(J2DPane* pane, f32 x, f32 y);
Vec get_pane_center(J2DPane* pane);
void safe_delete_custom_pane(J2DPane*& pane);

inline bool is_collection_menu_enabled() {
    return true;   // the library is always active; consumers gate features via their policies
}

// Ordon Shield is starter gear like the wooden sword / ordon clothes: it's only
// in the collection with the "Add wooden sword and ordon clothes" option on.
// ("Keep Ordon Shield in collection" is a sub-option of that: when set, the
// shield is never lost for good - the burn still un-equips it, but the owned bit
// is kept (on_meter2_info_set_shield_pre) and re-granted if it was already gone
// when the option was enabled (keep_ordon_shield_tick). Trade-off: shops then
// won't sell a replacement Wooden Shield, since the game still sees you owning
// one - fine, since the whole point is that you keep it.)
inline bool ordon_shield_slot_present() {
    return cl_column_claimed(2, 1);
}

inline bool is_collect_item_unlocked(u8 x, u8 y) {
    // A modular slot with its own unlock test owns the answer (its cell may be an
    // arbitrary parked free cell the per-cell logic below knows nothing about).
    if (const SlotSpec* modSlot = slot_at(x, y)) {
        if (modSlot->unlockFn) return modSlot->unlockFn(x, y);
    }
    // Blank layout: mod slots own everything, vanilla cells stay hidden.
    if (cl_vanilla_layout_hidden()) return false;
    if (y == 0) {
        u8 eqSword = custom_equip_active(CE_SWORD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipSword();
        if (x == 3) {
            // Wooden sword slot only exists when the consumer claims the column
            // (and its unlocked() gate has already approved at the top of this
            // function via the registry check).
            if (!cl_column_claimed(1, 1)) return false;
            return dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e) ||
                   (eqSword == dItemNo_WOOD_STICK_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_SWORD_e) ||
                   (eqSword == dItemNo_SWORD_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) ||
                   (eqSword == dItemNo_MASTER_SWORD_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ||
                   (eqSword == dItemNo_LIGHT_SWORD_e);
        }
        if (x == 4) {
            return dComIfGs_isItemFirstBit(dItemNo_SWORD_e) ||
                   (eqSword == dItemNo_SWORD_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) ||
                   (eqSword == dItemNo_MASTER_SWORD_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ||
                   (eqSword == dItemNo_LIGHT_SWORD_e);
        }
        if (x == 5) {
            return dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ||
                   (eqSword == dItemNo_MASTER_SWORD_e) ||
                   (eqSword == dItemNo_LIGHT_SWORD_e);
        }
        if (x == 6) return true; // Heart Container
    } else if (y == 1) {
        u8 eqShield = custom_equip_active(CE_SHIELD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipShield();
        if (x == 3) {
            if (!ordon_shield_slot_present()) return false;
            bool hasEverHadOrdonShield = dComIfGs_isCollectShield(0) ||
                                         dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) ||
                                         (eqShield == dItemNo_WOOD_SHIELD_e);
            if (cl_keep_ordon_shield_enabled()) {
                return hasEverHadOrdonShield;
            }
            return dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) ||
                   (eqShield == dItemNo_WOOD_SHIELD_e);
        }
        if (x == 4) {
            return dComIfGs_isItemFirstBit(dItemNo_SHIELD_e) ||
                   (eqShield == dItemNo_SHIELD_e);
        }
        if (x == 5) {
            return dComIfGs_isItemFirstBit(dItemNo_HYLIA_SHIELD_e) ||
                   (eqShield == dItemNo_HYLIA_SHIELD_e);
        }
        // (Ordon Hero and any other modular shield-row slot handle their own
        //  unlock via SlotSpec::unlockFn, checked at the top of this function.)
    } else if (y == 2) {
        if (x == 3) return cl_column_claimed(3, 1); // Ordon Clothes (only when the consumer claims the column)
        if (x == 4) {
            return dComIfGs_isItemFirstBit(dItemNo_WEAR_KOKIRI_e) ||
                   (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_KOKIRI_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e) ||
                   (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e) ||
                   dComIfGs_isItemFirstBit(dItemNo_ARMOR_e) ||
                   (dComIfGs_getSelectEquipClothes() == dItemNo_ARMOR_e);
        }
        if (x == 5) return dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e) || (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e);
        if (x == 6) return dComIfGs_isItemFirstBit(dItemNo_ARMOR_e) || (dComIfGs_getSelectEquipClothes() == dItemNo_ARMOR_e);
    }
    return false;
}

inline bool is_collect_item_equipped(u8 x, u8 y) {
    if (const SlotSpec* modSlot = slot_at(x, y)) {
        if (modSlot->equippedFn) return modSlot->equippedFn(x, y);
    }
    if (y == 0) {
        if (custom_equip_active(CE_SWORD)) return false;
        if (x == 3) return dComIfGs_getSelectEquipSword() == dItemNo_WOOD_STICK_e;
        if (x == 4) {
            // Relocated native Master Sword pane when the row is item2/3-
            // swapped (cl_item23_swapped) - see collection_common.hpp.
            if (cl_item23_swapped(1)) {
                u8 sword = dComIfGs_getSelectEquipSword();
                return sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e;
            }
            return dComIfGs_getSelectEquipSword() == dItemNo_SWORD_e;
        }
        if (x == 5) {
            u8 sword = dComIfGs_getSelectEquipSword();
            return sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e;
        }
    } else if (y == 1) {
        if (custom_equip_active(CE_SHIELD)) return false;
        if (x == 3) return dComIfGs_getSelectEquipShield() == dItemNo_WOOD_SHIELD_e;
        if (x == 4) return dComIfGs_getSelectEquipShield() == dItemNo_SHIELD_e;
        if (x == 5) return dComIfGs_getSelectEquipShield() == dItemNo_HYLIA_SHIELD_e;
    } else if (y == 2) {
        if (x == 3) return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e;
        if (x == 4) return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_KOKIRI_e;
        if (x == 5) return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_ZORA_e;
        if (x == 6) return dComIfGs_getSelectEquipClothes() == dItemNo_ARMOR_e;
    }
    return false;
}
