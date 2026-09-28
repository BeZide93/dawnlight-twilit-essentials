#include "quick_access_internal.hpp"
#include "../collection_menu/collection_menu.hpp"
#include "../controls/controls.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_select_cursor.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "Z2AudioLib/Z2AudioMgr.h"

#include <dolphin/gx.h>

#include <cmath>
#include <cstdio>
#include <cstring>

const ResTIMG* get_orig_z_button_texture();
JUtility::TColor get_orig_z_button_black();
JUtility::TColor get_orig_z_button_white();

namespace {

enum QaPage {
    QA_PAGE_ITEMS = 0,
    QA_PAGE_TUNICS = 1,
};

constexpr f32 kTunicRadius = 92.0f;
constexpr f32 kPi = 3.14159265f;
constexpr f32 kStickDeadzone = 0.35f;

int s_page = QA_PAGE_ITEMS;
int s_tunicSelected = SLOT_NONE;
f32 s_tunicScale[COLLECTION_TUNIC_COUNT] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
J2DPicture* s_tunicPic[COLLECTION_TUNIC_COUNT] = {};
const ResTIMG* s_tunicPicTex[COLLECTION_TUNIC_COUNT] = {};
J2DPicture* s_shoulderPic = nullptr;
const ResTIMG* s_shoulderTex = nullptr;
bool s_lPrev = false;
bool s_rPrev = false;
bool s_pageBtnValid = false;
f32 s_pageBtnRect[4] = {};

f32 tunic_angle(int tunic) {
    return -kPi * 0.5f + static_cast<f32>(tunic) * (2.0f * kPi / COLLECTION_TUNIC_COUNT);
}

void tunic_slot_pos(int tunic, f32 cx, f32 cy, f32* x, f32* y) {
    const f32 a = tunic_angle(tunic);
    *x = cx + std::cos(a) * kTunicRadius;
    *y = cy + std::sin(a) * kTunicRadius;
}

J2DPicture* make_picture(J2DPicture* pic, const ResTIMG* tex) {
    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
    if (pic == nullptr) {
        pic = JKR_NEW J2DPicture(tex);
    } else {
        pic->changeTexture(tex, 0);
    }
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    return pic;
}

J2DPicture* tunic_picture(int tunic, const ResTIMG** outTex) {
    const ResTIMG* tex = collection_tunic_icon(tunic);
    if (tex == nullptr) return nullptr;
    if (s_tunicPic[tunic] == nullptr || s_tunicPicTex[tunic] != tex) {
        s_tunicPic[tunic] = make_picture(s_tunicPic[tunic], tex);
        s_tunicPicTex[tunic] = tex;
    }
    *outTex = tex;
    return s_tunicPic[tunic];
}

J2DPicture* shoulder_picture() {
    const ResTIMG* tex = get_orig_z_button_texture();
    if (tex == nullptr) return nullptr;
    if (s_shoulderPic == nullptr || s_shoulderTex != tex) {
        s_shoulderPic = make_picture(s_shoulderPic, tex);
        s_shoulderTex = tex;
    }
    return s_shoulderPic;
}

void play_page_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 0.9f, 1.2f, -1.0f, -1.0f, 0);
}

bool right_shoulder_raw_trigger() {
    JUTGamePad* gamePad = JUTGamePad::getGamePad(PAD_1);
    return gamePad != nullptr && (gamePad->getTrigger() & PAD_TRIGGER_Z) != 0;
}

