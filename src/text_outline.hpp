#pragma once

#include "global.h"
#include "JSystem/JUtility/TColor.h"

class JUTFont;

void draw_text_outline(JUTFont* font, const char* text, f32 x, f32 y, f32 charW, f32 charH,
                       JUtility::TColor color, f32 radius);
