#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

extern bool g_configHudAutoFadeEnabled;
extern float g_configHudAutoFadeIdleSeconds;
extern float g_configHudAutoFadeFadeSeconds;
extern float g_configHudAutoFadeRestAlpha;

ModResult init_hud_auto_fade(const HookService* hook_svc, ModError* error);
void update_hud_auto_fade();
void shutdown_hud_auto_fade();
