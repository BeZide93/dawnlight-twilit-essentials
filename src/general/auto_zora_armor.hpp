#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

extern bool g_configAutoZoraArmor;

ModResult init_auto_zora_armor(const HookService* hook_svc, ModError* error);
void update_auto_zora_armor();
void shutdown_auto_zora_armor();
