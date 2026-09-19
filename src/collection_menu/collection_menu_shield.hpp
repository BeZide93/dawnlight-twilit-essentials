#pragma once

#include "mods/service.hpp"

struct LogService;
struct ModContext;
struct HookService;
ModResult init_collection_menu_chest(const HookService* hook_svc, const LogService* log_svc,
                                     ModContext* mod_ctx, ModError* error);
void update_collection_menu_chest(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_collection_menu_chest();
