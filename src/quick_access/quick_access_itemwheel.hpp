#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/save.h"

extern bool g_configQuickAccessHideWheelItems;

ModResult init_quick_access_itemwheel(const HookService* hook_svc, const SaveService* save_svc,
                                      ModContext* ctx, ModError* error);
void update_quick_access_itemwheel();
void shutdown_quick_access_itemwheel();

void quick_access_itemwheel_refresh();
void quick_access_itemwheel_sync_ring_archive();
