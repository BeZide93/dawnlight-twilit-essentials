#pragma once

// Internal declarations shared by the files of the collection_lib unity translation unit
// (collection_lib.cpp includes every .cpp of the library).

#include <collection_lib/collection_lib.hpp>

#include "mods/svc/hook.hpp"
#include "mods/svc/host.h"
#include "mods/svc/resource.h"
#include "mods/svc/save.h"

#include "d/d_menu_collect.h"
#include "d/d_menu_window.h"
#include "d/d_select_cursor.h"
#include "d/d_pane_class.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_string_base.h"
#include "d/d_msg_string.h"
#include "d/d_msg_out_font.h"
#include "d/d_meter_HIO.h"
#include "d/actor/d_a_alink.h"
#include "d/d_lib.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_ext.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Services / globals (collection_common.cpp, collection_lib.cpp)
// ---------------------------------------------------------------------------

extern ModContext* g_modCtx;
extern const LogService* g_logSvc;
extern const SaveService* g_saveSvc;

const ResourceService* cl_resource_service();
const HostService* cl_host_service();

void log_collect_info(const char* fmt, ...);
void log_collect_warn(const char* fmt, ...);

// Hook installs can fail (the hooking backend cannot relocate every function prologue);
// the library works around missing hooks where it can, so say which ones are missing.
template <class Entry>
void cl_add_pre(const HookService* h, HookPreFn fn, const char* name) {
    if (mods::hook::add_pre<Entry>(h, fn) != MOD_OK) log_collect_warn("collection-lib: hook %s not installed", name);
}
template <class Entry>
void cl_add_post(const HookService* h, HookPostFn fn, const char* name) {
    if (mods::hook::add_post<Entry>(h, fn) != MOD_OK) log_collect_warn("collection-lib: hook %s not installed", name);
}
template <class Entry>
void cl_replace(const HookService* h, HookReplaceFn fn, const char* name) {
    if (mods::hook::replace<Entry>(h, fn) != MOD_OK) log_collect_warn("collection-lib: hook %s not installed", name);
}
template <class Entry>
void cl_add_pre(const HookService* h, HookPreFn fn, const char* name, int32_t priority) {
    HookOptions options = HOOK_OPTIONS_INIT;
    options.priority = priority;
    if (mods::hook::add_pre<Entry>(h, fn, &options) != MOD_OK) log_collect_warn("collection-lib: hook %s not installed", name);
}
template <class Entry>
void cl_add_post(const HookService* h, HookPostFn fn, const char* name, int32_t priority) {
    HookOptions options = HOOK_OPTIONS_INIT;
    options.priority = priority;
    if (mods::hook::add_post<Entry>(h, fn, &options) != MOD_OK) log_collect_warn("collection-lib: hook %s not installed", name);
}
#define CL_HOOK_PRE(entry, fn) cl_add_pre<entry>(hook_svc, fn, #entry)
#define CL_HOOK_POST(entry, fn) cl_add_post<entry>(hook_svc, fn, #entry)
#define CL_HOOK_PRE_PRIO(entry, fn, prio) cl_add_pre<entry>(hook_svc, fn, #entry, prio)
#define CL_HOOK_POST_PRIO(entry, fn, prio) cl_add_post<entry>(hook_svc, fn, #entry, prio)
#define CL_HOOK_REPLACE(entry, fn) cl_replace<entry>(hook_svc, fn, #entry)

constexpr int32_t kClBeforeOtherMods = 100;
constexpr int32_t kClAfterOtherMods = -100;

extern dMenu_Collect2D_c* s_currentCollect2D;
extern bool s_needReloadCollect;

bool cl_unequip_enabled();
bool cl_keep_ordon_shield_enabled();
bool cl_hd_layout_requested();

// ---------------------------------------------------------------------------
// Pane / texture helpers (collection_common.cpp)
// ---------------------------------------------------------------------------

// Pane tag "<p0><p1><p2><p3><a><b>" for panes the library creates.
u64 cl_make_tag(char p0, char p1, char p2, char p3, u8 a, u8 b);

// First texture of a picture pane (or of its first picture child).
const ResTIMG* cl_pane_texture(J2DPane* pane);

void cl_set_pane_pos(J2DPane* pane, f32 x, f32 y);

