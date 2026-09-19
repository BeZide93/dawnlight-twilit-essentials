#pragma once

#include "JSystem/JUtility/TColor.h"

class daAlink_c;

void draw_boss_rush_texts(float floorY);

f32 boss_rush_texts_measure_width(const char* text, f32 charW);
void boss_rush_texts_draw_label(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                                JUtility::TColor top, JUtility::TColor bottom, u8 alpha);

void boss_rush_texts_reset_fade();

void draw_boss_rush_debug_coords(daAlink_c* link);

void draw_boss_rush_fight_timer();
