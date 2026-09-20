#pragma once

#include <cstddef>
#include "collection_common.hpp"

// -------------------------------------------------------------------------
// Generic custom equipment (swords / shields / tunics).
//
// One data table per type (shield/shields.cpp, sword/swords.cpp, ...) registers
// its entries; ALL the plumbing - grid slot, icon .bti, "equipped" state, the
// model swap on Link (world + collection-menu doll), frame highlights, nav - is
// handled here. Adding a new custom item is one custom_equip_register({...}) line.
// -------------------------------------------------------------------------

// Accepts either a file id (0x0042) or nullptr (= "no archive icon", 0xFFFF)
// in the slot initializer, e.g.:
//   "Layout/clctres.arc", 0x0042   -> icon from the mounted clctres archive
//   "collection_screen/x.bti", nullptr -> plain .bti from res/
struct IconArcRef {
    u16 fileId = 0xFFFF;
    IconArcRef() = default;
    IconArcRef(u16 id) : fileId(id) {}
    IconArcRef(std::nullptr_t) : fileId(0xFFFF) {}
};

struct CustomEquipDef {
    CustomEquipKind kind;
    u8          item;          // 1-based column in its row (shield row = row 2, etc.)
    const char* name;          // slot title (literal)
    const char* description;   // slot description (literal)
    const char* iconBti;       // res-relative, e.g. "textures/clctres/reinforced_shield.bti" -
                               //   OR the path of an archive whose file at iconArcFileId holds
                               //   the icon (e.g. "Layout/clctres.arc"); ignored then
    IconArcRef  iconArcFileId;              // file id of the icon inside the collection screen's own
                               //   resource archive (res/Layout/clctres.arc, already mounted by
                               //   the game -> overlay version wins). 0xFFFF = use iconBti
    const char* modelArc;      // res-relative, e.g. "models/clctres/ReinforcedShield.arc" - or
                               //   the name of one of the game's own object archives (e.g.
                               //   "AlLink.arc") to build on vanilla models without shipping
                               //   them in res/ (game-data fallback)
    u32         modelFileId;   // archive file index of the BMD, e.g. 0x0003
    u32         sheathFileId = 0xFFFF; // SWORD only: archive file index of the sheath BMD (0xFFFF = none / nullptr)

    // Model fit-up, relative to the vanilla equip model's TR matrix.
    f32 offX = 0.0f, offY = 0.0f, offZ = 0.0f;
    f32 rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;   // degrees
    f32 scale = 1.0f;

    // TUNIC only: which vanilla clothes model the custom body is built on top of
    // (Kokiri / Zora / Magic Armor / casual). The engine is forced to this base
    // so the swap has the right skeleton + sub-models to graft onto.
    u8 baseClothes = dItemNo_WEAR_KOKIRI_e;

    // TUNIC only: override the GameCube-LED (gamepad) colour while this tunic
    // is worn - 0xRRGGBB packed. 0xFFFFFFFF (default) = vanilla colour logic
    // (dusk gamepad_color.cpp: green Kokiri / blue Zora / red Magic ...).
    unsigned int padColor = 0xFFFFFFFFu;

    bool (*unlocked)() = nullptr;   // optional gate; nullptr = always available

};

// --- registration (called each menu-screen build; idempotent by kind+item) ---
void custom_equip_reset_registry();
// Remove one registered slot by id (compacts the registry; ids of later slots shift).
void custom_equip_remove(int id);
int  custom_equip_register(const CustomEquipDef& def);
int  custom_equip_count();
const CustomEquipDef* custom_equip_get(int id);

// --- per-kind active state (at most one custom item active per kind) ---
void custom_equip_activate(int id);
void custom_equip_clear(CustomEquipKind kind);
bool custom_equip_active(CustomEquipKind kind);
int  custom_equip_active_id(CustomEquipKind kind);

// --- generic slot callbacks (wired into addSwordItem/addShieldItem/...) ---
void custom_equip_on_equip(dMenu_Collect2D_c* collect2D);   // SlotEquipFn
bool custom_equip_is_unlocked(u8 x, u8 y);                  // SlotUnlockFn
bool custom_equip_is_equipped(u8 x, u8 y);                  // SlotUnlockFn (equippedFn)

// icon texture for a def, loaded on demand from def->iconBti
ResTIMG* custom_equip_icon(int id);

// unique-per-def pane tags for addSlot (icon container / icon pic / frame)
u64 custom_equip_icon_tag(int id);
u64 custom_equip_pic_tag(int id);
u64 custom_equip_frame_tag(int id);

// --- lifecycle (called from collection_menu.cpp) ---
struct SaveService;
class daAlink_c;
void custom_equip_init_hooks(const HookService* hook_svc, const SaveService* save_svc);
void custom_equip_update();
void custom_equip_shutdown();
void custom_equip_restore_from_save();
// Re-apply the custom gear the instant Link's human models are (re)built -
// wired to the daAlink_c::changeLink hook (create / clothes change / Wolf->Human)
// so nothing vanilla flashes.
void custom_equip_before_link_rebuild();            // changeLink PRE
void custom_equip_on_alink_created(daAlink_c* a);   // changeLink / create POST
void custom_equip_set_link_model_wolf(bool isWolf); // changeWolf/changeLink POST
