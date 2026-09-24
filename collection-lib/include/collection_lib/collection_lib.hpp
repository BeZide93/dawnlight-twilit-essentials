#pragma once

#include "collection_common.hpp"
#include "collection_page.hpp"
#include "custom_equip.hpp"

#include "mods/svc/save.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ModResult collectionlib_init(const HookService* hook_svc, const LogService* log_svc,
                             const SaveService* save_svc, ModContext* mod_ctx,
                             ModError* error = nullptr);

void collectionlib_update();
void collectionlib_shutdown();

// The callback describes every change the mod makes to the equipment rows. It runs on init
// and again before every build of the Collection screen, always starting from the native
// layout - so it must be a pure description (same calls every time).
void collectionlib_set_register_callback(void (*fn)());

// ---------------------------------------------------------------------------
// Layout
//
// Without any call the equipment rows stay exactly native. Columns are 1-based and follow
// the native layout:
//   row 1 (swords):  1 = Ordon Sword (Wooden Sword before it), 2 = Master Sword
//   row 2 (shields): 1 = Wooden Shield (Ordon Shield before it), 2 = Hylian Shield
//   row 3 (clothes): 1 = Hero's Clothes, 2 = Zora Armor, 3 = Magic Armor
// Native items keep their native behavior (visibility, name, equip, equipped frame) no matter
// where they are moved to. A row holds up to 6 columns - the sword row only 4 while the Pieces
// of Heart and the Fused Shadow use its spare grid cells; put those on a page (cl::heart(),
// cl::fused_shadow()) for all 6. Rows longer than the native ones push the heart and the
// Fused Shadow to the right.
// ---------------------------------------------------------------------------

struct CollectionSlotRef {
    u8 row = 0;
    u8 item = 0;

    // Put a custom item into this column (replaces a native or custom item, fills an empty
    // column). Returns the custom slot id or -1.
    int  replace(const CustomEquipDef& def) const;
    // Put a custom item into this column; this column's item and the ones behind it move one
    // column to the right (up to the next empty column). Returns the custom slot id or -1.
    int  insert(const CustomEquipDef& def) const;
    // Empty this column (native items disappear from the grid).
    bool remove() const;
    // Move this column's item to another column of the same row; the item there takes this
    // column.
    bool move(u8 newRow, u8 newItem) const;
    bool exists() const;
    bool is_native() const;
};
using Slot = CollectionSlotRef;

Slot get_slot(u8 row, u8 item);
CollectionSlotRef collectionlib_get_slot_ref(u8 row, u8 item);

// Custom item at an explicit column (same as get_slot(row, item).replace(def)).
int collectionlib_add_sword_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_shield_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_tunic_slot(u8 item, const CustomEquipDef& def);
int collectionlib_add_slot_override(u8 row, u8 item, const CustomEquipDef& def);

// Custom item at the first empty column of the row; -1 when the row is full.
int collectionlib_add_next_sword_slot(const CustomEquipDef& def);
int collectionlib_add_next_shield_slot(const CustomEquipDef& def);
int collectionlib_add_next_tunic_slot(const CustomEquipDef& def);

// Row from def.kind, column def.item (0 = first empty column).
int collectionlib_register_slot(const CustomEquipDef& def);

int  collectionlib_remove_slot(u8 row, u8 item);
int  collectionlib_clear_all_slots();                 // empty all three rows (native items too)
CollectionSlot collectionlib_get_slot(u8 row, u8 item);
bool collectionlib_move_slot(CollectionSlot from, u8 newItem);
void collectionlib_reset_layout();                    // back to the native layout

// Rebuild the Collection screen (if it is open) after the registration changed.
void collectionlib_request_reload();

// ---------------------------------------------------------------------------
// Custom items
// ---------------------------------------------------------------------------

inline int  collectionlib_slot_count() { return custom_equip_count(); }
inline void collectionlib_activate(int id) { custom_equip_activate(id); }
inline void collectionlib_clear(CustomEquipKind kind) { custom_equip_clear(kind); }
inline bool collectionlib_active(CustomEquipKind kind) { return custom_equip_active(kind); }
inline int  collectionlib_active_id(CustomEquipKind kind) { return custom_equip_active_id(kind); }

// ---------------------------------------------------------------------------
// Optional, non-native behavior (off unless the predicate returns true)
// ---------------------------------------------------------------------------

// A on the equipped sword / shield unequips it.
void collectionlib_set_unequip_policy(bool (*fn)());
// Item checks in message flows treat the Ordon Shield as still owned once collected.
void collectionlib_set_keep_ordon_shield_policy(bool (*fn)());
