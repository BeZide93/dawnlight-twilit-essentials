#pragma once

#include "mods/service.hpp"
#include "mods/svc/config.h"

#include <cstddef>

extern bool g_configBossRushTimer;
extern bool g_configBossRushShowBestTimer;

void boss_rush_timer_init(const ConfigService* config_svc, ModContext* mod_ctx,
                          ConfigVarHandle var);

void boss_rush_timer_update();

void boss_rush_timer_notify_defeat();

void boss_rush_timer_reset_run();

void boss_rush_timer_clear_best();

bool boss_rush_timer_active_cs(unsigned int* outCs);
bool boss_rush_timer_best_cs(int tableIndex, unsigned int* outCs);
bool boss_rush_timer_result(unsigned int* outCs, bool* outIsRecord);
bool boss_rush_timer_last_was_record();
void boss_rush_timer_format(unsigned int cs, char* buf, size_t bufLen);

void boss_rush_timer_begin_chain_run();
void boss_rush_timer_end_chain_run();
bool boss_rush_timer_chain_active();
bool boss_rush_timer_chain_best_cs(unsigned int* outCs);
void boss_rush_timer_commit_chain_total();
void boss_rush_timer_init_chain_best(const ConfigService* config_svc, ModContext* mod_ctx,
                                     ConfigVarHandle var);

void boss_rush_timer_begin_all_phases();
void boss_rush_timer_end_all_phases();
bool boss_rush_timer_all_phases_active();
void boss_rush_timer_init_all_phases_best(const ConfigService* config_svc, ModContext* mod_ctx,
                                        ConfigVarHandle var);
bool boss_rush_timer_all_phases_best_cs(unsigned int* outCs);
