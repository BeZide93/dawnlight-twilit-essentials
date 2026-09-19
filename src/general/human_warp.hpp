#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configGeneralHumanWarpAnimation;

ModResult init_human_warp(const HookService* hook_svc, ModError* error);
void update_human_warp(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_human_warp();

void human_warp_cinematic_departure();
void human_warp_cinematic_arrival();
void human_warp_cinematic_end();

void human_warp_arm_arrival_replay();
