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

struct CollectionVanillaSlotDef {
    bool (*unlocked)();
    bool (*equipped)() = nullptr;

    // Only used for cells without an existing vanilla pane
    const char* name = nullptr;
    u16 nameMsgId = 0;
    const char* description = nullptr;
    u16 descMsgId = 0;
    ResTIMG* icon = nullptr;
};

// Blank-layout mode: hides every vanilla cell, i thinks its actually buggy somehow?
void cl_set_vanilla_layout_hidden(bool hidden);
bool cl_vanilla_layout_hidden();
void cl_apply_blank_layout(struct dMenu_Collect2D_c* collect2D);

void cl_remove_cell(u8 row, u8 item);
bool cl_cell_removed(u8 row, u8 item);
void cl_add_removed_cell(u8 row, u8 item);
void cl_clear_removed_cell(u8 row, u8 item);

int collectionlib_add_vanilla_slot(u8 row, const CollectionVanillaSlotDef& def);

void cl_set_item23_swapped(u8 row, bool swapped);
bool cl_item23_swapped(u8 row);

bool cl_column_claimed(u8 row, u8 item);
bool cl_column_occupied(u8 row, u8 item);
bool cl_item_exists(u8 row, u8 item);
bool cl_vanilla_slot_unlocked(u8 x, u8 y);
bool cl_vanilla_slot_equipped(u8 x, u8 y);
int cl_vanilla_slot_count();
const CollectionVanillaSlotDef* cl_vanilla_slot_get(int index);
u8 cl_vanilla_slot_row(int index);
u8 cl_vanilla_slot_item(int index);

void collectionlib_run_slot_registration();

void collectionlib_set_unequip_policy(bool (*fn)());
void collectionlib_set_keep_ordon_shield_policy(bool (*fn)());
bool cl_unequip_enabled();
bool cl_keep_ordon_shield_enabled();
void cl_apply_slot_moves(J2DScreen* screen, f32 baseX, f32 dx);

struct CollectionSlot { u8 row = 0, item = 0; };

CollectionSlot collectionlib_get_slot(u8 row, u8 item);
bool collectionlib_move_slot(CollectionSlot from, u8 newItem);
void collectionlib_reset_layout();
void collectionlib_request_reload();

extern const ResourceService* cl_get_resource_service();

struct SaveService;
extern ModContext* g_modCtx;
extern const LogService* g_logSvc;
extern const SaveService* g_saveSvc;
extern J2DScreen* s_cachedScreen;
extern J2DScreen* s_capturedScreen;
extern dMenu_Collect2D_c* s_currentCollect2D;
extern bool s_needReloadCollect;

void log_collect_info(const char* fmt, ...);

extern J2DPicture* s_picTunagiKen2;
extern J2DPicture* s_picTunagiTate2;
extern J2DPicture* s_picTunagiFuku3;

extern J2DPicture* s_customConnectors[6];
extern int         s_customConnectorCount;
extern J2DPane*    s_customConnectorParent[3];
extern J2DPicture* s_customConnectorTemplate[3];

enum CustomEquipKind : u8 { CE_SWORD = 0, CE_SHIELD = 1, CE_TUNIC = 2 };
bool custom_equip_active(CustomEquipKind kind);

struct SlotText {
    const char* str = nullptr;
    u16 msgID = 0;
    SlotText() = default;
    SlotText(const char* s) : str(s) {}
    SlotText(u32 id) : msgID(static_cast<u16>(id)) {}
};

using SlotEquipFn = void (*)(dMenu_Collect2D_c* collect2D);

using SlotUnlockFn = bool (*)(u8 x, u8 y);

struct SlotCell { u8 x = 0xFF, y = 0xFF; };   // {0xFF,0xFF} = none
inline bool slot_cell_set(SlotCell c) { return c.x != 0xFF; }

struct GridPos { u8 row = 0, item = 0; };     // {0,0} = unset
inline bool grid_pos_set(GridPos p) { return p.row != 0; }

struct SlotAuto {
    bool on = false;
    f32  posX = 0.0f, posY = 0.0f;
    GridPos navLeft, navRight, navUp, navDown;
};

// A mod-added collection-grid slot, built by addSlot()
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
    J2DPane*    icon    = nullptr;
    J2DPicture* iconPic = nullptr;
    J2DPicture* frame   = nullptr;
};

int cl_removed_cell_count();
CollectionSlot cl_removed_cell_at(int i);
void cl_suppress_removed_cells(struct dMenu_Collect2D_c* collect2D);

SlotCell grid_cell(u8 row, u8 item);

void slot_registry_clear();
void slot_registry_add(const SlotSpec& s);
int  slot_count();
const SlotSpec* slot_get(int i);
const SlotSpec* slot_at(u8 x, u8 y);
const SlotSpec* slot_in_row(u8 y);

const ResTIMG* slot_applied_tex(int i);
void slot_set_applied_tex(int i, const ResTIMG* tex);
const SlotSpec* slot_by_msgid(u32 msgID);

inline J2DPane*    slot_icon(u8 x, u8 y)    { const SlotSpec* s = slot_at(x, y); return s ? s->icon    : nullptr; }
inline J2DPicture* slot_iconPic(u8 x, u8 y) { const SlotSpec* s = slot_at(x, y); return s ? s->iconPic : nullptr; }
inline J2DPicture* slot_frame(u8 x, u8 y)   { const SlotSpec* s = slot_at(x, y); return s ? s->frame   : nullptr; }

SlotCell slot_nav_target(u8 x, u8 y, int dir);

u16 slot_name_id(const SlotSpec* s);
u16 slot_desc_id(const SlotSpec* s);

extern ResourceBuffer s_ordonClothesBtiBuf;

// Pristine vanilla .blo pane translations
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

inline f32 collection_slot_x(f32 col) { return s_ken_n0_origX + col * (s_col_dx + 5.0f); }
inline f32 collection_row_icon_y(u8 row) {
    return row == 0 ? s_ken_n0_origY : row == 1 ? s_tate_n0_origY : s_fuku_n0_origY;
}

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

// Returns the user's texture_replacements/<name>.png override for the given
// bundled texture path, or `fallback` when no override exists.
ResTIMG* tex_replacements_apply(const char* res_path, ResTIMG* fallback);

ResTIMG* get_ordon_clothes_texture();
ResTIMG* get_ordon_hero_texture();
ResTIMG* get_reinforced_shield_texture();
const ResTIMG* safe_get_tex_info(J2DPane* pane);
void set_pane_pos(J2DPane* pane, f32 x, f32 y);
Vec get_pane_center(J2DPane* pane);
void safe_delete_custom_pane(J2DPane*& pane);

inline bool is_collection_menu_enabled() {
    return true;
}

inline bool ordon_shield_slot_present() {
    return cl_column_claimed(2, 1);
}

inline bool is_collect_item_unlocked(u8 x, u8 y) {
    if (const SlotSpec* modSlot = slot_at(x, y)) {
        if (modSlot->unlockFn) return modSlot->unlockFn(x, y);
    }
    if (cl_vanilla_layout_hidden()) return false;
    if (y == 0) {
        u8 eqSword = custom_equip_active(CE_SWORD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipSword();
        if (x == 3) {
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
        if (x == 6) return true;
    } else if (y == 1) {
        u8 eqShield = custom_equip_active(CE_SHIELD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipShield();
        if (x == 3) {
            if (!ordon_shield_slot_present()) return false;
            if (cl_keep_ordon_shield_enabled()) {
                return true;
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
    } else if (y == 2) {
        if (x == 3) return cl_column_claimed(3, 1);
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
