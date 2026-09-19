#include "quick_access_internal.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_select_cursor.h"
#define private public
#define protected public
#include "d/d_meter2_draw.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#undef protected
#undef private
#include "d/d_pane_class.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <dolphin/gx.h>

#include <cmath>
#include <cstdio>

static J2DScreen* s_wheelScreen = nullptr;
static CPaneMgr* s_wheelCircle = nullptr;
static bool s_wheelLoaded = false;
static int s_wheelRetryTimer = 0;

static f32 s_slotScale[QA_QUICK_SLOTS] = { 1.0f, 1.0f, 1.0f, 1.0f };

static void cleanup_radial_wheel_resources() {
    if (s_wheelScreen != nullptr) {
        JKR_DELETE(s_wheelScreen);
        s_wheelScreen = nullptr;
    }
    if (s_wheelCircle != nullptr) {
        JKR_DELETE(s_wheelCircle);
        s_wheelCircle = nullptr;
    }
    s_wheelLoaded = false;
    s_wheelRetryTimer = 0;
}

static void load_radial_wheel_resources() {
    if (s_wheelLoaded) return;
    if (s_wheelRetryTimer > 0) {
        s_wheelRetryTimer--;
        return;
    }
    s_wheelRetryTimer = 30;

    JKRArchive* ringArc = g_dComIfG_gameInfo.play.getRingResArchive();
    if (!ringArc) return;

    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;

    if (!s_wheelScreen) {
        s_wheelScreen = JKR_NEW J2DScreen();
        if (s_wheelScreen != nullptr) {
            bool ok = s_wheelScreen->setPriority("SCRN/zelda_item_select_icon3_center_parts.blo", 0x20000, ringArc);
            if (!ok) {
                ok = s_wheelScreen->setPriority("zelda_item_select_icon3_center_parts.blo", 0x20000, ringArc);
            }
            if (ok) {
                dPaneClass_showNullPane(s_wheelScreen);

                J2DPane* center_n = s_wheelScreen->search(MULTI_CHAR('center_n'));
                if (center_n) center_n->hide();
                J2DPane* label_n = s_wheelScreen->search(MULTI_CHAR('label_n'));
                if (label_n) label_n->hide();
                J2DPane* a_itmn_n = s_wheelScreen->search(MULTI_CHAR('a_itmn_n'));
                if (a_itmn_n) a_itmn_n->hide();
                J2DPane* itemn_n = s_wheelScreen->search(MULTI_CHAR('itemn_n'));
                if (itemn_n) itemn_n->hide();

                s_wheelCircle = JKR_NEW CPaneMgr(s_wheelScreen, MULTI_CHAR('circle_n'), 2, nullptr);
            } else {
                JKR_DELETE(s_wheelScreen);
                s_wheelScreen = nullptr;
            }
        }
    }

    if (oldHeap != nullptr) {
        mDoExt_setCurrentHeap(oldHeap);
    }

    s_wheelLoaded = (s_wheelScreen != nullptr && s_wheelCircle != nullptr);
}

void quick_access_radial_select(f32 stickX, f32 stickY, f32 stickMag) {
    const f32 kDeadzone = 0.35f;
    if (stickMag < kDeadzone) {
        return;
    }

    int newSlot = SLOT_NONE;
    if (stickY > 0.0f && stickY > std::abs(stickX)) {
        newSlot = SLOT_UP;
    } else if (stickY < 0.0f && -stickY > std::abs(stickX)) {
        newSlot = SLOT_DOWN;
    } else if (stickX < 0.0f && -stickX > std::abs(stickY)) {
        newSlot = SLOT_LEFT;
    } else if (stickX > 0.0f && stickX > std::abs(stickY)) {
        newSlot = SLOT_RIGHT;
    }

    if (newSlot != s_selectedSlot && newSlot != SLOT_NONE) {
        u8 active[QA_QUICK_SLOTS];
        int count = qa_get_active_items(active);
        if (newSlot < count) {
            s_selectedSlot = newSlot;
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }
}

void quick_access_radial_reset() {
    cleanup_radial_wheel_resources();
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        s_slotScale[i] = 1.0f;
    }
}

