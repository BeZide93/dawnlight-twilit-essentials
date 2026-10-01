#include "stamina_kantera.hpp"
#include "../compat/lazy_tweaks.hpp"
#include "../compat/twilight_hd.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "m_Do/m_Do_graphic.h"

#define private public
#define protected public
#include "d/d_meter2_draw.h"
#undef private
#undef protected

#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"

#include <cmath>

void qa_hud_scale_begin(f32 anchorX, f32 anchorY);
void qa_hud_scale_end();

static f32 s_stackShift = 0.0f;
static f32 s_hdStackShift = 0.0f;
static f32 s_hdFrameHeight = 16.0f;
static f32 s_hdBottom = 0.0f;

static bool s_lanternGaugeValid = false;
static f32 s_lanternGaugeBottom = 0.0f;
static bool s_staminaGaugeValid = false;
static f32 s_staminaGaugeBottom = 0.0f;

static bool s_screenHookSeen = false;
static bool s_screenSkillValid = false;
static f32 s_screenSkillBottom = 0.0f;
static bool s_screenLanternValid = false;
static f32 s_screenLanternBottom = 0.0f;
static bool s_screenOxygenValid = false;
static f32 s_screenOxygenTX = 0.0f;
static f32 s_screenOxygenTY = 0.0f;
static f32 s_screenOxygenSX = 1.0f;
static f32 s_screenOxygenSY = 1.0f;

f32 stamina_twilight_hd_bottom() {
    return s_hdBottom;
}

f32 stamina_twilight_hd_frame_height() {
    return s_hdFrameHeight;
}

static void place_stamina_twilight_hd(dMeter2Draw_c* draw, J2DPane* parentPane) {
    f32 left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    if (!twilight_hd_meter_frame_bounds(draw->mpKanteraScreen, left, top, right, bottom)) {
        return;
    }
    s_hdFrameHeight = bottom - top;
    const f32 centerX = (mDoGph_gInf_c::getSafeMinXF() + mDoGph_gInf_c::getSafeMaxXF()) * 0.5f;
    const f32 centerY = twilight_hd_top_meter_center_y() + s_hdStackShift;
    parentPane->translate(parentPane->getTranslateX() + centerX - (left + right) * 0.5f,
                          parentPane->getTranslateY() + centerY - (top + bottom) * 0.5f);
    s_hdBottom = centerY + s_hdFrameHeight * 0.5f;
}

static bool kantera_gauge_bottom(dMeter2Draw_c* draw, f32& bottom) {
    CPaneMgr* parts[] = {draw->mpMagicFrameL, draw->mpMagicFrameR, draw->mpMagicBase, draw->mpMagicMeter};
    CPaneMgr mgr;
    Mtx mtx;
    bool any = false;
    for (CPaneMgr* part : parts) {
        J2DPane* pane = part != nullptr ? part->getPanePtr() : nullptr;
        if (pane == nullptr) continue;
        for (u8 i = 0; i < 4; i++) {
            const Vec v = mgr.getGlobalVtx(pane, &mtx, i, false, 0);
            if (!any || v.y > bottom) bottom = v.y;
            any = true;
        }
    }
    return any;
}

bool stamina_hud_gauges_bottom(dMeter2Draw_c* draw, f32& bottom) {
    if (draw == nullptr || twilight_hd_enabled()) return false;
    const bool staminaInHud = !stamina_hud_radial_style();
    bool any = false;
    if (s_lanternGaugeValid && draw->getMeterGaugeAlphaRate(1) > 0.02f) {
        bottom = s_lanternGaugeBottom;
        any = true;
    }
    if (staminaInHud && s_staminaGaugeValid && stamina_bar_alpha() > 0.01f) {
        if (!any || s_staminaGaugeBottom > bottom) bottom = s_staminaGaugeBottom;
        any = true;
    }
    if (lazy_tweaks_build() && s_screenSkillValid) {
        if (!any || s_screenSkillBottom > bottom) bottom = s_screenSkillBottom;
        any = true;
    }
    return any;
}

