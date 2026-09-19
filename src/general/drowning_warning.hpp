#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

extern bool g_configDrowningWarningEnabled;

ModResult init_drowning_warning(const HookService* hook_svc, ModError* error);
void update_drowning_warning();
void shutdown_drowning_warning();
