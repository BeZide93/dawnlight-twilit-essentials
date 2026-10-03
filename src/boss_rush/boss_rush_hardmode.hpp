#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/log.h"

class fopAc_ac_c;

void init_boss_rush_hardmode(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_rush_hardmode();
void update_boss_rush_hardmode();
void draw_boss_rush_hardmode_embers();

void boss_rush_hardmode_bind_config(const ConfigService* config_svc, ModContext* mod_ctx,
                                    ConfigVarHandle handle);
void boss_rush_hardmode_set_enabled(bool enabled);
void boss_rush_hardmode_toggle();

bool boss_rush_hardmode_enabled();
bool boss_rush_hardmode_active();
bool boss_rush_hardmode_service_available();
float boss_rush_hardmode_health_scale(const fopAc_ac_c* actor);
