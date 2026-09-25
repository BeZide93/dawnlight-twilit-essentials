#pragma once

#include <cstdint>

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

ModResult init_boss_rush_music(const HookService* hook_svc, const LogService* log_svc,
                               ModContext* mod_ctx);
void shutdown_boss_rush_music();
void boss_rush_music_yield_to_stream(uint32_t streamId);
void boss_rush_music_begin_chamber_transition();
