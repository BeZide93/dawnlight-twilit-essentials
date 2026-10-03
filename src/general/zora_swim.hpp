#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

extern bool g_configModernZoraSwim;

float zora_swim_fov_kick();

ModResult init_zora_swim(const HookService* hook_svc, ModError* error);
void shutdown_zora_swim();
