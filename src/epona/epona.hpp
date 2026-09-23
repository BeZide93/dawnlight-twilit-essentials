#pragma once

#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/ui.h"

extern bool g_configEponaEnabled;

extern ConfigVarHandle g_varEponaEnabled;
extern ConfigVarHandle g_varEponaTurnRatePct;
extern ConfigVarHandle g_varEponaTopSpeedPct;
extern ConfigVarHandle g_varEponaUnlimitedSpurs;
extern ConfigVarHandle g_varEponaAutoGallop;

extern UiElementHandle g_eponaStatusText;

ModResult init_epona_config(const ConfigService* config_svc, ModContext* mod_ctx);
ModResult init_epona(const HookService* hook_svc, const UiService* ui_svc, ModError* error);
void update_epona_status_text();
void shutdown_epona();
