#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

extern bool g_configFasterMidnaCancel;

ModResult init_faster_midna_cancel(const HookService* hook_svc, ModError* error);
void shutdown_faster_midna_cancel();
