#pragma once

#include "global.h"
#include "mods/service.hpp"

ModResult init_boss_rush_gamemode(ModContext* mod_ctx);
void shutdown_boss_rush_gamemode();
bool boss_rush_game_mode_is_active();
bool boss_rush_game_mode_entering();
void boss_rush_game_mode_return_to_menu();
void boss_rush_game_mode_return_to_menu_smooth();
