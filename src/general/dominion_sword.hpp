#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

extern bool g_configGeneralDominionSword;

ModResult init_dominion_sword(const HookService* hook_svc, ModError* error);
void update_dominion_sword();
void shutdown_dominion_sword();
