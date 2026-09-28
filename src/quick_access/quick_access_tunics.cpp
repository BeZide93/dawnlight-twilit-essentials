#include "quick_access_internal.hpp"
#include "quick_access_bottles.hpp"
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
    QA_PAGE_BOTTLES = -1,
    QA_PAGE_ITEMS = 0,
    QA_PAGE_TUNICS = 1,
};

constexpr int kBottleSlots = 4;

constexpr f32 kTunicRadius = 92.0f;
constexpr f32 kPi = 3.14159265f;
constexpr f32 kStickDeadzone = 0.35f;

int s_page = QA_PAGE_ITEMS;
int s_tunicSelected = SLOT_NONE;
int s_bottleSelected = SLOT_NONE;
f32 s_bottleScale[kBottleSlots] = {1.0f, 1.0f, 1.0f, 1.0f};
f32 s_tunicScale[COLLECTION_TUNIC_COUNT] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
J2DPicture* s_tunicPic[COLLECTION_TUNIC_COUNT] = {};
const ResTIMG* s_tunicPicTex[COLLECTION_TUNIC_COUNT] = {};
J2DPicture* s_shoulderPic = nullptr;
const ResTIMG* s_shoulderTex = nullptr;
bool s_lPrev = false;
bool s_rPrev = false;
bool s_pageBtnValid[2] = {};
f32 s_pageBtnRect[2][4] = {};

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
    const int side = left ? 0 : 1;
    s_pageBtnRect[side][0] = left ? btnX : labelX;
    s_pageBtnRect[side][1] = y;
    s_pageBtnRect[side][2] = left ? labelX + labelW : btnX + btnW;
    s_pageBtnRect[side][3] = y + btnH;
    s_pageBtnValid[side] = true;

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

void bottle_slot_pos(int slot, f32 cx, f32 cy, f32* x, f32* y) {
    static const f32 kDir[kBottleSlots][2] = {{0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}};
    *x = cx + kDir[slot][0] * kTunicRadius;
    *y = cy + kDir[slot][1] * kTunicRadius;
}

int assigned_or_first_bottle() {
    const int assigned = qa_bottle_assigned_slot();
    if (assigned >= 0 && qa_bottle_owned(assigned)) return assigned;
    for (int i = 0; i < kBottleSlots; i++) {
        if (qa_bottle_owned(i)) return i;
    }
    return SLOT_NONE;
}

void draw_bottle_slot(int i, f32 sx, f32 sy, f32 baseSize, f32 scale, u8 alpha, bool selected) {
    if (!qa_bottle_owned(i)) {
        qa_draw_collection_slot(sx, sy, baseSize, static_cast<u8>(alpha * 0.35f), false, false);
        return;
    }
    qa_draw_collection_slot(sx, sy, baseSize * scale, alpha, selected,
                            qa_bottle_assigned_slot() == i);
    J2DPicture* pic = nullptr;
    ResTIMG* img = nullptr;
    J2DPicture* pic2 = nullptr;
    if (qa_get_item_icon(qa_bottle_item(i), &pic, &img, &pic2)) {
        draw_tunic_icon(pic, img, pic2, sx, sy, scale * 0.94f, alpha);
    }
}

void draw_label_at(const char* label, f32 x, f32 y, u8 alpha) {
    if (label == nullptr || label[0] == '\0') return;
    const f32 fontW = 9.0f;
    const f32 fontH = 11.5f;
    const f32 textW = qa_get_text_width(label, fontW);
    qa_draw_text(label, x - textW * 0.5f, y, fontW, fontH, JUtility::TColor(255, 248, 210, alpha),
                 JUtility::TColor(235, 185, 65, alpha), alpha);
}

void draw_ring_cursor(f32 x, f32 y) {
    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (cursor == nullptr) return;
    cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
    cursor->setPos(x, y);
    cursor->setAlphaRate(s_menuAlpha);
    cursor->draw();
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port != nullptr) port->setup2D();
}

