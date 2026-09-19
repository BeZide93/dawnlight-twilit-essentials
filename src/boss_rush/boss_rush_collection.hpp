#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

void init_boss_rush_collection(const HookService* hook_svc, const LogService* log_svc, ModContext* mod_ctx);