void draw_spoke(f32 cx, f32 cy, f32 angle, f32 len, f32 width, JUtility::TColor color) {
    const f32 dx = std::cos(angle);
    const f32 dy = std::sin(angle);
    const f32 nx = -dy * width * 0.5f;
    const f32 ny = dx * width * 0.5f;
    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx != nullptr) ctx->setup2D();
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(cx + nx, cy + ny, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(cx + dx * len + nx, cy + dy * len + ny, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(cx + dx * len - nx, cy + dy * len - ny, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(cx - nx, cy - ny, 0.0f);
    GXColor1u32(color);
    GXEnd();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

void draw_button_letter(const char* letter, f32 cx, f32 cy, f32 h, u8 alpha) {
    JUTFont* font = mDoExt_getMesgFont();
    if (font == nullptr) return;
    const f32 charH = h * 0.42f;
    const f32 charW = charH;
    f32 base = static_cast<f32>(font->getWidth());
    if (base <= 0.0f) base = 1.0f;
    f32 glyphW = static_cast<f32>(font->getWidth(letter[0]));
    if (glyphW <= 0.0f) glyphW = base;
    const f32 textW = glyphW * (charW / base);
    const f32 tx = cx - textW * 0.5f;
    const f32 ty = cy + charH * 0.36f;

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port != nullptr) port->setup2D();
    font->setGX();
    font->setCharColor(JUtility::TColor(0, 0, 0, static_cast<u8>(alpha * 0.7f)));
    static const f32 kOff[4][2] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
    for (const auto& o : kOff) {
        font->drawString_scale(tx + o[0], ty + o[1], charW, charH, letter, true);
    }
    font->setCharColor(JUtility::TColor(255, 255, 255, alpha));
    font->drawString_scale(tx, ty, charW, charH, letter, true);
    if (port != nullptr) port->setup2D();
}

constexpr f32 kShoulderBtnH = 26.0f;
constexpr f32 kShoulderFontW = 9.0f;
constexpr f32 kShoulderFontH = 11.5f;
constexpr f32 kShoulderGap = 6.0f;

f32 shoulder_button_width() {
    J2DPicture* pic = shoulder_picture();
    if (pic != nullptr && s_shoulderTex->height > 0) {
        return kShoulderBtnH * static_cast<f32>(s_shoulderTex->width) /
               static_cast<f32>(s_shoulderTex->height);
    }
    return kShoulderBtnH * 2.0f;
}

f32 shoulder_button_total_width(const char* label) {
    return shoulder_button_width() + kShoulderGap + qa_get_text_width(label, kShoulderFontW);
}

void draw_shoulder_button(bool left, const char* label, f32 anchorX, f32 y, u8 alpha) {
    const f32 btnH = kShoulderBtnH;
    const f32 fontW = kShoulderFontW;
    const f32 fontH = kShoulderFontH;
    const f32 gap = kShoulderGap;

    const f32 btnW = shoulder_button_width();
    J2DPicture* pic = shoulder_picture();
    const f32 labelW = qa_get_text_width(label, fontW);

    const f32 btnX = left ? anchorX : anchorX - btnW;
    const f32 labelX = left ? btnX + btnW + gap : btnX - gap - labelW;
    s_pageBtnRect[0] = left ? btnX : labelX;
    s_pageBtnRect[1] = y;
    s_pageBtnRect[2] = left ? labelX + labelW : btnX + btnW;
    s_pageBtnRect[3] = y + btnH;
    s_pageBtnValid = true;

    if (pic != nullptr) {
        pic->setBlackWhite(get_orig_z_button_black(), get_orig_z_button_white());
        pic->setAlpha(alpha);
        pic->draw(btnX, y, btnW, btnH, left, false, false);
    } else {
        qa_draw_rounded_rect(btnX, y, btnW, btnH, btnH * 0.45f,
                             JUtility::TColor(40, 90, 160, alpha));
    }
    draw_button_letter(left ? "L" : "R", btnX + btnW * 0.5f, y + btnH * 0.5f, btnH, alpha);

    qa_draw_text(label, labelX, y + btnH * 0.5f + fontH * 0.4f, fontW, fontH,
                 JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                 alpha);
}

void draw_tunic_icon(J2DPicture* pic, const ResTIMG* tex, J2DPicture* pic2, f32 sx, f32 sy,
                     f32 scale, u8 alpha) {
    f32 w = 32.0f * scale;
    f32 h = 32.0f * scale;
    if (tex != nullptr && tex->width > 0 && tex->height > 0 && tex->width != tex->height) {
        if (tex->width > tex->height) {
            h = w * static_cast<f32>(tex->height) / static_cast<f32>(tex->width);
        } else {
            w = h * static_cast<f32>(tex->width) / static_cast<f32>(tex->height);
        }
    }
    pic->setAlpha(alpha);
    pic->draw(sx - w * 0.5f, sy - h * 0.5f, w, h, false, false, false);
    if (pic2 != nullptr) {
        pic2->setAlpha(alpha);
        pic2->draw(sx - w * 0.5f, sy - h * 0.5f, w, h, false, false, false);
    }
}

int equipped_or_first_tunic() {
    int first = SLOT_NONE;
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        if (!collection_tunic_unlocked(t)) continue;
        if (collection_tunic_equipped(t)) return t;
        if (first == SLOT_NONE) first = t;
    }
    return first;
}

void draw_tunic_slot(int t, f32 sx, f32 sy, f32 baseSize, f32 scale, u8 alpha, bool selected) {
    if (!collection_tunic_unlocked(t)) {
        qa_draw_collection_slot(sx, sy, baseSize, static_cast<u8>(alpha * 0.35f), false, false);
        return;
    }
    qa_draw_collection_slot(sx, sy, baseSize * scale, alpha, selected, collection_tunic_equipped(t));

    const u8 iconItem = collection_tunic_icon_item(t);
    J2DPicture* itemPic = nullptr;
    ResTIMG* itemImg = nullptr;
    J2DPicture* itemPic2 = nullptr;
    if (iconItem != dItemNo_NONE_e && qa_get_item_icon(iconItem, &itemPic, &itemImg, &itemPic2)) {
        draw_tunic_icon(itemPic, itemImg, itemPic2, sx, sy, scale, alpha);
        return;
    }
    const ResTIMG* tex = nullptr;
    J2DPicture* pic = tunic_picture(t, &tex);
    if (pic != nullptr) {
        draw_tunic_icon(pic, tex, nullptr, sx, sy, scale, alpha);
    }
}

int nearest_unlocked_tunic(f32 angle) {
    int best = SLOT_NONE;
    f32 bestDiff = 10.0f;
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        if (!collection_tunic_unlocked(t)) continue;
        f32 diff = std::fabs(angle - tunic_angle(t));
        while (diff > kPi) diff = std::fabs(diff - 2.0f * kPi);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = t;
        }
    }
    return best;
}

}

