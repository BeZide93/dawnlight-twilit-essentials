#include "quick_access_internal.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_select_cursor.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>

#include <cmath>
#include <cstdio>

static const f32 BOX_HALF = 20.0f;
static const f32 BOX_SPACING = 56.0f;
static const f32 BAR_Y = 60.0f;

static f32 s_stripScale[QA_QUICK_SLOTS] = { 1.0f, 1.0f, 1.0f, 1.0f };

static f32 s_stripCursorAlpha = 0.0f;
static f32 s_stripCursorX = 0.0f;
static f32 s_stripCursorY = 0.0f;
static bool s_stripCursorRequested = false;

void quick_access_strip_cursor_reset() {
    s_stripCursorAlpha = 0.0f;
    s_stripCursorRequested = false;
}

void quick_access_strip_cursor_request(f32 cx, f32 cy) {
    s_stripCursorX = cx;
    s_stripCursorY = cy;
    s_stripCursorRequested = true;
}

void quick_access_strip_cursor_present() {
    const bool requested = s_stripCursorRequested;
    s_stripCursorRequested = false;

    const f32 target = requested ? s_menuAlpha : 0.0f;
    const f32 ease = target > s_stripCursorAlpha ? 0.35f : 0.22f;
    s_stripCursorAlpha += (target - s_stripCursorAlpha) * ease;
    if (s_stripCursorAlpha < 0.008f) {
        s_stripCursorAlpha = 0.0f;
    }
    if (s_stripCursorAlpha <= 0.0f) {
        return;
    }

    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (cursor == nullptr) {
        return;
    }
    cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    cursor->setPos(s_stripCursorX, s_stripCursorY);
    cursor->setAlphaRate(s_stripCursorAlpha);
    cursor->draw();
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port != nullptr) {
        port->setup2D();
    }
}

void quick_access_strip_reset() {
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        s_stripScale[i] = 1.0f;
    }
    quick_access_strip_cursor_reset();
}

void quick_access_strip_reset_selection() {
    s_selectedSlot = 0;

    u8 assigned = qa_strip_assigned_item();
    if (assigned != QA_ITEM_NONE) {
        for (int i = 0; i < QA_QUICK_SLOTS; i++) {
            if (qa_custom_item(i) == assigned) {
                s_selectedSlot = i;
                return;
            }
        }
    }
}

bool quick_access_strip_cycle(int dir) {
    s_selectedSlot = (s_selectedSlot + dir + QA_QUICK_SLOTS) % QA_QUICK_SLOTS;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    return true;
}

static bool item_state_badge(u8 itemNo) {
    if (itemNo == dItemNo_HVY_BOOTS_e && qa_load_boots_worn()) {
        return true;
    }
    if ((itemNo == dItemNo_KANTERA_e || itemNo == dItemNo_KANTERA2_e) && qa_is_lantern_active()) {
        return true;
    }
    return false;
}

