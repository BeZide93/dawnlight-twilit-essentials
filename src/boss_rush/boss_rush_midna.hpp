#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

ModResult init_boss_rush_midna(const HookService* hook_svc, const LogService* log_svc, ModContext* mod_ctx);
void refresh_boss_rush_midna_flow();
bool process_pending_boss_rush_midna_action(const LogService* log_svc, ModContext* mod_ctx);
void reset_boss_rush_midna_flow();
void shutdown_boss_rush_midna();

bool is_boss_rush_ganon_stage(const char* stage);
bool is_boss_rush_ganon_fight();
void update_boss_rush_midna(const LogService* log_svc, ModContext* mod_ctx);

bool boss_rush_midna_talk_hold_active();
