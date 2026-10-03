#include "text_outline.hpp"

#include "JSystem/JUtility/JUTFont.h"

#include <cmath>

namespace {

constexpr int kOuterTaps = 12;
constexpr int kInnerTaps = 6;
constexpr f32 kInnerScale = 0.55f;
constexpr f32 kOverlap = 3.0f;

struct OutlineTap {
    f32 dx;
    f32 dy;
};

const OutlineTap* outline_taps() {
    static OutlineTap s_taps[kOuterTaps + kInnerTaps];
    static bool s_ready = false;
    if (!s_ready) {
        for (int i = 0; i < kOuterTaps; ++i) {
            const f32 angle = static_cast<f32>(i) * (6.2831853f / kOuterTaps);
            s_taps[i] = {std::cos(angle), std::sin(angle)};
        }
        for (int i = 0; i < kInnerTaps; ++i) {
            const f32 angle = (static_cast<f32>(i) + 0.5f) * (6.2831853f / kInnerTaps);
            s_taps[kOuterTaps + i] = {std::cos(angle) * kInnerScale, std::sin(angle) * kInnerScale};
        }
        s_ready = true;
    }
    return s_taps;
}

}

void draw_text_outline(JUTFont* font, const char* text, f32 x, f32 y, f32 charW, f32 charH,
                       JUtility::TColor color, f32 radius) {
    if (font == nullptr || text == nullptr || color.a == 0 || radius <= 0.0f) return;

    const f32 coverage = static_cast<f32>(color.a) / 255.0f;
    const f32 tapCoverage = 1.0f - std::pow(1.0f - coverage, 1.0f / kOverlap);
    color.a = static_cast<u8>(255.0f * tapCoverage + 0.5f);
    font->setCharColor(color);

    const OutlineTap* taps = outline_taps();
    for (int i = 0; i < kOuterTaps + kInnerTaps; ++i) {
        font->drawString_scale(x + taps[i].dx * radius, y + taps[i].dy * radius, charW, charH, text, true);
    }
}
