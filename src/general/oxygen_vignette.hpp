#pragma once

#include "mods/api.h"
#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

extern bool g_configOxygenVignetteEnabled;
extern int g_configDamageVignetteIntensity;

ModResult init_oxygen_vignette(const GfxService* gfx_svc, const ResourceService* res_svc,
                               const LogService* log_svc, ModContext* mod_ctx, ModError* error);
void update_oxygen_vignette(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_oxygen_vignette();

void oxygen_vignette_request_preview();
