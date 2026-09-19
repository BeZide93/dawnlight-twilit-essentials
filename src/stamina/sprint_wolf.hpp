#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"

extern bool g_configStaminaWolfSprint;
extern float g_configStaminaWolfSprintSpeed;

ModResult init_sprint_wolf(const HookService* hook_svc);
void shutdown_sprint_wolf();
