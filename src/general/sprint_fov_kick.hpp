#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

extern bool g_configSprintFovKickEnabled;

ModResult init_sprint_fov_kick(const HookService* hook_svc, ModError* error);
void update_sprint_fov_kick();
void shutdown_sprint_fov_kick();
