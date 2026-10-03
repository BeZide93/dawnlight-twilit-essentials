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
bool collection_linkle_active();
bool collection_ordon_hero_enabled();

enum CollectionTunic {
    COLLECTION_TUNIC_ORDON = 0,
    COLLECTION_TUNIC_HERO,
    COLLECTION_TUNIC_ZORA,
    COLLECTION_TUNIC_MAGIC,
    COLLECTION_TUNIC_ORDON_HERO,
    COLLECTION_TUNIC_COUNT,
};

struct ResTIMG;
bool collection_tunic_unlocked(int tunic);
bool collection_tunic_equipped(int tunic);
enum CollectionTunicEquipFlag : unsigned {
    TUNIC_EQUIP_SWIMMING = 1u << 0,
    TUNIC_EQUIP_SEAMLESS = 1u << 1,
    TUNIC_EQUIP_SILENT = 1u << 2,
    TUNIC_EQUIP_ANY_POSE = 1u << 3,
};

bool collection_tunic_equip(int tunic, unsigned flags = 0);
bool collection_tunic_change_pending();
const char* collection_tunic_name(int tunic);
ResTIMG* collection_tunic_icon(int tunic);
unsigned char collection_tunic_icon_item(int tunic);

struct SaveService;
ModResult init_collection_menu(const HookService* hook_svc, const LogService* log_svc, const SaveService* save_svc, ModContext* mod_ctx, ModError* error);
void update_collection_menu(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_collection_menu();
