#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configGeneralFastForwardCutscenes;

ModResult init_fast_forward_cutscenes(const HookService* hook_svc, ModError* error);
void update_fast_forward_cutscenes(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_fast_forward_cutscenes();

float general_get_aurora_timescale();
void fast_forward_set_hidden_run(bool on);