const char* page_name(int page) {
    switch (page) {
    case QA_PAGE_BOTTLES: return "Bottles";
    case QA_PAGE_TUNICS: return "Tunics";
    default: return "Items";
    }
}

void play_cursor() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void play_error() {
    Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
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

bool qa_bottle_page_active() {
    return s_page == QA_PAGE_BOTTLES;
}

bool qa_side_page_active() {
    return s_page != QA_PAGE_ITEMS;
}

void qa_page_reset() {
    s_page = QA_PAGE_ITEMS;
    s_tunicSelected = SLOT_NONE;
    s_bottleSelected = SLOT_NONE;
    s_lPrev = controls_l_shoulder_raw_held();
    s_rPrev = true;
}

void qa_page_step(int dir) {
    const int next = s_page + dir;
    if (next < QA_PAGE_BOTTLES || next > QA_PAGE_TUNICS) return;
    s_page = next;
    s_tunicSelected = SLOT_NONE;
    s_bottleSelected = SLOT_NONE;
    s_selectedSlot = SLOT_NONE;
    play_page_se();
}

bool qa_page_button_rect(int side, f32* left, f32* top, f32* right, f32* bottom) {
    if (side < 0 || side > 1 || !s_pageBtnValid[side]) return false;
    *left = s_pageBtnRect[side][0];
    *top = s_pageBtnRect[side][1];
    *right = s_pageBtnRect[side][2];
    *bottom = s_pageBtnRect[side][3];
    return true;
}

bool qa_page_input(interface_of_controller_pad& pad) {
    const bool lRaw = controls_l_shoulder_raw_held();
    const bool lTrig = (pad.mPressedButtonFlags & PAD_TRIGGER_L) != 0 || (lRaw && !s_lPrev);
    s_lPrev = lRaw;
    const bool rRawTrig = right_shoulder_raw_trigger() && !s_rPrev;
    s_rPrev = false;
    const bool rTrig = (pad.mPressedButtonFlags & PAD_TRIGGER_R) != 0 || rRawTrig;

    const int before = s_page;
    if (lTrig) {
        qa_page_step(-1);
    } else if (rTrig) {
        qa_page_step(1);
    }
    return s_page != before;
}

int qa_side_slot_count() {
    return s_page == QA_PAGE_BOTTLES ? kBottleSlots : COLLECTION_TUNIC_COUNT;
}

void qa_side_slot_center(int slot, f32 centerX, f32 centerY, f32* x, f32* y) {
    if (s_page == QA_PAGE_BOTTLES) {
        bottle_slot_pos(slot, centerX, centerY, x, y);
    } else {
        tunic_slot_pos(slot, centerX, centerY, x, y);
    }
}

f32 qa_side_strip_slot_x(int slot, f32 centerX) {
    const int count = qa_side_slot_count();
    const f32 span = static_cast<f32>(count - 1) * QA_STRIP_BOX_SPACING;
    return centerX - span * 0.5f + static_cast<f32>(slot) * QA_STRIP_BOX_SPACING;
}

void qa_side_hover(int slot) {
    if (s_page == QA_PAGE_BOTTLES) {
        if (slot < 0 || slot >= kBottleSlots || slot == s_bottleSelected || !qa_bottle_owned(slot)) return;
        s_bottleSelected = slot;
        play_cursor();
        return;
    }
    if (slot < 0 || slot >= COLLECTION_TUNIC_COUNT || slot == s_tunicSelected ||
        !collection_tunic_unlocked(slot)) {
        return;
    }
    s_tunicSelected = slot;
    play_cursor();
}

void qa_side_confirm() {
    if (s_page == QA_PAGE_BOTTLES) {
        if (s_bottleSelected != SLOT_NONE) qa_bottle_use(s_bottleSelected);
        return;
    }
    if (s_tunicSelected == SLOT_NONE) return;
    if (!collection_tunic_equip(s_tunicSelected)) play_error();
}

void qa_side_pick(int slot) {
    const bool valid = s_page == QA_PAGE_BOTTLES
                           ? (slot >= 0 && slot < kBottleSlots && qa_bottle_owned(slot))
                           : (slot >= 0 && slot < COLLECTION_TUNIC_COUNT && collection_tunic_unlocked(slot));
    if (!valid) {
        play_error();
        return;
    }
    if (s_page == QA_PAGE_BOTTLES) {
        s_bottleSelected = slot;
    } else {
        s_tunicSelected = slot;
    }
    qa_pointer_close_menu();
    qa_side_confirm();
}

bool qa_side_has_selection() {
    return s_page == QA_PAGE_BOTTLES ? s_bottleSelected != SLOT_NONE : s_tunicSelected != SLOT_NONE;
}

void qa_side_select(f32 stickX, f32 stickY, f32 stickMag) {
    if (stickMag < kStickDeadzone) return;
    if (s_page == QA_PAGE_BOTTLES) {
        int slot = SLOT_NONE;
        if (stickY > 0.0f && stickY > std::fabs(stickX)) {
            slot = SLOT_UP;
        } else if (stickY < 0.0f && -stickY > std::fabs(stickX)) {
            slot = SLOT_DOWN;
        } else if (stickX < 0.0f && -stickX > std::fabs(stickY)) {
            slot = SLOT_LEFT;
        } else if (stickX > 0.0f && stickX > std::fabs(stickY)) {
            slot = SLOT_RIGHT;
        }
        if (slot != SLOT_NONE) qa_side_hover(slot);
        return;
    }
    const int t = nearest_unlocked_tunic(std::atan2(-stickY, stickX));
    if (t != SLOT_NONE) qa_side_hover(t);
}

void qa_side_strip_cycle(int dir) {
    const bool bottles = s_page == QA_PAGE_BOTTLES;
    const int count = qa_side_slot_count();
    int& selected = bottles ? s_bottleSelected : s_tunicSelected;
    int i = selected == SLOT_NONE ? (bottles ? assigned_or_first_bottle() : equipped_or_first_tunic())
                                  : selected;
    if (i == SLOT_NONE) return;
    for (int n = 0; n < count; n++) {
        i = (i + dir + count) % count;
        if (bottles ? qa_bottle_owned(i) : collection_tunic_unlocked(i)) break;
    }
    if (i != selected) {
        selected = i;
        play_cursor();
    }
}

void qa_draw_page_buttons_strip(f32 centerX, f32 y, u8 alpha) {
    s_pageBtnValid[0] = false;
    s_pageBtnValid[1] = false;
    const f32 edge = 148.0f;
    const f32 btnY = y - kShoulderBtnH * 0.5f;
    if (s_page > QA_PAGE_BOTTLES) {
        const char* label = page_name(s_page - 1);
        draw_shoulder_button(true, label, centerX - edge - shoulder_button_total_width(label), btnY,
                             alpha);
    }
    if (s_page < QA_PAGE_TUNICS) {
        const char* label = page_name(s_page + 1);
        draw_shoulder_button(false, label, centerX + edge + shoulder_button_total_width(label), btnY,
                             alpha);
    }
}

void qa_draw_page_buttons(f32 centerX, f32 centerY, u8 alpha) {
    s_pageBtnValid[0] = false;
    s_pageBtnValid[1] = false;
    const f32 y = centerY - 150.0f;
    if (s_page > QA_PAGE_BOTTLES) {
        draw_shoulder_button(true, page_name(s_page - 1), centerX - 150.0f, y, alpha);
    }
    if (s_page < QA_PAGE_TUNICS) {
        draw_shoulder_button(false, page_name(s_page + 1), centerX + 150.0f, y, alpha);
    }
}

void quick_access_side_page_strip_draw(f32 screenW, f32 screenH, u8 alpha) {
    (void)screenH;
    qa_hud_scale_begin(screenW * 0.5f, 0.0f);
    const f32 centerX = screenW * 0.5f;
    const f32 slide = -(1.0f - s_menuAlpha) * 16.0f;
    const f32 cy = QA_STRIP_BAR_Y + slide;
    const bool bottles = s_page == QA_PAGE_BOTTLES;

    qa_draw_msg_window(centerX - 140.0f, 34.0f + slide, 280.0f, 52.0f, s_menuAlpha);

    int& selected = bottles ? s_bottleSelected : s_tunicSelected;
    if (selected == SLOT_NONE) {
        selected = bottles ? assigned_or_first_bottle() : equipped_or_first_tunic();
    }

    for (int i = 0; i < qa_side_slot_count(); i++) {
        const bool isSel = selected == i;
        f32& scale = bottles ? s_bottleScale[i] : s_tunicScale[i];
        scale += ((isSel ? 1.16f : 1.0f) - scale) * 0.28f;
        const f32 x = qa_side_strip_slot_x(i, centerX);
        if (bottles) {
            draw_bottle_slot(i, x, cy, QA_STRIP_BOX_HALF * 2.0f, scale, alpha, isSel);
        } else {
            draw_tunic_slot(i, x, cy, QA_STRIP_BOX_HALF * 2.0f, scale, alpha, isSel);
        }
    }

    if (selected != SLOT_NONE) {
        quick_access_strip_cursor_request(qa_side_strip_slot_x(selected, centerX), cy);
        char label[64] = "";
        if (bottles) {
            qa_item_label(qa_bottle_item(selected), label, sizeof(label));
        } else {
            std::snprintf(label, sizeof(label), "%s", collection_tunic_name(selected));
        }
        draw_label_at(label, centerX, QA_STRIP_BAR_Y - QA_STRIP_BOX_HALF - 12.0f + slide, alpha);
    }

    qa_draw_page_buttons_strip(centerX, cy, alpha);
    quick_access_strip_cursor_present();
    qa_hud_scale_end();
}

void quick_access_side_page_draw(f32 centerX, f32 centerY, u8 alpha) {
    qa_hud_scale_begin(centerX, centerY);
    centerY -= (1.0f - s_menuAlpha) * 16.0f;
    const bool bottles = s_page == QA_PAGE_BOTTLES;

    qa_radial_draw_wheel(centerX, centerY, alpha, s_menuAlpha, bottles);
    if (!bottles) {
        const JUtility::TColor spokeColor(120, 100, 50, static_cast<u8>(alpha * 0.45f));
        for (int t = 0; t < COLLECTION_TUNIC_COUNT; t++) {
            draw_spoke(centerX, centerY, tunic_angle(t), kTunicRadius, 2.0f, spokeColor);
        }
    }

    const int selected = bottles ? s_bottleSelected : s_tunicSelected;
    f32 pos[COLLECTION_TUNIC_COUNT][2];
    for (int i = 0; i < qa_side_slot_count(); i++) {
        qa_side_slot_center(i, centerX, centerY, &pos[i][0], &pos[i][1]);
        const bool isSel = selected == i;
        f32& scale = bottles ? s_bottleScale[i] : s_tunicScale[i];
        const bool usable = bottles ? qa_bottle_owned(i) : collection_tunic_unlocked(i);
        if (usable) scale += ((isSel ? 1.22f : 1.0f) - scale) * 0.28f;
        if (bottles) {
            draw_bottle_slot(i, pos[i][0], pos[i][1], 40.0f, scale, alpha, isSel);
        } else {
            draw_tunic_slot(i, pos[i][0], pos[i][1], 40.0f, scale, alpha, isSel);
        }
    }

    if (selected != SLOT_NONE) {
        draw_ring_cursor(pos[selected][0], pos[selected][1]);
        char label[64] = "";
        if (bottles) {
            qa_item_label(qa_bottle_item(selected), label, sizeof(label));
        } else {
            std::snprintf(label, sizeof(label), "%s", collection_tunic_name(selected));
        }
        draw_label_at(label, pos[selected][0], pos[selected][1] + 32.0f, alpha);
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
    s_bottleSelected = SLOT_NONE;
}
