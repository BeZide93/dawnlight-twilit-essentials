#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

extern bool g_configShieldSurf;

bool shield_surf_wants_a();

ModResult init_shield_surf(const HookService* hook_svc, ModError* error);
void shutdown_shield_surf();
