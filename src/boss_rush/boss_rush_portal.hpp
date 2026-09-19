#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configBossRushPortal;

ModResult init_boss_rush_portal(const HookService* hook_svc, const LogService* log_svc,
                                ModContext* mod_ctx);
void update_boss_rush_portal(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_rush_portal();
