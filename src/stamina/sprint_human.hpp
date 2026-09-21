#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"

extern bool g_configStaminaSprint;
extern bool g_configStaminaSrcSprint;
extern float g_configStaminaSprintSpeed;
extern bool g_configStaminaSprintStartRoll;

ModResult init_sprint_human(const HookService* hook_svc);
void update_sprint_human();
void shutdown_sprint_human();
