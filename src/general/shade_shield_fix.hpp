#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

ModResult init_shade_shield_fix(const HookService* hook_svc, ModError* error);
void shutdown_shade_shield_fix();
