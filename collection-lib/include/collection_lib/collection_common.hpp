#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

#include "d/d_com_inf_game.h"

enum CustomEquipKind : u8 { CE_SWORD = 0, CE_SHIELD = 1, CE_TUNIC = 2 };

struct CollectionSlot {
    u8 row = 0;
    u8 item = 0;
};