static void draw_icon(J2DPicture* pic, ResTIMG* img, f32 sx, f32 sy, f32 scale, u8 alpha,
                      J2DPicture* pic2 = nullptr, f32* outX = nullptr, f32* outY = nullptr,
                      f32* outW = nullptr, f32* outH = nullptr) {
    f32 targetW = 32.0f * scale;
    f32 targetH = 32.0f * scale;
    if (img != nullptr && img->width > 0 && img->height > 0 && img->width != img->height) {
        if (img->width > img->height) {
            targetH = targetW * (static_cast<f32>(img->height) / static_cast<f32>(img->width));
        } else {
            targetW = targetH * (static_cast<f32>(img->width) / static_cast<f32>(img->height));
        }
    }
    const f32 x = sx - targetW * 0.5f;
    const f32 y = sy - targetH * 0.5f;
    pic->setAlpha(alpha);
    pic->draw(x, y, targetW, targetH, false, false, false);
    if (pic2 != nullptr) {
        pic2->setAlpha(alpha);
        pic2->draw(x, y, targetW, targetH, false, false, false);
    }
    if (outX != nullptr) *outX = x;
    if (outY != nullptr) *outY = y;
    if (outW != nullptr) *outW = targetW;
    if (outH != nullptr) *outH = targetH;
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

void qa_radial_draw_wheel(f32 centerX, f32 centerY, u8 alpha, f32 alphaRate) {
    const f32 wheelScale = 0.85f;
    const f32 radius = 92.0f;

    load_radial_wheel_resources();

    if (s_wheelCircle != nullptr && s_wheelScreen != nullptr) {
        J2DPane* n_all = s_wheelScreen->search(MULTI_CHAR('n_all'));
        if (n_all != nullptr) {
            n_all->translate(centerX, centerY);
        }
        if (s_wheelCircle->getPanePtr() != nullptr) {
            s_wheelCircle->getPanePtr()->translate(0.0f, 0.0f);
        }
        s_wheelCircle->scale(wheelScale, wheelScale);
        s_wheelCircle->setAlphaRate(alphaRate);
        s_wheelScreen->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());
    } else {
        const f32 r = 145.0f;
        qa_draw_solid_disc(centerX, centerY, r,
                           JUtility::TColor(20, 16, 8, static_cast<u8>(alpha * 0.55f)), 64);
        qa_draw_solid_ring(centerX, centerY, r - 2.5f, r + 2.5f,
                           JUtility::TColor(210, 175, 90, static_cast<u8>(alpha * 0.9f)), 64);
        qa_draw_solid_ring(centerX, centerY, r - 15.0f, r - 12.5f,
                           JUtility::TColor(185, 155, 75, static_cast<u8>(alpha * 0.5f)), 64);
    }

    JUtility::TColor spokeColor(120, 100, 50, static_cast<u8>(alpha * 0.45f));
    const f32 spokeW = 2.0f;

    qa_draw_solid_rect(centerX - spokeW * 0.5f, centerY - radius, spokeW, radius, spokeColor);
    qa_draw_solid_rect(centerX - spokeW * 0.5f, centerY, spokeW, radius, spokeColor);
    qa_draw_solid_rect(centerX - radius, centerY - spokeW * 0.5f, radius, spokeW, spokeColor);
    qa_draw_solid_rect(centerX, centerY - spokeW * 0.5f, radius, spokeW, spokeColor);
}

