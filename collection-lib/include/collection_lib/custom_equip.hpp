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
    u32         sheathFileId = 0xFFFF; // SWORD only

    f32 offX = 0.0f, offY = 0.0f, offZ = 0.0f;
    f32 rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;   // degrees
    f32 scale = 1.0f;

    u8 baseClothes = dItemNo_WEAR_KOKIRI_e; // TUNIC only: base skeleton
    unsigned int padColor = 0xFFFFFFFFu;    // TUNIC only: 0xRRGGBB, 0xFFFFFFFF = vanilla logic

    bool (*unlocked)() = nullptr;   // nullptr = always available
};

void custom_equip_reset_registry();
void custom_equip_remove(int id);
int  custom_equip_register(const CustomEquipDef& def);
int  custom_equip_count();
const CustomEquipDef* custom_equip_get(int id);

void custom_equip_activate(int id);
void custom_equip_clear(CustomEquipKind kind);
bool custom_equip_active(CustomEquipKind kind);
int  custom_equip_active_id(CustomEquipKind kind);

void custom_equip_on_equip(dMenu_Collect2D_c* collect2D);
bool custom_equip_is_unlocked(u8 x, u8 y);
bool custom_equip_is_equipped(u8 x, u8 y);

ResTIMG* custom_equip_icon(int id);

u64 custom_equip_icon_tag(int id);
u64 custom_equip_pic_tag(int id);
u64 custom_equip_frame_tag(int id);

struct SaveService;
class daAlink_c;
void custom_equip_init_hooks(const HookService* hook_svc, const SaveService* save_svc);
void custom_equip_update();
void custom_equip_shutdown();
void custom_equip_restore_from_save();
void custom_equip_before_link_rebuild();
void custom_equip_on_alink_created(daAlink_c* a);
void custom_equip_set_link_model_wolf(bool isWolf);
