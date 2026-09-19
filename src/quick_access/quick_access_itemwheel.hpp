#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configQuickAccessHideWheelItems;

ModResult init_quick_access_itemwheel(const HookService* hook_svc, ModError* error);
void update_quick_access_itemwheel();
void shutdown_quick_access_itemwheel();

void quick_access_itemwheel_refresh();
