#pragma once

#include "quick_access.hpp"
#include "quick_access_itemwheel.hpp"

#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/JUTTexture.h"

enum {
    QA_QUICK_SLOTS = 4,
    QA_ITEM_NONE = 0xFF,
};

enum RadialSlot {
    SLOT_UP = 0,
    SLOT_DOWN = 1,
    SLOT_LEFT = 2,
    SLOT_RIGHT = 3,
    SLOT_COUNT = 4,
    SLOT_NONE = -1,
};

enum QuickAccessAppearance {
    QA_APPEARANCE_RADIAL = 0,
    QA_APPEARANCE_STRIP = 1,
};

static const int QA_TAP_FRAMES = 8;
static const f32 QA_STRIP_BOX_HALF = 20.0f;
static const f32 QA_STRIP_BOX_SPACING = 56.0f;
static const f32 QA_STRIP_BAR_Y = 60.0f;

extern bool s_menuOpen;
extern bool s_editMode;
extern int s_selectedSlot;
extern f32 s_menuAlpha;
extern f32 s_glowTimer;

extern int g_qaSelectOverrideDepth[4];

struct QaSelectSlotScope {
    explicit QaSelectSlotScope(int index) : mIndex(index) { ++g_qaSelectOverrideDepth[mIndex]; }
    ~QaSelectSlotScope() { --g_qaSelectOverrideDepth[mIndex]; }
    QaSelectSlotScope(const QaSelectSlotScope&) = delete;
    QaSelectSlotScope& operator=(const QaSelectSlotScope&) = delete;
    int mIndex;
};

int qa_custom_count();
u8 qa_custom_item(int idx);
bool qa_custom_contains(u8 itemNo);
bool qa_custom_contains_family(u8 itemNo);
u8 qa_custom_find_family_item(u8 itemNo);
bool qa_bottles_menu_owns_item(u8 itemNo);
bool qa_custom_add_item(u8 itemNo);
bool qa_custom_remove_item(u8 itemNo);
bool qa_custom_replace_at(int idx, u8 itemNo);
u8 qa_strip_assigned_item();
void qa_custom_store();

bool qa_is_item_available(u8 itemNo);
int qa_get_active_items(u8 outItems[QA_QUICK_SLOTS]);
void qa_execute_item(u8 itemNo);
bool qa_is_lantern_active();
bool qa_load_boots_worn();
bool qa_is_rod_item(u8 itemNo);
bool qa_get_item_icon(u8 itemNo, J2DPicture** outPic, ResTIMG** outImg, J2DPicture** outPic2 = nullptr);
void qa_item_label(u8 itemNo, char* buf, size_t bufSize);

void qa_draw_item_ammo(u8 itemNo, f32 baseX, f32 baseY, f32 iconW, f32 iconH, u8 alpha);
void qa_shutdown_item_ammo();

void qa_draw_solid_ring(f32 cx, f32 cy, f32 innerR, f32 outerR, JUtility::TColor color, int segs = 64);
void qa_draw_solid_disc(f32 cx, f32 cy, f32 radius, JUtility::TColor color, int segs = 48);
void qa_draw_solid_rect(f32 x, f32 y, f32 w, f32 h, JUtility::TColor color);
f32 qa_draw_text(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                 JUtility::TColor top, JUtility::TColor bottom, u8 alpha);
f32 qa_get_text_width(const char* text, f32 charW);
void qa_draw_rounded_rect(f32 x, f32 y, f32 w, f32 h, f32 r, JUtility::TColor c, int segs = 7);
void qa_draw_framed_plate(f32 x, f32 y, f32 w, f32 h, f32 r,
                          JUtility::TColor frame, JUtility::TColor fill);
void qa_draw_top_sheen(f32 x, f32 y, f32 w, f32 h, JUtility::TColor top);

class dSelect_cursor_c;
dSelect_cursor_c* qa_sel_cursor(int idx = 0);
void qa_sel_cursor_destroy();

bool itemwheel_filter_active();

f32 qa_user_hud_scale();
void qa_hud_scale_begin(f32 anchorX, f32 anchorY);
void qa_hud_scale_end();

