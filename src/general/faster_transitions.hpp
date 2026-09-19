#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

extern bool g_configFasterTransitions;

void faster_transitions_apply_mode();

ModResult init_faster_transitions(const HookService* hook_svc, ModError* error);
void shutdown_faster_transitions();