bool qa_tunic_page_active() {
    return s_page == QA_PAGE_TUNICS;
}

void qa_page_reset() {
    s_page = QA_PAGE_ITEMS;
    s_tunicSelected = SLOT_NONE;
    s_lPrev = controls_l_shoulder_raw_held();
    s_rPrev = true;
}

void qa_page_toggle() {
    s_page = s_page == QA_PAGE_ITEMS ? QA_PAGE_TUNICS : QA_PAGE_ITEMS;
    s_tunicSelected = SLOT_NONE;
    s_selectedSlot = SLOT_NONE;
    play_page_se();
}

bool qa_page_button_rect(f32* left, f32* top, f32* right, f32* bottom) {
    if (!s_pageBtnValid) return false;
    *left = s_pageBtnRect[0];
    *top = s_pageBtnRect[1];
    *right = s_pageBtnRect[2];
    *bottom = s_pageBtnRect[3];
    return true;
}

void qa_tunic_slot_center(int tunic, f32 centerX, f32 centerY, f32* x, f32* y) {
    tunic_slot_pos(tunic, centerX, centerY, x, y);
}

int qa_tunic_slot_count() {
    return COLLECTION_TUNIC_COUNT;
}

void qa_tunic_hover(int tunic) {
    if (tunic < 0 || tunic >= COLLECTION_TUNIC_COUNT || tunic == s_tunicSelected ||
        !collection_tunic_unlocked(tunic)) {
        return;
    }
    s_tunicSelected = tunic;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void qa_tunic_pick(int tunic) {
    if (tunic < 0 || tunic >= COLLECTION_TUNIC_COUNT || !collection_tunic_unlocked(tunic)) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }
    s_tunicSelected = tunic;
    qa_tunic_confirm();
    qa_pointer_close_menu();
}