void quick_access_radial_select(f32 stickX, f32 stickY, f32 stickMag);
void quick_access_radial_draw(f32 centerX, f32 centerY, u8 alpha, f32 glow);
void quick_access_radial_reset();
void qa_radial_draw_wheel(f32 centerX, f32 centerY, u8 alpha, f32 alphaRate, bool spokes = true);

bool qa_tunic_page_active();
bool qa_bottle_page_active();
bool qa_side_page_active();
void qa_page_reset();
void qa_page_anim_tick();
f32 qa_page_anim_offset();
f32 qa_page_anim_alpha();
void qa_page_step(int dir);
bool qa_page_input(interface_of_controller_pad& pad);
bool qa_page_button_rect(int side, f32* left, f32* top, f32* right, f32* bottom);
int qa_side_slot_count();
void qa_side_slot_center(int slot, f32 centerX, f32 centerY, f32* x, f32* y);
f32 qa_side_strip_slot_x(int slot, f32 centerX);
void qa_side_hover(int slot);
void qa_side_pick(int slot);
void qa_side_confirm();
bool qa_side_has_selection();
void qa_side_select(f32 stickX, f32 stickY, f32 stickMag);
void qa_side_strip_cycle(int dir);
void qa_draw_page_buttons(f32 centerX, f32 centerY, u8 alpha);
void qa_draw_page_buttons_strip(f32 centerX, f32 y, u8 alpha);
void quick_access_side_page_draw(f32 centerX, f32 centerY, u8 alpha);
void quick_access_side_page_strip_draw(f32 screenW, f32 screenH, u8 alpha);
void quick_access_tunics_shutdown();
void qa_pointer_hover_item_slot(int slot);
void qa_pointer_pick_item_slot(int slot);
void qa_pointer_close_menu();
void qa_pointer_set_radial_center(f32 centerX, f32 centerY);
void qa_pointer_set_strip_center(f32 centerX);
void qa_pointer_hover_strip_slot(int slot);
void qa_pointer_pick_strip_slot(int slot);
u8 qa_bottle_item(int idx);
bool qa_bottle_owned(int idx);
int qa_bottle_assigned_slot();
void qa_bottle_use(int idx);

bool quick_access_strip_cycle(int dir);
void quick_access_strip_reset_selection();
void quick_access_strip_draw(f32 centerX, f32 centerY, u8 alpha, f32 glow);
void quick_access_strip_reset();
void quick_access_strip_cursor_request(f32 cx, f32 cy);
void quick_access_strip_cursor_present();
void quick_access_strip_cursor_reset();

ResTIMG* qa_copy_texture(const ResTIMG* src);
ResTIMG* qa_extract_pane_texture(J2DPane* pane);
void qa_reset_icon_caches();

void qa_draw_collection_slot(f32 cx, f32 cy, f32 size, u8 alpha, bool selected, bool assigned);

void qa_draw_msg_window(f32 x, f32 y, f32 w, f32 h, f32 alphaRate);
void qa_invalidate_msg_window();

void quick_access_wolf_draw(f32 screenW, f32 screenH, u8 alpha, f32 glow);
void quick_access_wolf_shutdown();
ModContext* qa_mod_ctx();

bool qa_hint_button_ready();
f32 qa_hint_button_width(int idx, f32 h);
void qa_draw_hint_button(int idx, f32 x, f32 y, f32 h, u8 alpha);
void qa_enter_edit_mode();
void qa_strip_tap_action();
void quick_access_edit_enter();
void quick_access_edit_exit();
void quick_access_edit_cycle(int dir);
void quick_access_edit_move(int dx, int dy);
bool quick_access_edit_toggle_current();
void quick_access_edit_clear_slot();
void quick_access_edit_rotate_slot(int dir);
int quick_access_edit_current_slot();
int quick_access_edit_count();
int quick_access_edit_cursor();
u8 quick_access_edit_item(int idx);
void quick_access_edit_draw(f32 screenW, f32 screenH, u8 alpha, f32 glow);
void quick_access_edit_shutdown();
void quick_access_edit_pointer_install(const HookService* hook_svc, ModContext* mod_ctx);
void quick_access_edit_pointer_update();
void qa_pointer_clear_menu_hint();
void qa_pointer_set_menu_hint(f32 left, f32 right, f32 y, f32 anchorX, f32 anchorY);
void qa_edit_pointer_back();
void qa_edit_pointer_close();
