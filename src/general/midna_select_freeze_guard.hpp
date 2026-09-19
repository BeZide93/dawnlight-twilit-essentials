#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

ModResult init_midna_select_freeze_guard(const HookService* hook_svc, ModError* error);
void shutdown_midna_select_freeze_guard();
