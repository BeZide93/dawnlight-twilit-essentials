#pragma once
#include <cstdint>
#include "mods/api.h"
#include "mods/svc/config.h"

struct HookService;
struct LogService;
struct ConfigService;

extern bool g_configBossBarEnabled;

/* Screen-space offset of the boss bar from its default position. */
extern float g_configBossBarX;
extern float g_configBossBarY;

ModResult init_boss_bar(const HookService* hook_svc, ModError* error);
void update_boss_bar(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_bar();

/* Shows the boss bar as a static preview at its configured position (used by
 * the Customization tab); hidden again via cancel. */
void boss_bar_preview_request();
void boss_bar_preview_cancel();

/* Config vars bossBarX/bossBarY ([0] = X, [1] = Y), registered by
 * init_boss_bar_config. */
extern ConfigVarHandle g_bossBarVars[2];
ModResult init_boss_bar_config(const ConfigService* cfg, ModContext* ctx);

bool boss_bar_is_boss_name(int16_t name);

bool boss_bar_consume_defeat_event();
void boss_bar_force_defeat_event();
void boss_bar_rearm_defeat(unsigned int actorId);

bool boss_bar_debug_snapshot(char* buf, size_t bufSize);

bool boss_bar_current_fight_state(const char** outLabel, bool& outEngaged);

bool boss_bar_boss_defeated_now();

bool boss_bar_hidden_for_transition();

void boss_bar_force_reset();
