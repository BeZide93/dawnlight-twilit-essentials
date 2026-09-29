#pragma once

#include "mods/api.h"
#include "mods/svc/hook.hpp"

ModResult init_sprint_wind(const HookService* hook_svc);
void shutdown_sprint_wind();
