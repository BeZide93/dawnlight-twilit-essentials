#pragma once

#include <cstdint>

#include "mods/api.h"
#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

extern bool g_configDamageVignetteEnabled;
extern int g_configDamageVignetteIntensity;

ModResult init_damage_vignette(const GfxService* gfx_svc, const ResourceService* res_svc,
                               const LogService* log_svc, ModContext* mod_ctx, ModError* error);
void update_damage_vignette(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_damage_vignette();

void damage_vignette_request_preview();
void damage_vignette_notify_life_set(std::uint16_t life);
