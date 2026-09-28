#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

extern bool g_configNoBattleMusic;

ModResult init_no_battle_music(const HookService* hook_svc, ModError* error);
void shutdown_no_battle_music();
