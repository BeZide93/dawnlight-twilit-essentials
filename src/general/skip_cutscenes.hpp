#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

extern bool g_configGeneralSkipCutscenes;

ModResult init_skip_cutscenes(const HookService* hook_svc, ModError* error);
void shutdown_skip_cutscenes();
