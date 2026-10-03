#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

class daAlink_c;

enum ZoraArmorFxType : int {
    ZORA_FX_NONE = -1,
    ZORA_FX_KOKIRI = 0,
    ZORA_FX_ZORA,
    ZORA_FX_MAGIC,
    ZORA_FX_CASUAL,
    ZORA_FX_CUSTOM_BASE,
};

ModResult init_zora_armor_fx(const HookService* hook_svc, ModError* error);
int zora_armor_fx_current_type(daAlink_c* link);
void zora_armor_fx_preload(int type);
bool zora_armor_fx_begin(daAlink_c* link, int fromType, int toType);
void update_zora_armor_fx();
void shutdown_zora_armor_fx();
