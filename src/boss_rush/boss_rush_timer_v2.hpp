#pragma once

#include "mods/svc/config.h"

struct ConfigService;

extern float g_configBossRushTimerX;
extern float g_configBossRushTimerY;

void boss_rush_timer_v2_draw(unsigned int cs, bool showingResult, bool isRecord,
                             bool hasBest, unsigned int bestCs, float yShift = 0.0f);

void boss_rush_timer_v2_shutdown();

void boss_rush_timer_preview_request();
void boss_rush_timer_preview_cancel();
bool boss_rush_timer_preview_active();

extern ConfigVarHandle g_bossRushTimerPosVars[2];
ModResult init_boss_rush_timer_pos_config(const ConfigService* cfg, ModContext* ctx);
