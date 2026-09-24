#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"
#include "mods/svc/config.h"

ModResult init_boss_rush(const HookService* hook_svc, const LogService* log_svc,
                         const UiService* ui_svc, const ConfigService* config_svc,
                         ModContext* mod_ctx, ModError* error);

void update_boss_rush(const LogService* log_svc, ModContext* mod_ctx);

void shutdown_boss_rush();

void start_boss_rush();
void boss_rush_begin_game_mode();
void boss_rush_arm_chamber_camera(int frames);
void start_boss_rush_dungeon_warp();
void start_boss_rush_map_portal_warp();
void exit_boss_rush();

bool is_in_boss_rush_chamber();
bool boss_rush_settle_window_active();
bool is_boss_rush_active();
bool boss_rush_is_fighting_here();
bool boss_rush_is_fight_engaged();
bool is_boss_rush_transition_in_flight();
unsigned int boss_rush_debug_transition_bits();
bool boss_rush_is_hud_menu_blocking();
bool boss_rush_is_fight_retry_warp();
bool boss_rush_is_returning_to_chamber();
const char* boss_rush_current_target_name();
int boss_rush_current_target_index();
void return_to_boss_rush_chamber(const LogService* log_svc, ModContext* mod_ctx, const char* reason);
void boss_rush_retry_current_fight(const LogService* log_svc, ModContext* mod_ctx);
void boss_rush_request_retry();
void boss_rush_debug_kill_current_boss();

extern bool g_configBossRushSuggestedItems;
extern bool g_configMasterRushRetryFromStart;

extern bool g_configBossRushRefillAfterFight;

extern bool g_configBossRushSeparateGanon;

size_t boss_rush_get_active_gallery_count();
size_t boss_rush_get_active_gallery_table_index(size_t circleSlot);

void boss_rush_debug_log(const char* fmt, ...);
int boss_rush_gauntlet_phase();
