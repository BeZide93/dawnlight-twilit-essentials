#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configFlurryRushEnabled;
extern int g_configFlurryRushPerfectFrames;
extern int g_configFlurryRushSlowFactor;
extern int g_configFlurryRushWindowTicks;
extern int g_configFlurryRushHits;

bool flurry_rush_is_rush_active();

void flurry_rush_apply_enabled();

ModResult init_flurry_rush(const HookService* hook_svc, const LogService* log_svc, ModError* error);
void update_flurry_rush(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_flurry_rush();
