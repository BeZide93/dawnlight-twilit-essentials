#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"

extern bool g_configStaminaSwimSprint;
extern float g_configStaminaSwimSprintSpeed;

ModResult init_sprint_swim(const HookService* hook_svc);
void update_sprint_swim();
void shutdown_sprint_swim();
