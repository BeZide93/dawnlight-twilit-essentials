#pragma once

#include <cstddef>
#include "collection_common.hpp"

struct IconArcRef {
    u16 fileId = 0xFFFF;
    IconArcRef() = default;
    IconArcRef(u16 id) : fileId(id) {}
    IconArcRef(std::nullptr_t) : fileId(0xFFFF) {}
};

struct CustomEquipDef {
    CustomEquipKind kind;
    u8          item;
    const char* name;
    const char* description;
    const char* iconBti;

    IconArcRef  iconArcFileId;
    const char* modelArc;
    u32         modelFileId;
    u32         sheathFileId = 0xFFFF;

    f32 offX = 0.0f, offY = 0.0f, offZ = 0.0f;
    f32 rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    f32 scale = 1.0f;

    u8 baseItem = dItemNo_NONE_e;
    unsigned int padColor = 0xFFFFFFFFu;

    bool (*unlocked)() = nullptr;

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

void custom_equip_restore_from_save();
