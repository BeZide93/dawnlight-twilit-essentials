#pragma once

#include "stamina_hud.hpp"

void stamina_radial_init(const HookService* hook_svc);
void stamina_radial_capture_anchor();
void stamina_radial_draw(const StaminaHudFrame& frame);
void stamina_radial_shutdown();
