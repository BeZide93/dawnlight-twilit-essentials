#include "stamina_radial.hpp"
#include "stamina_internal.hpp"
#include "../interp.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"

#include "JSystem/J2DGraph/J2DGrafContext.h"

#include <cmath>

void qa_hud_scale_begin(f32 anchorX, f32 anchorY);
void qa_hud_scale_end();

DEFINE_HOOK(&daAlink_c::execute, StaminaWheelAlinkExecute);

static constexpr f32 kWheelAnchorHumanY = 150.0f;
static constexpr f32 kWheelAnchorWolfY = 90.0f;

static constexpr int kWheelMaxRings = stamina_impl::kRingMax;
static constexpr f32 kWheelScale = 0.8f;
static constexpr f32 kWheelHoleR = 3.75f * kWheelScale;
static constexpr f32 kWheelMainR = 12.0f * kWheelScale;
static constexpr f32 kWheelRingGap = 1.75f * kWheelScale;
static constexpr f32 kWheelRingWidth[] = {2.0f * kWheelScale, 1.5f * kWheelScale};
static constexpr f32 kWheelOutline = 1.0f * kWheelScale;
static constexpr f32 kWheelSideGap = 3.0f * kWheelScale;
static constexpr f32 kWheelSideWorld = 32.0f;
static constexpr f32 kWheelScreenMargin = 6.0f;
static constexpr f32 kWheelTeleportDist = 250.0f;
static constexpr f32 kTwoPi = 6.2831853f;

static InterpPos s_wheelAnchor;

static bool wheel_anchor_world(cXyz& out) {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    if (player == nullptr) return false;
    out = player->current.pos;
    out.y += player->checkWolf() ? kWheelAnchorWolfY : kWheelAnchorHumanY;
    return true;
}

static void on_wheel_alink_execute_post(ModContext*, void*, void*, void*) {
    cXyz anchor;
    if (wheel_anchor_world(anchor)) s_wheelAnchor.record(anchor);
}

struct WheelRing {
    f32 r0;
    f32 r1;
    f32 base;
    f32 cap;
    f32 full;
};

static int wheel_layout(f32 maxValue, WheelRing* rings) {
    const f32 mainFull = stamina_impl::main_ring_capacity(maxValue);
    f32 outerFull = mainFull;
    if (g_configStaminaScaleWithHearts) {
        outerFull = (stamina_impl::kScaleMaxHearts - stamina_impl::kScaleMinHearts) *
                    static_cast<f32>(g_configStaminaPerHeart) / static_cast<f32>(kWheelMaxRings - 1);
    }
    int count = 0;
    f32 base = 0.0f;
    for (int i = 0; i < kWheelMaxRings; i++) {
        const f32 full = i == 0 ? mainFull : outerFull;
        if (full <= 0.0f) break;
        f32 cap = maxValue - base;
        if (cap > full) cap = full;
        if (cap < 0.5f) break;
        WheelRing& r = rings[count++];
        r.base = base;
        r.cap = cap;
        r.full = full;
        base += full;
        if (i == 0) {
            r.r0 = kWheelHoleR;
            r.r1 = kWheelMainR;
        } else {
            r.r0 = rings[count - 2].r1 + kWheelRingGap;
            r.r1 = r.r0 + kWheelRingWidth[i - 1];
        }
    }
    return count;
}

static JUtility::TColor with_alpha(JUtility::TColor c, f32 a) {
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    c.a = static_cast<u8>(a * 255.0f);
    return c;
}

static void wheel_arc(f32 cx, f32 cy, f32 r0, f32 r1, f32 a0, f32 a1,
                      JUtility::TColor inner, JUtility::TColor outer) {
    if (a1 - a0 < 0.001f || (inner.a == 0 && outer.a == 0)) return;
    int segs = static_cast<int>(std::ceil((a1 - a0) * (96.0f / kTwoPi)));
    if (segs < 2) segs = 2;
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (segs + 1) * 2);
    for (int i = 0; i <= segs; i++) {
        const f32 ang = a0 + (a1 - a0) * static_cast<f32>(i) / static_cast<f32>(segs);
        const f32 dx = std::sin(ang);
        const f32 dy = -std::cos(ang);
        GXPosition3f32(cx + dx * r1, cy + dy * r1, 0.0f);
        GXColor1u32(outer);
        GXPosition3f32(cx + dx * r0, cy + dy * r0, 0.0f);
        GXColor1u32(inner);
    }
    GXEnd();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

static bool wheel_link_screen_pos(f32& sx, f32& sy, f32& side) {
    cXyz live;
    view_class* view = dComIfGd_getView();
    if (!wheel_anchor_world(live) || view == nullptr) return false;
    cXyz pos = live;
    cXyz smooth;
    if (s_wheelAnchor.lookup(smooth) &&
        smooth.abs2(live) < kWheelTeleportDist * kWheelTeleportDist) {
        pos = smooth;
    }
    Vec cam;
    mDoLib_pos2camera(&pos, &cam);
    if (cam.z > -1.0f) return false;
    Vec screen;
    mDoLib_project(&pos, &screen);
    cXyz sidePos(pos.x + view->viewMtx[0][0] * kWheelSideWorld,
                 pos.y + view->viewMtx[0][1] * kWheelSideWorld,
                 pos.z + view->viewMtx[0][2] * kWheelSideWorld);
    Vec sideScreen;
    mDoLib_project(&sidePos, &sideScreen);
    sx = screen.x;
    sy = screen.y;
    side = std::fabs(sideScreen.x - screen.x);
    return true;
}