void stamina_kantera_screen_post(dMeter2Draw_c* draw, u8 meterType) {
    if (draw == nullptr || !lazy_tweaks_build()) {
        return;
    }
    s_screenHookSeen = true;
    if (meterType == 0) {
        s_screenSkillValid = !twilight_hd_enabled() && draw->mMeterAlphaRate[0] > 0.02f &&
                             kantera_gauge_bottom(draw, s_screenSkillBottom);
    } else if (meterType == 1) {
        s_screenLanternValid = !twilight_hd_enabled() && draw->getMeterGaugeAlphaRate(1) > 0.02f &&
                               kantera_gauge_bottom(draw, s_screenLanternBottom);
    } else if (meterType == 2) {
        J2DPane* parentPane = draw->mpMagicParent != nullptr ? draw->mpMagicParent->getPanePtr() : nullptr;
        s_screenOxygenValid = parentPane != nullptr;
        if (s_screenOxygenValid) {
            s_screenOxygenTX = parentPane->getTranslateX();
            s_screenOxygenTY = parentPane->getTranslateY();
            s_screenOxygenSX = parentPane->getScaleX();
            s_screenOxygenSY = parentPane->getScaleY();
        }
    }
}

void stamina_kantera_begin_draw(dMeter2Draw_c* draw) {
    s_hdBottom = 0.0f;
    if (lazy_tweaks_build() && s_screenHookSeen) {
        if (s_screenLanternValid) {
            s_lanternGaugeBottom = s_screenLanternBottom;
            s_lanternGaugeValid = true;
        }
    } else if (!twilight_hd_enabled() && draw->getMeterGaugeAlphaRate(1) > 0.02f &&
        kantera_gauge_bottom(draw, s_lanternGaugeBottom)) {
        s_lanternGaugeValid = true;
    }
    const f32 hdStackTarget = twilight_hd_gauge_visible(draw) ? s_hdFrameHeight + 4.0f : 0.0f;
    s_hdStackShift += (hdStackTarget - s_hdStackShift) * 0.15f;
}