// Screen-space center of a pane, as the selection cursor wants it.
Vec cl_pane_global_center(J2DPane* pane);

// Icon texture from a .bti in the mod's res/ (iconArc.fileId == 0xFFFF) or from a file of an
// archive (res/ first, then the game's collection archive). The result is a private copy that
// stays valid for the lifetime of the library - game archives free their resources when the
// menu closes, and the B button icon needs the texture outside the menu too.
ResTIMG* cl_load_icon(const char* path, IconArcRef iconArc);
void cl_free_icons();

// User PNG overrides from <config>/texture_replacements (tex_replacements.cpp).
ResTIMG* tex_replacements_apply(const char* res_path, ResTIMG* fallback);

// ---------------------------------------------------------------------------
// Layout model (collection_layout.cpp)
// ---------------------------------------------------------------------------

constexpr int kClRows = 3;
constexpr u16 kClEquipMsg = 0x436;     // A button: "Equip"
constexpr u16 kClUnequipMsg = 0x437;   // A button with the unequip policy (text supplied by the library)
constexpr u16 kClItemNameMsg = 0x165;  // name of vanilla item n: 0x165 + n, description 0x100 higher
constexpr int kClMaxCols = 6;
constexpr u8  kClNoCell = 0xFF;

enum class ClColType : u8 { Empty, Native, Custom };

struct ClColumn {
    ClColType type = ClColType::Empty;
    u8  x = kClNoCell;     // grid cell x in dMenu_Collect2D_c's 7x6 tables (cell y = row index)
    int customId = -1;
};

struct ClNativeCell {
    u8  x;
    u64 iconTag;
    u64 frameTag;
};

// Native cells of the equipment rows, per row index (0..2) and native column (1-based).
int native_col_count(int r);
const ClNativeCell& native_cell(int r, int nativeCol);
// Native column of cell x in row r, 0 if none.
int native_col_of_x(int r, u8 x);

CustomEquipKind row_kind(int r);

// The current layout. Columns are 1..kClMaxCols.
const ClColumn& layout_column(int r, int col);
// Column (1-based) owning grid cell (x, r), 0 if none.
int layout_col_of_cell(int r, u8 x);
// Last used column of a row, 0 for an empty row.
int layout_last_col(int r);
// Is a native item still part of the layout (anywhere in its row)?
bool layout_native_present(int r, u8 nativeX);
// Custom slot id owning cell (x, r), -1 if the cell is not a custom slot.
int layout_custom_at_cell(int r, u8 x);
// Called by custom_equip_remove to keep column ids in sync.
void layout_on_custom_removed(int id);
// Does any layout slot equip this vanilla item (custom slots via baseItem)?
bool layout_uses_base_item(u8 itemNo);
// Is there a slot without a model for this vanilla item (the item in a column of its own)?
bool layout_has_stand_in(u8 itemNo);

void collectionlib_run_slot_registration();

// Synthetic message ids for custom names / descriptions (collection_nav.cpp resolves them).
u16 layout_name_msg(int r, u8 x);
u16 layout_desc_msg(int r, u8 x);
bool layout_msg_lookup(u32 msgId, int* r, u8* x, bool* isDesc);

// ---------------------------------------------------------------------------
// Custom equipment (custom_equip.cpp)
// ---------------------------------------------------------------------------

int  custom_equip_upsert(const CustomEquipDef& def);
void custom_equip_remove(int id);
bool custom_equip_has_model(int id);
bool custom_equip_unlocked(int id);
bool custom_equip_equipped(int id);
// A on the slot. Returns true when the equipment changed (sound and rumble are played).
bool custom_equip_toggle(int id);
ResTIMG* custom_equip_icon(int id);
u8 custom_equip_resolved_base(const CustomEquipDef& def);

void custom_equip_init_hooks(const HookService* hook_svc, const SaveService* save_svc);
void custom_equip_update();
void custom_equip_shutdown();
void custom_equip_before_link_rebuild();
void custom_equip_on_alink_created(daAlink_c* a);
void custom_equip_set_link_model_wolf(bool isWolf);
void custom_equip_menu_doll_begin();
void custom_equip_menu_doll_end();