bool qa_page_input(interface_of_controller_pad& pad) {
    const bool lRaw = controls_l_shoulder_raw_held();
    const bool lTrig = (pad.mPressedButtonFlags & PAD_TRIGGER_L) != 0 || (lRaw && !s_lPrev);
    s_lPrev = lRaw;
    const bool rRawTrig = right_shoulder_raw_trigger() && !s_rPrev;
    s_rPrev = false;
    const bool rTrig = (pad.mPressedButtonFlags & PAD_TRIGGER_R) != 0 || rRawTrig;

    if ((s_page == QA_PAGE_ITEMS && rTrig) || (s_page == QA_PAGE_TUNICS && lTrig)) {
        qa_page_toggle();
        return true;
    }
    return false;
}

void qa_tunic_select(f32 stickX, f32 stickY, f32 stickMag) {
    if (stickMag < kStickDeadzone) return;
    const int t = nearest_unlocked_tunic(std::atan2(-stickY, stickX));
    if (t != SLOT_NONE && t != s_tunicSelected) {
        s_tunicSelected = t;
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

bool qa_tunic_has_selection() {
    return s_tunicSelected != SLOT_NONE;
}

void qa_tunic_confirm() {
    if (s_tunicSelected == SLOT_NONE) return;
    if (!collection_tunic_equip(s_tunicSelected)) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

void qa_tunic_strip_cycle(int dir) {
    int t = s_tunicSelected == SLOT_NONE ? equipped_or_first_tunic() : s_tunicSelected;
    if (t == SLOT_NONE) return;
    for (int i = 0; i < COLLECTION_TUNIC_COUNT; i++) {
        t = (t + dir + COLLECTION_TUNIC_COUNT) % COLLECTION_TUNIC_COUNT;
        if (collection_tunic_unlocked(t)) break;
    }
    if (t != s_tunicSelected) {
        s_tunicSelected = t;
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

f32 qa_tunic_strip_slot_x(int tunic, f32 centerX) {
    const f32 span = (COLLECTION_TUNIC_COUNT - 1) * QA_STRIP_BOX_SPACING;
    return centerX - span * 0.5f + static_cast<f32>(tunic) * QA_STRIP_BOX_SPACING;
}

void qa_draw_page_buttons_strip(f32 centerX, f32 y, u8 alpha) {
    s_pageBtnValid = false;
    const f32 edge = 148.0f;
    const f32 btnY = y - kShoulderBtnH * 0.5f;
    if (s_page == QA_PAGE_TUNICS) {
        draw_shoulder_button(true, "Items", centerX - edge - shoulder_button_total_width("Items"),
                             btnY, alpha);
    } else {
        draw_shoulder_button(false, "Tunics", centerX + edge + shoulder_button_total_width("Tunics"),
                             btnY, alpha);
    }
}

void quick_access_tunic_strip_draw(f32 screenW, f32 screenH, u8 alpha) {
    (void)screenH;
    qa_hud_scale_begin(screenW * 0.5f, 0.0f);
    const f32 centerX = screenW * 0.5f;
    const f32 slide = -(1.0f - s_menuAlpha) * 16.0f;
    const f32 cy = QA_STRIP_BAR_Y + slide;

    qa_draw_msg_window(centerX - 140.0f, 34.0f + slide, 280.0f, 52.0f, s_menuAlpha);

    if (s_tunicSelected == SLOT_NONE) {
        s_tunicSelected = equipped_or_first_tunic();
    }

    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        const bool selected = s_tunicSelected == t;
        s_tunicScale[t] += ((selected ? 1.16f : 1.0f) - s_tunicScale[t]) * 0.28f;
        draw_tunic_slot(t, qa_tunic_strip_slot_x(t, centerX), cy, QA_STRIP_BOX_HALF * 2.0f,
                        s_tunicScale[t], alpha, selected);
    }

    if (s_tunicSelected != SLOT_NONE) {
        quick_access_strip_cursor_request(qa_tunic_strip_slot_x(s_tunicSelected, centerX), cy);
        const char* label = collection_tunic_name(s_tunicSelected);
        const f32 fontW = 9.0f;
        const f32 fontH = 11.5f;
        const f32 textW = qa_get_text_width(label, fontW);
        qa_draw_text(label, centerX - textW * 0.5f, QA_STRIP_BAR_Y - QA_STRIP_BOX_HALF - 12.0f + slide,
                     fontW, fontH, JUtility::TColor(255, 248, 210, alpha),
                     JUtility::TColor(235, 185, 65, alpha), alpha);
    }

    qa_draw_page_buttons_strip(centerX, cy, alpha);
    quick_access_strip_cursor_present();
    qa_hud_scale_end();
}

void qa_draw_page_buttons(f32 centerX, f32 centerY, u8 alpha) {
    s_pageBtnValid = false;
    const f32 y = centerY - 150.0f;
    if (s_page == QA_PAGE_TUNICS) {
        draw_shoulder_button(true, "Items", centerX - 150.0f, y, alpha);
    } else {
        draw_shoulder_button(false, "Tunics", centerX + 150.0f, y, alpha);
    }
}

void quick_access_tunic_draw(f32 centerX, f32 centerY, u8 alpha) {
    qa_hud_scale_begin(centerX, centerY);
    centerY -= (1.0f - s_menuAlpha) * 16.0f;

    qa_radial_draw_wheel(centerX, centerY, alpha, s_menuAlpha, false);
    const JUtility::TColor spokeColor(120, 100, 50, static_cast<u8>(alpha * 0.45f));
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        draw_spoke(centerX, centerY, tunic_angle(t), kTunicRadius, 2.0f, spokeColor);
    }

    f32 pos[COLLECTION_TUNIC_COUNT][2];
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        tunic_slot_pos(t, centerX, centerY, &pos[t][0], &pos[t][1]);
        const bool selected = s_tunicSelected == t;
        if (collection_tunic_unlocked(t)) {
            s_tunicScale[t] += ((selected ? 1.22f : 1.0f) - s_tunicScale[t]) * 0.28f;
        }
        draw_tunic_slot(t, pos[t][0], pos[t][1], 40.0f, s_tunicScale[t], alpha, selected);
    }

    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (s_tunicSelected != SLOT_NONE && cursor != nullptr) {
        cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
        cursor->setPos(pos[s_tunicSelected][0], pos[s_tunicSelected][1]);
        cursor->setAlphaRate(s_menuAlpha);
        cursor->draw();
        J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
        if (port != nullptr) port->setup2D();
    }

    if (s_tunicSelected != SLOT_NONE) {
        const char* label = collection_tunic_name(s_tunicSelected);
        const f32 fontW = 9.0f;
        const f32 fontH = 11.5f;
        const f32 textW = qa_get_text_width(label, fontW);
        qa_draw_text(label, pos[s_tunicSelected][0] - textW * 0.5f, pos[s_tunicSelected][1] + 32.0f,
                     fontW, fontH, JUtility::TColor(255, 248, 210, alpha),
                     JUtility::TColor(235, 185, 65, alpha), alpha);
    }

    qa_draw_page_buttons(centerX, centerY, alpha);
    qa_hud_scale_end();
}

void quick_access_tunics_shutdown() {
    for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
        JKR_DELETE(s_tunicPic[t]);
        s_tunicPic[t] = nullptr;
        s_tunicPicTex[t] = nullptr;
    }
    JKR_DELETE(s_shoulderPic);
    s_shoulderPic = nullptr;
    s_shoulderTex = nullptr;
    s_page = QA_PAGE_ITEMS;
    s_tunicSelected = SLOT_NONE;
}
