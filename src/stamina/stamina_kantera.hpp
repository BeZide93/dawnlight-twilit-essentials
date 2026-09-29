#pragma once

#include "stamina_hud.hpp"

class dMeter2Draw_c;

void stamina_kantera_begin_draw(dMeter2Draw_c* draw);
void stamina_kantera_draw(dMeter2Draw_c* draw, const StaminaHudFrame& frame);
void stamina_kantera_shutdown();
