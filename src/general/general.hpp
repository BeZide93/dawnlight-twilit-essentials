#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

#include "skip_cutscenes.hpp"
#include "fast_forward_cutscenes.hpp"
#include "dominion_sword.hpp"
#include "faster_transitions.hpp"
#include "lockon_letterbox.hpp"

ModResult init_general(const HookService* hook_svc, ModError* error);
void update_general(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_general();
