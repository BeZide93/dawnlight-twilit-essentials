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

struct CollectionSlotRef {
    u8 row = 0;
    u8 item = 0;

    int  replace(const CustomEquipDef& def) const;

    int  insert(const CustomEquipDef& def) const;

    bool remove() const;

    bool move(u8 newRow, u8 newItem) const;
    bool exists() const;
    bool is_native() const;
};
using Slot = CollectionSlotRef;

Slot get_slot(u8 row, u8 item);
CollectionSlotRef collectionlib_get_slot_ref(u8 row, u8 item);

int collectionlib_add_sword_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_shield_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_tunic_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_slot_override(u8 row, u8 item, const CustomEquipDef& def);

int collectionlib_add_next_sword_slot(const CustomEquipDef& def);
int collectionlib_add_next_shield_slot(const CustomEquipDef& def);
int collectionlib_add_next_tunic_slot(const CustomEquipDef& def);

int collectionlib_register_slot(const CustomEquipDef& def);

int  collectionlib_remove_slot(u8 row, u8 item);
int  collectionlib_clear_all_slots();
CollectionSlot collectionlib_get_slot(u8 row, u8 item);
bool collectionlib_move_slot(CollectionSlot from, u8 newItem);
void collectionlib_reset_layout();

void collectionlib_request_reload();

inline int  collectionlib_slot_count() { return custom_equip_count(); }
inline void collectionlib_activate(int id) { custom_equip_activate(id); }
inline void collectionlib_clear(CustomEquipKind kind) { custom_equip_clear(kind); }
inline bool collectionlib_active(CustomEquipKind kind) { return custom_equip_active(kind); }
inline int  collectionlib_active_id(CustomEquipKind kind) { return custom_equip_active_id(kind); }

void collectionlib_set_unequip_policy(bool (*fn)());

void collectionlib_set_keep_ordon_shield_policy(bool (*fn)());

void collectionlib_set_hd_layout_policy(bool (*fn)());
