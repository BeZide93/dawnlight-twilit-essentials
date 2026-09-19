#pragma once
#include <cstdint>
#include "mods/api.h"

struct HookService;
struct LogService;

extern bool g_configBossBarEnabled;

ModResult init_boss_bar(const HookService* hook_svc, ModError* error);
void update_boss_bar(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_bar();

bool boss_bar_is_boss_name(int16_t name);

bool boss_bar_consume_defeat_event();
void boss_bar_force_defeat_event();
void boss_bar_rearm_defeat(unsigned int actorId);

bool boss_bar_debug_snapshot(char* buf, size_t bufSize);

bool boss_bar_current_fight_state(const char** outLabel, bool& outEngaged);

bool boss_bar_boss_defeated_now();

bool boss_bar_hidden_for_transition();

void boss_bar_force_reset();
