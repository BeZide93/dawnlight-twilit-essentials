#pragma once

#include "collection_common.hpp"
#include "collection_page.hpp"
#include "custom_equip.hpp"

#include "mods/svc/save.h"

ModResult collectionlib_init(const HookService* hook_svc, const LogService* log_svc,
                             const SaveService* save_svc, ModContext* mod_ctx,
                             ModError* error = nullptr);

void collectionlib_update();
void collectionlib_shutdown();

void collectionlib_set_register_callback(void (*fn)());

int collectionlib_add_sword_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_shield_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_tunic_slot(u8 item, const CustomEquipDef& def);

int collectionlib_add_next_sword_slot(const CustomEquipDef& def);
int collectionlib_add_next_shield_slot(const CustomEquipDef& def);
int collectionlib_add_next_tunic_slot(const CustomEquipDef& def);

inline int collectionlib_register_slot(const CustomEquipDef& def) {
    return custom_equip_register(def);
}

int collectionlib_clear_all_slots();

struct CollectionSlotRef {
    u8 row = 0;
    u8 item = 0;
    bool remove() const;
    bool move(u8 newRow, u8 newItem) const;
    bool exists() const;
    int replace(const CustomEquipDef& def) const;
};

CollectionSlotRef collectionlib_get_slot_ref(u8 row, u8 item);
int collectionlib_add_slot_override(u8 row, u8 item, const CustomEquipDef& def);

using Slot = CollectionSlotRef;
Slot get_slot(u8 row, u8 item);

int collectionlib_remove_slot(u8 row, u8 item);

inline int collectionlib_slot_count() {
    return custom_equip_count();
}

inline void collectionlib_activate(int id) { custom_equip_activate(id); }
inline void collectionlib_clear(CustomEquipKind kind) { custom_equip_clear(kind); }
inline bool collectionlib_active(CustomEquipKind kind) { return custom_equip_active(kind); }
inline int collectionlib_active_id(CustomEquipKind kind) { return custom_equip_active_id(kind); }