void stamina_radial_draw(const StaminaHudFrame& frame) {
    WheelRing rings[kWheelMaxRings];
    const int ringCount = wheel_layout(frame.max, rings);
    if (ringCount <= 0) return;
    const f32 a = frame.alpha;
    const f32 outerR = rings[ringCount - 1].r1 + kWheelOutline;

    const f32 minX = mDoGph_gInf_c::getSafeMinXF();
    const f32 maxX = mDoGph_gInf_c::getSafeMaxXF();
    const f32 minY = mDoGph_gInf_c::getSafeMinYF();
    const f32 maxY = mDoGph_gInf_c::getSafeMaxYF();

    f32 cx = 0.0f, cy = 0.0f;
    f32 side = 0.0f;
    if (wheel_link_screen_pos(cx, cy, side)) {
        cx += side + kWheelSideGap + outerR;
    } else {
        cx = maxX - 80.0f;
        cy = (minY + maxY) * 0.5f;
    }
    cx += g_configStaminaBarX;
    cy += g_configStaminaBarY;
    const f32 lim = outerR + kWheelScreenMargin;
    if (cx < minX + lim) cx = minX + lim;
    if (cx > maxX - lim) cx = maxX - lim;
    if (cy < minY + lim) cy = minY + lim;
    if (cy > maxY - lim) cy = maxY - lim;

    JUtility::TColor hi(150, 238, 92, 255);
    JUtility::TColor lo(62, 178, 48, 255);
    hi = stamina_hud_lerp(hi, JUtility::TColor(225, 255, 205, 255), frame.pulse * 0.5f);
    lo = stamina_hud_lerp(lo, JUtility::TColor(150, 230, 130, 255), frame.pulse * 0.5f);
    const f32 breathe = 0.5f + 0.5f * std::sin(static_cast<f32>(frame.exhaustPhase) * 0.12f);
    hi = stamina_hud_lerp(hi, stamina_hud_lerp(JUtility::TColor(205, 45, 30, 255), JUtility::TColor(255, 105, 70, 255), breathe), frame.exhaustBlend);
    lo = stamina_hud_lerp(lo, stamina_hud_lerp(JUtility::TColor(120, 14, 8, 255), JUtility::TColor(175, 32, 20, 255), breathe), frame.exhaustBlend);
    hi = stamina_hud_lerp(hi, JUtility::TColor(255, 170, 120, 255), frame.emptyFlash);
    lo = stamina_hud_lerp(lo, JUtility::TColor(206, 40, 30, 255), frame.emptyFlash);
    hi = with_alpha(hi, a);
    lo = with_alpha(lo, a);
    const JUtility::TColor trailHi = with_alpha(JUtility::TColor(255, 96, 60, 255), a);
    const JUtility::TColor trailLo = with_alpha(JUtility::TColor(200, 40, 24, 255), a);
    const JUtility::TColor outline = with_alpha(JUtility::TColor(0, 0, 0, 255), 0.45f * a);
    const JUtility::TColor track = with_alpha(JUtility::TColor(40, 46, 40, 255), 0.4f * a);

    J2DGrafContext* graf = dComIfGp_getCurrentGrafPort();
    qa_hud_scale_begin(cx, cy);
    if (graf) graf->setup2D();

    for (int i = 0; i < ringCount; i++) {
        const WheelRing& r = rings[i];
        const f32 span = r.cap / r.full * kTwoPi;
        const bool full = span >= kTwoPi - 0.001f;
        f32 fillV = frame.value - r.base;
        if (fillV < 0.0f) fillV = 0.0f;
        if (fillV > r.cap) fillV = r.cap;
        f32 trailV = frame.trail - r.base;
        if (trailV < 0.0f) trailV = 0.0f;
        if (trailV > r.cap) trailV = r.cap;
        const f32 fillA = fillV / r.full * kTwoPi;
        const f32 trailA = trailV / r.full * kTwoPi;
        const f32 ext = full ? 0.0f : kWheelOutline / ((r.r0 + r.r1) * 0.5f);

        wheel_arc(cx, cy, r.r0 - kWheelOutline, r.r1 + kWheelOutline, -ext, span + ext, outline, outline);
        wheel_arc(cx, cy, r.r0, r.r1, 0.0f, span, track, track);
        if (trailA > fillA) wheel_arc(cx, cy, r.r0, r.r1, fillA, trailA, trailLo, trailHi);
        wheel_arc(cx, cy, r.r0, r.r1, 0.0f, fillA, lo, hi);
    }

    qa_hud_scale_end();
}

void stamina_radial_init(const HookService* hook_svc) {
    if (hook_svc == nullptr) return;
    mods::hook::add_post<StaminaWheelAlinkExecute>(hook_svc, on_wheel_alink_execute_post);
}

void stamina_radial_shutdown() {
    s_wheelAnchor.forget();
}
