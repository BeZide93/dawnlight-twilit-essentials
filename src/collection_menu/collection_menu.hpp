#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/hook.hpp"

extern bool g_configCollectionStarterEquip;
extern bool g_configCollectionKeepOrdonShield;
extern bool g_configCollectionShowOrdonHero;
extern bool g_configCollectionOrdonHeroAlways;

void request_collection_menu_reload();
void sync_collection_ordon_hero_page();


struct SaveService;
ModResult init_collection_menu(const HookService* hook_svc, const LogService* log_svc, const SaveService* save_svc, ModContext* mod_ctx, ModError* error);
void update_collection_menu(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_collection_menu();
