#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

enum FastForwardCutscenesMode {
    FF_CUTSCENES_OFF = 0,
    FF_CUTSCENES_ON = 1,
    FF_CUTSCENES_VERY_FAST = 2,
};

extern int g_configGeneralFastForwardCutscenesMode;
extern float g_configGeneralFastForwardSpeed;
extern bool g_configGeneralFastDoorAnimations;

float clamp_fast_forward_speed(float speed);

ModResult init_fast_forward_cutscenes(const HookService* hook_svc, ModError* error);
void update_fast_forward_cutscenes(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_fast_forward_cutscenes();

float general_get_aurora_timescale();
bool general_timescale_available();
void general_set_slow_motion(float scale);
void fast_forward_set_hidden_run(bool on);
