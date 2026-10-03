#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/ui.h"

void boss_rush_leaderboard_init(const ConfigService* config_svc, ModContext* mod_ctx,
                                ConfigVarHandle enabled_var, ConfigVarHandle name_var,
                                ConfigVarHandle token_var);
void boss_rush_leaderboard_shutdown();
void boss_rush_leaderboard_update();

void boss_rush_leaderboard_set_enabled(bool enabled);
void boss_rush_leaderboard_set_name(const char* name);
bool boss_rush_leaderboard_enabled();

void boss_rush_leaderboard_submit_boss(int tableIndex, unsigned int cs);
void boss_rush_leaderboard_submit_all_phases(unsigned int cs);
void boss_rush_leaderboard_submit_master_rush(unsigned int cs);

void boss_rush_leaderboard_add_status(UiElementHandle pane);
void boss_rush_leaderboard_status_update();

void boss_rush_leaderboard_install_hooks(const HookService* hook_svc);
void boss_rush_leaderboard_view_open();
bool boss_rush_leaderboard_view_active();
bool boss_rush_leaderboard_view_focused();
void draw_boss_rush_leaderboard_view();
