#include "quick_access_internal.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_select_cursor.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "m_Do/m_Do_ext.h"
#include "mods/svc/resource.h"

#include <dolphin/gx.h>

#include <cmath>

extern const ResourceService* get_resource_service();

static ResourceBuffer s_wolfDayBti = RESOURCE_BUFFER_INIT;
static ResourceBuffer s_wolfNightBti = RESOURCE_BUFFER_INIT;
static J2DPicture* s_wolfDayPic = nullptr;
static J2DPicture* s_wolfNightPic = nullptr;
static bool s_wolfTexTried = false;

void quick_access_wolf_shutdown() {
    JKR_DELETE(s_wolfDayPic);
    JKR_DELETE(s_wolfNightPic);
    s_wolfDayPic = nullptr;
    s_wolfNightPic = nullptr;
    s_wolfTexTried = false;

    const ResourceService* res_svc = get_resource_service();
    ModContext* ctx = qa_mod_ctx();
    if (res_svc != nullptr && ctx != nullptr) {
        res_svc->free(ctx, &s_wolfDayBti);
        res_svc->free(ctx, &s_wolfNightBti);
    }
}

static J2DPicture* make_bti_picture(ResourceBuffer& buf) {
    if (buf.data == nullptr) {
        return nullptr;
    }
    ResTIMG* img = reinterpret_cast<ResTIMG*>(buf.data);
    img->alphaEnabled = 1;

    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
    J2DPicture* pic = JKR_NEW J2DPicture(img);
    if (oldHeap != nullptr) {
        mDoExt_setCurrentHeap(oldHeap);
    }
    return pic;
}

static void load_wolf_textures() {
    if (s_wolfTexTried) {
        return;
    }
    s_wolfTexTried = true;

    const ResourceService* res_svc = get_resource_service();
    ModContext* ctx = qa_mod_ctx();
    if (res_svc == nullptr || ctx == nullptr) {
        return;
    }
    res_svc->load(ctx, "textures/quick_access/day.bti", &s_wolfDayBti);
    res_svc->load(ctx, "textures/quick_access/night.bti", &s_wolfNightBti);
    s_wolfDayPic = make_bti_picture(s_wolfDayBti);
    s_wolfNightPic = make_bti_picture(s_wolfNightBti);
}

static bool qa_wolf_is_night() {
    bool night = dKy_daynight_check() != FALSE;
    if (g_env_light.time_change_rate == 1.0f) {
        night = !night;
    }
    return night;
}

static void draw_wolf_icon(f32 cx, f32 cy, f32 size, u8 alpha) {
    load_wolf_textures();
    J2DPicture* pic = qa_wolf_is_night() ? s_wolfNightPic : s_wolfDayPic;
    if (pic != nullptr) {
        pic->setAlpha(alpha);
        pic->draw(cx - size * 0.5f, cy - size * 0.5f, size, size, false, false, false);
        return;
    }

    if (qa_wolf_is_night()) {
        qa_draw_solid_disc(cx, cy, size * 0.5f, JUtility::TColor(240, 198, 79, alpha), 32);
    } else {
        qa_draw_solid_disc(cx, cy, size * 0.5f, JUtility::TColor(250, 240, 205, alpha), 32);
    }
}

static void draw_wolf_hint(f32 centerX, f32 screenH, u8 alpha) {
    const f32 hintY = screenH - 40.0f;
    const bool iconsReady = qa_hint_button_ready();
    const char* hintText = iconsReady ? "Howl" : "A Howl";
    const f32 hintFontW = 8.0f;
    const f32 hintFontH = 10.0f;
    const f32 hintIconH = 17.0f;
    const f32 hintIconW = iconsReady ? qa_hint_button_width(0, hintIconH) : 0.0f;
    const f32 hintTextW = qa_get_text_width(hintText, hintFontW);
    const f32 hintGap = 5.0f;
    const f32 hintTotal = hintTextW + (iconsReady ? hintIconW + hintGap : 0.0f);
    f32 hintX = centerX - hintTotal * 0.5f;
    if (iconsReady) {
        qa_draw_hint_button(0, hintX, hintY - 12.0f, hintIconH, alpha);
        hintX += hintIconW + hintGap;
    }
    qa_draw_text(hintText, hintX, hintY, hintFontW, hintFontH,
                 JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                 alpha);
}

static void draw_label_centered(const char* text, f32 cx, f32 y, u8 alpha) {
    const f32 fontW = 9.0f;
    const f32 fontH = 11.5f;
    const f32 textW = qa_get_text_width(text, fontW);
    qa_draw_text(text, cx - textW * 0.5f, y, fontW, fontH,
                 JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                 alpha);
}

static void draw_wolf_cursor(f32 cx, f32 cy, u8 alpha) {
    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (cursor == nullptr) {
        return;
    }
    cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    cursor->setPos(cx, cy);
    cursor->setAlphaRate(s_menuAlpha);
    cursor->draw();
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port != nullptr) {
        port->setup2D();
    }
}

void quick_access_wolf_draw(f32 screenW, f32 screenH, u8 alpha, f32 glow) {
    (void)glow;

    const f32 radius = 92.0f;
    f32 centerX = screenW * 0.5f;
    f32 centerY = screenH * 0.5f;
    centerY -= (1.0f - s_menuAlpha) * 16.0f;

    qa_radial_draw_wheel(centerX, centerY, alpha, s_menuAlpha);

    const u8 dimAlpha = static_cast<u8>(alpha * 0.35f);
    qa_draw_collection_slot(centerX, centerY - radius, 40.0f, dimAlpha, false, false);
    qa_draw_collection_slot(centerX - radius, centerY, 40.0f, dimAlpha, false, false);
    qa_draw_collection_slot(centerX + radius, centerY, 40.0f, dimAlpha, false, false);

    const f32 slotY = centerY + radius;
    qa_draw_collection_slot(centerX, slotY, 40.0f, alpha, true, false);
    draw_wolf_icon(centerX, slotY, 32.0f, alpha);
    draw_wolf_cursor(centerX, slotY, alpha);
    draw_label_centered("Sun Song", centerX, slotY + 32.0f, alpha);
    draw_wolf_hint(centerX, screenH, alpha);
}
