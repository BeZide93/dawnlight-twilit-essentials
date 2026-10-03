#pragma once

#include "mods/api.h"
#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

ModResult init_bullet_time_vignette(const GfxService* gfx_svc, const ResourceService* res_svc,
                                    const LogService* log_svc, ModContext* mod_ctx, ModError* error);
void shutdown_bullet_time_vignette();