static void draw_stamina_meter(dMeter2Draw_c* draw, const StaminaHudFrame& frame) {
    CPaneMgr* meter = draw->mpMagicMeter;
    CPaneMgr* base = draw->mpMagicBase;
    CPaneMgr* frameL = draw->mpMagicFrameL;
    CPaneMgr* frameR = draw->mpMagicFrameR;
    CPaneMgr* parent = draw->mpMagicParent;
    if (!meter || !base || !frameL || !frameR || !parent) return;

    const f32 a = frame.alpha;
    const f32 fill01 = frame.max > 0.0f ? frame.value / frame.max : 0.0f;
    const f32 span = frameR->getInitPosX() - frameL->getInitPosX();

    JUtility::TColor hi(170, 255, 150, 255);
    JUtility::TColor lo(28, 158, 54, 255);
    f32 drain = 1.0f - fill01;
    drain = drain * drain * (3.0f - 2.0f * drain);
    hi = stamina_hud_lerp(hi, JUtility::TColor(255, 110, 20, 255), drain);
    lo = stamina_hud_lerp(lo, JUtility::TColor(150, 45, 5, 255), drain);
    hi = stamina_hud_lerp(hi, JUtility::TColor(224, 255, 214, 255), frame.pulse * 0.6f);
    lo = stamina_hud_lerp(lo, JUtility::TColor(120, 224, 128, 255), frame.pulse * 0.6f);
    const f32 breathe = 0.5f + 0.5f * std::sin(static_cast<f32>(frame.exhaustPhase) * 0.12f);
    hi = stamina_hud_lerp(hi, stamina_hud_lerp(JUtility::TColor(200, 40, 30, 255), JUtility::TColor(255, 96, 70, 255), breathe), frame.exhaustBlend);
    lo = stamina_hud_lerp(lo, stamina_hud_lerp(JUtility::TColor(90, 10, 8, 255), JUtility::TColor(150, 24, 16, 255), breathe), frame.exhaustBlend);
    hi = stamina_hud_lerp(hi, JUtility::TColor(255, 170, 120, 255), frame.emptyFlash);
    lo = stamina_hud_lerp(lo, JUtility::TColor(206, 40, 30, 255), frame.emptyFlash);

    meter->setBlackWhite(hi, lo);
    meter->resize(fill01 * meter->getInitSizeX(), meter->getInitSizeY());
    frameR->move(span + frameL->getInitPosX(), frameL->getInitPosY());
    base->resize(base->getInitSizeX(), base->getInitSizeY());

    parent->setAlphaRate(a);
    meter->setAlphaRate(a * g_drawHIO.mLanternMeterAlpha);
    frameL->setAlphaRate(a * g_drawHIO.mLanternMeterFrameAlpha);
    frameR->setAlphaRate(a * g_drawHIO.mLanternMeterFrameAlpha);

    J2DPane* basePane = parent->getPanePtr();
    const bool oxygenBase = lazy_tweaks_build() && s_screenOxygenValid && basePane != nullptr;
    const f32 engineTX = oxygenBase ? basePane->getTranslateX() : 0.0f;
    const f32 engineTY = oxygenBase ? basePane->getTranslateY() : 0.0f;
    const f32 engineSX = oxygenBase ? basePane->getScaleX() : 1.0f;
    const f32 engineSY = oxygenBase ? basePane->getScaleY() : 1.0f;
    if (oxygenBase) {
        basePane->scale(s_screenOxygenSX, s_screenOxygenSY);
        basePane->translate(s_screenOxygenTX, s_screenOxygenTY);
    }

    const f32 origTX = parent->getTranslateX();
    const f32 origTY = parent->getTranslateY();
    J2DPane* parentPane = parent->getPanePtr();
    const bool twilightHd = twilight_hd_enabled() && parentPane != nullptr;
    const f32 origSX = twilightHd ? parentPane->getScaleX() : 1.0f;
    const f32 origSY = twilightHd ? parentPane->getScaleY() : 1.0f;
    if (twilightHd) {
        const f32 hdScale = twilight_hd_overall_scale();
        parentPane->scale(origSX * hdScale, origSY * hdScale);
        parent->translate(origTX, origTY);
        place_stamina_twilight_hd(draw, parentPane);
        parentPane->translate(parentPane->getTranslateX() + g_configStaminaBarX,
                              parentPane->getTranslateY() + g_configStaminaBarY);
    } else {
        parent->translate(origTX + g_configStaminaBarX, origTY + s_stackShift + g_configStaminaBarY);
    }

    J2DGrafContext* graf = dComIfGp_getCurrentGrafPort();
    if (graf) graf->setup2D();
    constexpr f32 kStaminaScaleAnchorBlendX = 0.25f;
    constexpr f32 kStaminaScaleAnchorBlendY = 0.55f;
    static f32 s_drawnX = 0.0f, s_drawnY = 0.0f;
    static bool s_drawnMeasured = false;
    if (!s_drawnMeasured) {
        s_drawnX = frameL->getInitPosX();
        s_drawnY = frameL->getInitPosY();
        s_drawnMeasured = true;
    }
    if (twilightHd) {
        qa_hud_scale_begin((mDoGph_gInf_c::getSafeMinXF() + mDoGph_gInf_c::getSafeMaxXF()) * 0.5f,
                           mDoGph_gInf_c::getSafeMinYF());
    } else {
        qa_hud_scale_begin(frameL->getInitPosX() + kStaminaScaleAnchorBlendX * (s_drawnX - frameL->getInitPosX()),
                           frameL->getInitPosY() + kStaminaScaleAnchorBlendY * (s_drawnY - frameL->getInitPosY()));
    }
    draw->mpKanteraScreen->draw(0.0f, 0.0f, graf);
    qa_hud_scale_end();

    if (!twilightHd && kantera_gauge_bottom(draw, s_staminaGaugeBottom)) {
        s_staminaGaugeValid = true;
    }

    if (!twilightHd) {
        const JGeometry::TBox2<f32>& drawn = frameL->getPanePtr()->getGlbBounds();
        s_drawnX = drawn.i.x;
        s_drawnY = drawn.i.y;
    } else {
        parentPane->scale(origSX, origSY);
    }

    parent->translate(origTX, origTY);
    if (oxygenBase) {
        basePane->scale(engineSX, engineSY);
        basePane->translate(engineTX, engineTY);
    }
}

void stamina_kantera_draw(dMeter2Draw_c* draw, const StaminaHudFrame& frame) {
    if (!frame.preview) {
        const f32 stackTarget =
            (!twilight_hd_enabled() && draw->getMeterGaugeAlphaRate(1) > 0.02f) ? 16.0f : 0.0f;
        s_stackShift += (stackTarget - s_stackShift) * 0.15f;
    }
    draw_stamina_meter(draw, frame);
}

void stamina_kantera_shutdown() {
    s_stackShift = 0.0f;
}