// ---------------------------------------------------------------------------
// Collection screen (collection_screen.cpp)
// ---------------------------------------------------------------------------

// Is the library managing this Collection screen instance?
bool screen_active(const dMenu_Collect2D_c* c);
// Native heart (5,0) / fused shadow (6,0) cells still selectable on the main grid.
bool screen_heart_on_main();
bool screen_mask_on_main();
// Recolor every equipment frame (native logic first, then custom slots on top).
void screen_refresh_frames(dMenu_Collect2D_c* c);
// Positions, visibility and scale of the equipment rows for this frame.
void screen_apply_layout(dMenu_Collect2D_c* c);
// Show a native cell's name/description (captured before the library changed the tables).
void screen_show_native_name(dMenu_Collect2D_c* c, u8 x, u8 y);
// Every pane of the equipment grid (pages fade and slide them).
int screen_grid_panes(J2DPane** out, int max);

// Equipment row cell under the cursor? Returns the row index, -1 if none.
int screen_equip_row_at(u8 x, u8 y);
// Pane tag of the custom slot in cell (x, r), 0 if there is none.
u64 screen_custom_icon_tag(int r, u8 x);
// Pane of an equipment row cell (what getItemTag() would name), nullptr if none.
J2DPane* screen_cell_pane(u8 x, u8 y);
J2DPicture* screen_cell_frame(u8 x, u8 y);
bool screen_hd_active();

void screen_install_hooks(const HookService* hook_svc);
void screen_shutdown();

// ---------------------------------------------------------------------------
// Equipping (collection_equip.cpp)
// ---------------------------------------------------------------------------

// A pressed on a custom slot cell.
void equip_activate_custom(dMenu_Collect2D_c* c, int id);
// Is the vanilla item of native cell (x, r) worn (the rule its equipped frame follows)?
bool native_cell_equipped(int r, u8 x);

void equip_install_hooks(const HookService* hook_svc);
void nav_install_hooks(const HookService* hook_svc);

constexpr u64 kClHdRootTag = MULTI_CHAR('hd_colly');
constexpr f32 kClHdIconSize = 46.0f;
constexpr f32 kClHdFrameSize = kClHdIconSize + 6.0f;
constexpr f32 kClHdSideScale = 0.85f;
constexpr f32 kClHdHeartSize = 112.0f * kClHdSideScale;
constexpr f32 kClHdMaskSize = 80.0f * kClHdSideScale;

struct ClHdPos {
    f32 x = 0.0f, y = 0.0f;
};

void hd_place(J2DPane* pane, f32 x, f32 y, f32 w, f32 h);
bool hd_row_native(int r);
ClHdPos hd_column_pos(int r, int col);
ClHdPos hd_heart_pos();
ClHdPos hd_mask_pos();
int hd_frame_index(u8 x, u8 y);
u64 hd_native_flourish_tag(int frameIndex, int corner);
void hd_place_flourishes(J2DPane* topLeft, J2DPane* bottomRight, ClHdPos cell);
J2DPicture* hd_new_flourish(J2DPane* root, u64 tag, bool bottomRight);
extern const JUtility::TColor kClHdFrameOn;
extern const JUtility::TColor kClHdFrameOff;
bool hd_nav_target(dMenu_Collect2D_c* c, int dir, u8* x, u8* y);
void hd_install_hooks(const HookService* hook_svc);

// ---------------------------------------------------------------------------
// Pages (collection_page.cpp)
// ---------------------------------------------------------------------------

void collection_page_reset();
void collection_page_teardown();
void collection_page_update();
void collection_page_handle_input(dMenu_Collect2D_c*);
bool collection_page_active();
bool collection_page_p2_focused();
bool collection_page_on_page();
f32  collection_page_grid_dx();
void collection_page_apply(dMenu_Collect2D_c*);
bool collection_page_claims_cell(u8 x, u8 y);
void collection_page_sync_screen(J2DScreen* screen);
// Walking up from the item rows while a page is shown selects the page's first element.
bool collection_page_focus_first(dMenu_Collect2D_c*);

// ---------------------------------------------------------------------------
// Hook targets used by more than one file
// ---------------------------------------------------------------------------

DEFINE_HOOK(&dMw_c::_execute, MwExecuteHook);
