#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/hook.hpp"

class daAlink_c;
struct SaveService;

extern bool g_configQuickAccessEnabled;

extern int g_configQuickAccessAppearance;

ModResult init_quick_access(const HookService* hook_svc, const SaveService* save_svc,
                            ModContext* mod_ctx, ModError* error);
void update_quick_access(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_quick_access();

bool quick_access_is_active();
bool quick_access_keep_boots_equipped(daAlink_c* link);
bool quick_access_keep_bomb_equipped(daAlink_c* link);
