#pragma once

#include "boss_rush_common.hpp"
#include "mods/service.hpp"

extern bool g_configBossRushVanillaGear;

void apply_boss_rush_equipment_restriction(const BossGalleryEntry& boss);
void sync_life_meter_instant(u16 life, u16 maxLife);
