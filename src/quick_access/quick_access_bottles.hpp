#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

struct SaveService;

extern bool g_configBottlesQuickAccessEnabled;

ModResult init_quick_access_bottles(const HookService* hook_svc, const SaveService* save_svc,
                                    ModContext* mod_ctx, ModError* error);
void shutdown_quick_access_bottles();

bool quick_access_bottles_hotkey_active();
