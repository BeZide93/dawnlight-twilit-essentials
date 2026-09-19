#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

ModResult init_boss_rush_dpad(const HookService* hook_svc, const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_rush_dpad();
