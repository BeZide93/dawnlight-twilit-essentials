#pragma once

#include <cstddef>
#include "collection_common.hpp"

// Icon inside an archive: file id of a .bti in the archive named by CustomEquipDef::iconBti.
// Resolved against the mod's res/ first, then against the game's collection archive
// (Layout/clctres.arc), so overlay-patched game archives work too.
struct IconArcRef {
    u16 fileId = 0xFFFF;
    IconArcRef() = default;
    IconArcRef(u16 id) : fileId(id) {}
    IconArcRef(std::nullptr_t) : fileId(0xFFFF) {}
};

struct CustomEquipDef {
    CustomEquipKind kind;
    u8          item;               // column; set by the layout functions
    const char* name;               // nullptr = the game's name and description of baseItem
    const char* description;
    const char* iconBti;            // .bti path in res/, or the archive holding iconArcFileId;
                                    // nullptr = the game's icon of baseItem (Wooden / Ordon
                                    // Sword, Ordon / Wooden Shield)
    IconArcRef  iconArcFileId;
    const char* modelArc;           // nullptr = no model swap: the slot only equips baseItem
    u32         modelFileId;
    u32         sheathFileId = 0xFFFF; // SWORD only

    f32 offX = 0.0f, offY = 0.0f, offZ = 0.0f;
    f32 rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;   // degrees
    f32 scale = 1.0f;

    // The vanilla item this slot stands in for. Equipping the slot equips it underneath, so
    // gameplay (damage, Zora diving, ...) behaves like that item:
    //   swords:  WOOD_STICK / SWORD / MASTER_SWORD (Light Sword once owned)
    //   shields: WOOD_SHIELD / SHIELD / HYLIA_SHIELD
    //   clothes: WEAR_CASUAL / WEAR_KOKIRI / WEAR_ZORA / ARMOR (the body the model grafts on)
    // dItemNo_NONE_e: swords/shields keep whatever is equipped, clothes use WEAR_KOKIRI.
    u8 baseItem = dItemNo_NONE_e;
    unsigned int padColor = 0xFFFFFFFFu;    // TUNIC only: 0xRRGGBB, 0xFFFFFFFF = vanilla logic

    bool (*unlocked)() = nullptr;   // nullptr = always available

    // TUNIC only: the Iron Boots hide the model's own boots, like on every vanilla outfit.
    // false keeps them visible (models whose feet the Iron Boots do not cover).
    bool ironBootsHideFeet = true;
};

int  custom_equip_count();
const CustomEquipDef* custom_equip_get(int id);

void custom_equip_set_suppressed(bool suppressed);
bool custom_equip_is_suppressed();
void custom_equip_activate(int id);
void custom_equip_deactivate(CustomEquipKind kind);
void custom_equip_clear(CustomEquipKind kind);
bool custom_equip_active(CustomEquipKind kind);
int  custom_equip_active_id(CustomEquipKind kind);
// Equip the custom items the save file had on again (e.g. after custom_equip_set_suppressed).
void custom_equip_restore_from_save();
