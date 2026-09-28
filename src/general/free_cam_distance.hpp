#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

extern int g_configFreeCamDistance;
extern bool g_configFreeCamDistanceZTarget;

ModResult init_free_cam_distance(const HookService* hook_svc, ModError* error);
void shutdown_free_cam_distance();
