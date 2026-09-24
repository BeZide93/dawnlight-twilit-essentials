#pragma once

// Shared public types of the collection library. Mods include <collection_lib/collection_lib.hpp>.

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

// dComIfGs_* / dItemNo_* - what slot unlock predicates are written with.
#include "d/d_com_inf_game.h"

enum CustomEquipKind : u8 { CE_SWORD = 0, CE_SHIELD = 1, CE_TUNIC = 2 };

// A position in the equipment grid of the Collection screen.
//   row:  1 = swords, 2 = shields, 3 = clothes
//   item: 1-based column, counted left to right
struct CollectionSlot {
    u8 row = 0;
    u8 item = 0;
};
