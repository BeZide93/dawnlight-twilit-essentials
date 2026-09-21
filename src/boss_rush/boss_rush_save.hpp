#pragma once

#include "global.h"
#include "mods/service.hpp"
#include "mods/svc/log.h"

ModResult init_boss_rush_save(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_boss_rush_save();

bool boss_rush_save_preset_available();

bool boss_rush_save_apply_preset(bool i_refreshLink = true);

void boss_rush_save_apply_equips_to_savedata();

bool boss_rush_save_export_current();
