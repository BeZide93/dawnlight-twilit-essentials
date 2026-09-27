#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "SSystem/SComponent/c_sxyz.h"

#define USE_DARK_LINK 0

bool boss_rush_darklink_enabled();
void boss_rush_darklink_draw(const cXyz& pos, const csXyz& angle);
void boss_rush_darklink_unload();