void quick_access_radial_draw(f32 centerX, f32 centerY, u8 alpha, f32 glow) {
    (void)glow;
    const f32 radius = 92.0f;

    centerY -= (1.0f - s_menuAlpha) * 16.0f;

    qa_radial_draw_wheel(centerX, centerY, alpha, s_menuAlpha);

    u8 active[QA_QUICK_SLOTS];
    const int activeCount = qa_get_active_items(active);

    const f32 slotCoords[QA_QUICK_SLOTS][2] = {
        { centerX, centerY - radius },
        { centerX, centerY + radius },
        { centerX - radius, centerY },
        { centerX + radius, centerY },
    };

    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        bool isSelected = (s_selectedSlot == i);
        f32 sx = slotCoords[i][0];
        f32 sy = slotCoords[i][1];

        if (i >= activeCount) {
            qa_draw_collection_slot(sx, sy, 40.0f, static_cast<u8>(alpha * 0.35f), false, false);
            continue;
        }

        u8 itemNo = active[i];

        s_slotScale[i] += ((isSelected ? 1.22f : 1.0f) - s_slotScale[i]) * 0.28f;
        const f32 currentScale = s_slotScale[i];

        qa_draw_collection_slot(sx, sy, 40.0f * currentScale, alpha, isSelected,
                                itemNo == qa_strip_assigned_item());

        J2DPicture* pic = nullptr;
        ResTIMG* img = nullptr;
        J2DPicture* pic2 = nullptr;
        if (qa_get_item_icon(itemNo, &pic, &img, &pic2)) {
            f32 iconX = 0.0f;
            f32 iconY = 0.0f;
            f32 iconW = 0.0f;
            f32 iconH = 0.0f;
            draw_icon(pic, img, sx, sy, currentScale, alpha, pic2, &iconX, &iconY, &iconW, &iconH);

            qa_draw_item_ammo(itemNo, iconX, iconY, iconW, iconH, alpha);

            if (item_state_badge(itemNo)) {
                qa_draw_solid_disc(sx + 13.0f * currentScale, sy - 13.0f * currentScale, 4.5f,
                                   JUtility::TColor(255, 190, 45, alpha), 24);
            }
        }
    }

    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (s_selectedSlot != SLOT_NONE && s_selectedSlot < activeCount && cursor != nullptr) {
        cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
        cursor->setPos(slotCoords[s_selectedSlot][0], slotCoords[s_selectedSlot][1]);
        cursor->setAlphaRate(s_menuAlpha);
        cursor->draw();

        J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
        if (port != nullptr) {
            port->setup2D();
        }
    }

    if (s_selectedSlot != SLOT_NONE && s_selectedSlot < activeCount) {
        char labelBuf[64] = "";
        qa_item_label(active[s_selectedSlot], labelBuf, sizeof(labelBuf));

        if (labelBuf[0] != '\0') {
            const f32 fontW = 9.0f;
            const f32 fontH = 11.5f;
            static f32 s_labelW = 0.0f;
            static char s_labelCache[64] = "";
            f32 textW;
            if (s_labelW > 0.0f && std::strcmp(s_labelCache, labelBuf) == 0) {
                textW = s_labelW;
            } else {
                textW = qa_get_text_width(labelBuf, fontW);
            }
            f32 textX = slotCoords[s_selectedSlot][0] - textW * 0.5f;
            f32 textY = slotCoords[s_selectedSlot][1] + 32.0f;

            JUtility::TColor creamTop(255, 248, 210, alpha);
            JUtility::TColor goldBot(235, 185, 65, alpha);
            f32 endX = qa_draw_text(labelBuf, textX, textY, fontW, fontH, creamTop, goldBot, alpha);
            s_labelW = endX - textX;
            std::snprintf(s_labelCache, sizeof(s_labelCache), "%s", labelBuf);
        }
    }

    const f32 screenH = centerY * 2.0f;
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
    if (iconsReady) {
        qa_draw_hint_button(2, hintX, hintY - 12.0f, hintIconH, alpha);
        hintX += hintIconW + hintGap;
    }
    qa_draw_text(hintText, hintX, hintY, hintFontW, hintFontH,
                 JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                 alpha);
}