void quick_access_strip_draw(f32 screenW, f32 screenH, u8 alpha, f32 glow) {
    (void)glow;
    qa_hud_scale_begin(screenW * 0.5f, 0.0f);
    const f32 centerX = screenW * 0.5f;
    const f32 centerY = BAR_Y;

    const f32 slide = -(1.0f - s_menuAlpha) * 16.0f;

    const f32 totalSpan = (QA_QUICK_SLOTS - 1) * BOX_SPACING;
    qa_draw_msg_window(centerX - 140.0f, 34.0f + slide, 280.0f, 52.0f, s_menuAlpha);

    const u8 slotAlpha = static_cast<u8>(s_menuAlpha * 255.0f);

    u8 assigned = qa_strip_assigned_item();

    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        const u8 itemNo = qa_custom_item(i);
        const bool available = (itemNo != QA_ITEM_NONE) && qa_is_item_available(itemNo);

        const bool isSelected = (s_selectedSlot == i);

        s_stripScale[i] += ((isSelected ? 1.16f : 1.0f) - s_stripScale[i]) * 0.28f;
        const f32 half = BOX_HALF * s_stripScale[i];

        const f32 cx = centerX - totalSpan * 0.5f + static_cast<f32>(i) * BOX_SPACING;
        const f32 cy = centerY + slide;

        qa_draw_collection_slot(cx, cy, half * 2.0f, slotAlpha, isSelected,
                                available && assigned == itemNo);

        J2DPicture* pic = nullptr;
        ResTIMG* img = nullptr;
        J2DPicture* pic2 = nullptr;
        if (qa_get_item_icon(itemNo, &pic, &img, &pic2)) {
            f32 targetW = 32.0f * s_stripScale[i];
            f32 targetH = 32.0f * s_stripScale[i];
            if (img != nullptr && img->width > 0 && img->height > 0 && img->width != img->height) {
                if (img->width > img->height) {
                    targetH = targetW * (static_cast<f32>(img->height) / static_cast<f32>(img->width));
                } else {
                    targetW = targetH * (static_cast<f32>(img->width) / static_cast<f32>(img->height));
                }
            }
            const f32 iconX = cx - targetW * 0.5f;
            const f32 iconY = cy - targetH * 0.5f;
            pic->setAlpha(slotAlpha);
            pic->draw(iconX, iconY, targetW, targetH, false, false, false);
            if (pic2 != nullptr) {
                pic2->setAlpha(slotAlpha);
                pic2->draw(iconX, iconY, targetW, targetH, false, false, false);
            }

            if (available) {
                qa_draw_item_ammo(itemNo, iconX, iconY, targetW, targetH, slotAlpha);
            }

            if (item_state_badge(itemNo)) {
                qa_draw_solid_disc(cx + half - 6.0f, cy - half + 6.0f, 4.0f,
                                   JUtility::TColor(255, 190, 45, slotAlpha), 24);
            }
        }
    }

    if (s_selectedSlot == SLOT_NONE) {
        s_selectedSlot = 0;
    }
    quick_access_strip_cursor_request(
        centerX - totalSpan * 0.5f + static_cast<f32>(s_selectedSlot) * BOX_SPACING,
        centerY + slide);

    if (s_selectedSlot != SLOT_NONE) {
        const u8 sel = qa_custom_item(s_selectedSlot);
        if (sel != QA_ITEM_NONE && qa_is_item_available(sel)) {
            char labelBuf[64] = "";
            qa_item_label(sel, labelBuf, sizeof(labelBuf));
            if (labelBuf[0] != '\0') {
                const f32 fontW = 9.0f;
                const f32 fontH = 11.5f;
                f32 textW = qa_get_text_width(labelBuf, fontW);
                const f32 textX = centerX - textW * 0.5f;
                const f32 textY = BAR_Y - BOX_HALF - 22.0f + slide;

                JUtility::TColor creamTop(255, 248, 210, alpha);
                JUtility::TColor goldBot(235, 185, 65, alpha);
                qa_draw_text(labelBuf, textX, textY + 10.0f, fontW, fontH, creamTop, goldBot, alpha);
            }
        }
    }

    quick_access_strip_cursor_present();
    qa_hud_scale_end();
    qa_hud_scale_begin(screenW * 0.5f, screenH);

    const f32 hintY = screenH - 40.0f;
    const bool iconsReady = qa_hint_button_ready();
    const char* hintText = iconsReady ? "Customize" : "X Customize";
    const f32 hintFontW = 8.0f;
    const f32 hintFontH = 10.0f;
    const f32 hintIconH = 17.0f;
    const f32 hintIconW = iconsReady ? qa_hint_button_width(2, hintIconH) : 0.0f;
    const f32 hintTextW = qa_get_text_width(hintText, hintFontW);
    const f32 hintGap = 5.0f;
    const f32 hintTotal = hintTextW + (iconsReady ? hintIconW + hintGap : 0.0f);
    f32 hintX = centerX - hintTotal * 0.5f;
    qa_pointer_set_menu_hint(hintX, hintX + hintTotal, hintY, centerX, screenH);
    if (iconsReady) {
        qa_draw_hint_button(2, hintX, hintY - 12.0f, hintIconH, alpha);
        hintX += hintIconW + hintGap;
    }
    qa_draw_text(hintText, hintX, hintY, hintFontW, hintFontH,
                 JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                 alpha);
    qa_hud_scale_end();
}
