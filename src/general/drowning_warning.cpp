#include "drowning_warning.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_s_play.h"
#include "m_Do/m_Do_graphic.h"

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"
#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>

#include <cmath>

bool g_configDrowningWarningEnabled = false;

DEFINE_HOOK(&dMeter2Draw_c::draw, DrownWarnMeterDrawHook);

static constexpr f32 kWarnBelow = 0.50f;
static constexpr f32 kBandWidth = 11.0f;
static constexpr int kLayers = 5;
static constexpr f32 kLayerAlpha[kLayers] = {0.52f, 0.38f, 0.26f, 0.16f, 0.08f};

static f32 s_intensity = 0.0f;
static f32 s_pulsePhase = 0.0f;

static bool in_gameplay() {
    if (dComIfGp_getPlayer(0) == nullptr) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    return true;
}

void update_drowning_warning() {
    if (!g_configDrowningWarningEnabled || !in_gameplay()) {
        s_intensity = 0.0f;
        s_pulsePhase = 0.0f;
        return;
    }

    const int oxygen = dComIfGp_getOxygen();
    const s32 maxOxygen = dComIfGp_getMaxOxygen();

    f32 target = 0.0f;
    if (maxOxygen > 0 && oxygen < maxOxygen) {
        const f32 ratio = static_cast<f32>(oxygen) / static_cast<f32>(maxOxygen);
        if (ratio < 1.0f) {
            if (ratio < kWarnBelow) {
                target = (kWarnBelow - ratio) / kWarnBelow;
            } else {
                target = (1.0f - ratio) / (1.0f - kWarnBelow) * 0.25f;
            }
        }
        if (target > 1.0f) target = 1.0f;
    }

    s_intensity += (target - s_intensity) * (target > s_intensity ? 0.14f : 0.10f);
    if (s_intensity < 0.001f) s_intensity = 0.0f;
    if (s_intensity > 1.0f) s_intensity = 1.0f;

    const f32 urgency = target;
    s_pulsePhase += 0.0314f + urgency * 0.0942f;
    if (s_pulsePhase > 6.2831853f) s_pulsePhase -= 6.2831853f;
}

static void draw_band(f32 x, f32 y, f32 w, f32 h, u8 alpha) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x, y, 0.0f);
    GXColor1u32(JUtility::TColor(64, 148, 255, alpha));
    GXPosition3f32(x + w, y, 0.0f);
    GXColor1u32(JUtility::TColor(64, 148, 255, alpha));
    GXPosition3f32(x + w, y + h, 0.0f);
    GXColor1u32(JUtility::TColor(64, 148, 255, alpha));
    GXPosition3f32(x, y + h, 0.0f);
    GXColor1u32(JUtility::TColor(64, 148, 255, alpha));
    GXEnd();
}

static void on_meter_draw_post(ModContext*, void*, void*, void*) {
    if (!g_configDrowningWarningEnabled || s_intensity <= 0.01f) return;
    if (!in_gameplay()) return;

    f32 screenW = mDoGph_gInf_c::getWidthF();
    f32 screenH = mDoGph_gInf_c::getHeightF();
    if (screenW <= 0.0f) screenW = 640.0f;
    if (screenH <= 0.0f) screenH = 480.0f;

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) ctx->setup2D();
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);

    const f32 wave = 0.55f + 0.45f * std::sin(s_pulsePhase * 6.2831853f);
    const f32 strength = s_intensity * (0.45f + 0.55f * wave);

    for (int i = 0; i < kLayers; i++) {
        const f32 inset = static_cast<f32>(i) * kBandWidth;
        const f32 band = kBandWidth + 1.0f;
        const u8 alpha = static_cast<u8>(strength * kLayerAlpha[i] * 255.0f);
        if (alpha == 0) continue;

        draw_band(0.0f, inset, screenW, band, alpha);
        draw_band(0.0f, screenH - inset - band, screenW, band, alpha);
        draw_band(inset, 0.0f, band, screenH, alpha);
        draw_band(screenW - inset - band, 0.0f, band, screenH, alpha);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

ModResult init_drowning_warning(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    return mods::hook::add_post<DrownWarnMeterDrawHook>(hook_svc, on_meter_draw_post);
}

void shutdown_drowning_warning() {
    s_intensity = 0.0f;
    s_pulsePhase = 0.0f;
}
