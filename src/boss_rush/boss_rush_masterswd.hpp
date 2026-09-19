#pragma once

#include "mods/svc/log.h"
#include "SSystem/SComponent/c_xyz.h"

class daAlink_c;

void update_boss_rush_master_sword_effects();
void draw_boss_rush_master_sword(float floorY);
void draw_boss_rush_master_sword_label(const daAlink_c* link, float floorY);
void boss_rush_master_sword_reset_fade();
void unload_boss_rush_master_sword();
bool boss_rush_master_sword_near();
