#include "quick_access_internal.hpp"
#include "quick_access_bottles.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_pane_class.h"
#include "d/d_save.h"
#include "d/d_select_cursor.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "m_Do/m_Do_ext.h"
#include "mods/svc/log.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>

#include <cmath>
#include <cstdio>

static const int QA_EDIT_MAX_ITEMS = 32;
static const int QA_EDIT_COLS = 7;
static const f32 CELL_SPACING_X = 56.0f;
static const f32 CELL_SPACING_Y = 52.0f;
static const f32 GRID_TOP_Y = 150.0f;
static const f32 SLOTS_ROW_Y = 60.0f;

static u8 s_editItems[QA_EDIT_MAX_ITEMS];
static int s_editCount = 0;
static int s_editCursor = 0;
static int s_editSlot = 0;

static f32 s_previewScale[QA_QUICK_SLOTS] = {1.0f, 1.0f, 1.0f, 1.0f};

static void refresh_edit_slot() {
    if (s_editCursor >= 0 && s_editCursor < s_editCount) {
        const u8 cursorItem = s_editItems[s_editCursor];
        for (int i = 0; i < QA_QUICK_SLOTS; i++) {
            if (qa_custom_item(i) == cursorItem) {
                s_editSlot = i;
                break;
            }
        }
    }
    if (s_editSlot >= QA_QUICK_SLOTS) {
        s_editSlot = QA_QUICK_SLOTS - 1;
    }
    if (s_editSlot < 0) {
        s_editSlot = 0;
    }
}

static bool edit_list_contains(u8 itemNo) {
    for (int i = 0; i < s_editCount; i++) {
        if (s_editItems[i] == itemNo) {
            return true;
        }
    }
    return false;
}

static const u8 kCustomizeHardBlacklist[] = {
    dItemNo_HOOKSHOT_e,
    dItemNo_W_HOOKSHOT_e,
    dItemNo_COPY_ROD_e,
    dItemNo_COPY_ROD_2_e,
    dItemNo_IRONBALL_e,
    dItemNo_SPINNER_e,
    dItemNo_BOW_e,
    dItemNo_BOOMERANG_e,
    dItemNo_PACHINKO_e,
    dItemNo_HAWK_EYE_e,
};

static const u8 kCustomizeBlacklist[] = {
    dItemNo_EMPTY_BOTTLE_e,
    dItemNo_RED_BOTTLE_e,
    dItemNo_GREEN_BOTTLE_e,
    dItemNo_BLUE_BOTTLE_e,
    dItemNo_MILK_BOTTLE_e,
    dItemNo_HALF_MILK_BOTTLE_e,
    dItemNo_OIL_BOTTLE_e,
    dItemNo_WATER_BOTTLE_e,
    dItemNo_OIL_BOTTLE_2_e,
    dItemNo_RED_BOTTLE_2_e,
    dItemNo_UGLY_SOUP_e,
    dItemNo_HOT_SPRING_e,
    dItemNo_FAIRY_e,
    dItemNo_HOT_SPRING_2_e,
    dItemNo_OIL2_e,
    dItemNo_OIL_e,
    dItemNo_FAIRY_DROP_e,
    dItemNo_WORM_e,
    dItemNo_DROP_BOTTLE_e,
    dItemNo_BEE_CHILD_e,
    dItemNo_CHUCHU_RARE_e,
    dItemNo_CHUCHU_RED_e,
    dItemNo_CHUCHU_BLUE_e,
    dItemNo_CHUCHU_GREEN_e,
    dItemNo_CHUCHU_YELLOW_e,
    dItemNo_CHUCHU_PURPLE_e,
    dItemNo_LV1_SOUP_e,
    dItemNo_LV2_SOUP_e,
    dItemNo_LV3_SOUP_e,
    dItemNo_CHUCHU_YELLOW2_e,
    dItemNo_OIL_BOTTLE3_e,
    dItemNo_SHOP_BEE_CHILD_e,
    dItemNo_CHUCHU_BLACK_e,
    dItemNo_LIGHT_DROP_e,
};

bool qa_bottles_menu_owns_item(u8 itemNo) {
    if (!g_configBottlesQuickAccessEnabled) {
        return false;
    }
    for (const u8 banned : kCustomizeBlacklist) {
        if (itemNo == banned) {
            return true;
        }
    }
    return false;
}

static bool customize_blacklisted(u8 itemNo) {
    for (const u8 banned : kCustomizeHardBlacklist) {
        if (itemNo == banned) {
            return true;
        }
    }
    return qa_bottles_menu_owns_item(itemNo);
}

static void edit_list_add(u8 itemNo) {
    if (itemNo == dItemNo_NONE_e || s_editCount >= QA_EDIT_MAX_ITEMS || edit_list_contains(itemNo)) {
        return;
    }
    if (customize_blacklisted(itemNo)) {
        return;
    }
    s_editItems[s_editCount++] = itemNo;
}

static void reset_msg_window();

void quick_access_edit_enter() {
    s_editCount = 0;
    s_editCursor = 0;
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        s_previewScale[i] = 1.0f;
    }

    reset_msg_window();

    const dSv_player_item_c& inv = g_dComIfG_gameInfo.info.getPlayer().getItem();

    for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
        u8 slot = inv.mItemSlots[i];
        if (slot == 0xFF || slot >= 24) {
            continue;
        }
        edit_list_add(inv.mItems[slot]);
    }

    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        edit_list_add(qa_custom_item(i));
    }

    static const u8 kQaCandidates[] = {
        dItemNo_HORSE_FLUTE_e,
        dItemNo_HVY_BOOTS_e,
        dItemNo_KANTERA_e,
        dItemNo_KANTERA2_e,
        dItemNo_FISHING_ROD_1_e,
        dItemNo_LURE_ROD_e,
        dItemNo_BEE_ROD_e,
        dItemNo_JEWEL_ROD_e,
        dItemNo_WORM_ROD_e,
        dItemNo_JEWEL_BEE_ROD_e,
        dItemNo_JEWEL_WORM_ROD_e,
    };
    for (const u8 cand : kQaCandidates) {
        for (u8 s = 0; s < 24; s++) {
            if (dComIfGs_getItem(s, false) == cand) {
                edit_list_add(cand);
                break;
            }
        }
    }

    if (s_selectedSlot != SLOT_NONE && s_selectedSlot >= 0 && s_selectedSlot < QA_QUICK_SLOTS) {
        s_editSlot = s_selectedSlot;
    } else {
        s_editSlot = 0;
    }
}

void quick_access_edit_exit() {
    if (g_configQuickAccessAppearance == QA_APPEARANCE_STRIP) {
        s_selectedSlot = (s_editSlot >= 0 && s_editSlot < QA_QUICK_SLOTS) ? s_editSlot : 0;
    } else {
        s_selectedSlot = SLOT_NONE;
    }
    s_editCount = 0;
    s_editCursor = 0;
}

void quick_access_edit_cycle(int dir) {
    quick_access_edit_move(dir, 0);
}

void quick_access_edit_move(int dx, int dy) {
    if (s_editCount <= 0 || (dx == 0 && dy == 0)) {
        return;
    }

    int next = s_editCursor;
    if (dx != 0) {
        next = (s_editCursor + dx + s_editCount) % s_editCount;
    } else {
        const int rows = (s_editCount + QA_EDIT_COLS - 1) / QA_EDIT_COLS;
        int row = (s_editCursor / QA_EDIT_COLS + dy + rows) % rows;
        int col = s_editCursor % QA_EDIT_COLS;
        int inRow = s_editCount - row * QA_EDIT_COLS;
        if (inRow > QA_EDIT_COLS) {
            inRow = QA_EDIT_COLS;
        }
        if (col >= inRow) {
            col = inRow - 1;
        }
        next = row * QA_EDIT_COLS + col;
    }

    if (next != s_editCursor) {
        s_editCursor = next;
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
}

bool quick_access_edit_toggle_current() {
    if (s_editCount == 0) {
        return false;
    }
    if (s_editSlot < 0 || s_editSlot >= QA_QUICK_SLOTS) {
        s_editSlot = 0;
    }
    u8 itemNo = s_editItems[s_editCursor];
    const u8 selectedSlotItem = qa_custom_item(s_editSlot);

    if (selectedSlotItem == itemNo) {
        qa_custom_remove_item(itemNo);
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        refresh_edit_slot();
        return true;
    }

    const u8 familyItem = qa_custom_find_family_item(itemNo);
    if (familyItem != QA_ITEM_NONE) {
        qa_custom_remove_item(familyItem);
    }
    if (!qa_custom_replace_at(s_editSlot, itemNo)) {
        return false;
    }
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    refresh_edit_slot();
    return true;
}

void quick_access_edit_clear_slot() {
    const u8 slotItem = qa_custom_item(quick_access_edit_current_slot());
    if (slotItem == QA_ITEM_NONE) {
        Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        return;
    }
    qa_custom_remove_item(slotItem);
    refresh_edit_slot();
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void quick_access_edit_rotate_slot(int dir) {
    s_editSlot = (s_editSlot + dir + QA_QUICK_SLOTS) % QA_QUICK_SLOTS;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

int quick_access_edit_current_slot() {
    if (s_editSlot < 0 || s_editSlot >= QA_QUICK_SLOTS) {
        return 0;
    }
    return s_editSlot;
}

int quick_access_edit_count() {
    return s_editCount;
}

int quick_access_edit_cursor() {
    return s_editCursor;
}

u8 quick_access_edit_item(int idx) {
    return (idx >= 0 && idx < s_editCount) ? s_editItems[idx] : QA_ITEM_NONE;
}

static void draw_centered_label(const char* text, f32 y, f32 fontW, f32 fontH, u8 alpha,
                                JUtility::TColor top, JUtility::TColor bottom, f32 centerX) {
    f32 textW = qa_get_text_width(text, fontW);
    qa_draw_text(text, centerX - textW * 0.5f, y, fontW, fontH, top, bottom, alpha);
}

static void draw_quick_slot_box(f32 cx, f32 cy, u8 itemNo, bool isAssigned, bool isCurrent,
                                u8 alpha, f32 scale) {
    const f32 half = 20.0f * scale;

    qa_draw_collection_slot(cx, cy, half * 2.0f, alpha, isCurrent, isAssigned);

    if (itemNo == QA_ITEM_NONE) {
        return;
    }

    bool available = qa_is_item_available(itemNo);
    u8 iconAlpha = available ? alpha : static_cast<u8>(alpha * 0.30f);

    J2DPicture* pic = nullptr;
    ResTIMG* img = nullptr;
    J2DPicture* pic2 = nullptr;
    if (qa_get_item_icon(itemNo, &pic, &img, &pic2)) {
        f32 targetW = 32.0f * scale;
        f32 targetH = 32.0f * scale;
        if (img != nullptr && img->width > 0 && img->height > 0 && img->width != img->height) {
            if (img->width > img->height) {
                targetH = targetW * (static_cast<f32>(img->height) / static_cast<f32>(img->width));
            } else {
                targetW = targetH * (static_cast<f32>(img->width) / static_cast<f32>(img->height));
            }
        }
        pic->setAlpha(iconAlpha);
        pic->draw(cx - targetW * 0.5f, cy - targetH * 0.5f, targetW, targetH, false, false, false);
        if (pic2 != nullptr) {
            pic2->setAlpha(iconAlpha);
            pic2->draw(cx - targetW * 0.5f, cy - targetH * 0.5f, targetW, targetH, false, false, false);
        }

        if (available) {
            qa_draw_item_ammo(itemNo, cx - targetW * 0.5f, cy - targetH * 0.5f, targetW, targetH,
                              iconAlpha);
        }
    }

}

struct QaBtnLayer {
    J2DPicture* pic;
    f32 ox, oy;
    f32 w, h;
};

struct QaEditButtonIcons {
    J2DScreen* screen;
    QaBtnLayer layers[4][8];
    int layerCount[4];
    f32 scale[4];
    bool loaded;
};

static QaEditButtonIcons s_btnIcons;
static bool s_btnIconsAttempted = false;

static const f32 QA_HINT_ICON_H = 17.0f;
static const int QA_BTN_MAX_LAYERS = 8;
static const f32 QA_BTN_CIRCLE_PX = 18.0f;

static void reset_button_icons() {
    for (int g = 0; g < 4; g++) {
        for (int l = 0; l < s_btnIcons.layerCount[g]; l++) {
            JKR_DELETE(s_btnIcons.layers[g][l].pic);
        }
    }
    JKR_DELETE(s_btnIcons.screen);
    s_btnIcons = QaEditButtonIcons{};
}

static void collect_picture_panes(J2DPane* root, J2DPane** out, int& count, int max) {
    if (root == nullptr || count >= max) {
        return;
    }
    if (root->getTypeID() == 18) {
        out[count++] = root;
    }
    for (J2DPane* child = root->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        collect_picture_panes(child, out, count, max);
    }
}

static bool load_button_icons() {
    if (s_btnIcons.loaded) {
        return true;
    }
    if (s_btnIconsAttempted) {
        return false;
    }
    s_btnIconsAttempted = true;

    JKRArchive* arc = dComIfGp_getMain2DArchive();
    if (arc == nullptr) {
        s_btnIconsAttempted = false;
        return false;
    }

    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;

    s_btnIcons = QaEditButtonIcons{};

    s_btnIcons.screen = JKR_NEW J2DScreen();
    bool ok = s_btnIcons.screen != nullptr &&
              s_btnIcons.screen->setPriority("zelda_game_image.blo", 0x20000, arc);
    if (ok) {
        dPaneClass_showNullPane(s_btnIcons.screen);
    }

    J2DPane* bBtnPane = ok ? s_btnIcons.screen->search(MULTI_CHAR('b_btn')) : nullptr;

    static const u64 kTags[4] = {
        MULTI_CHAR('abtn_n'), MULTI_CHAR('ybtn_n'),
        MULTI_CHAR('xbtn_n'), MULTI_CHAR('bbtn_n'),
    };
    static const char* kNames[4] = { "A", "Y", "X", "B" };

    for (int i = 0; i < 4 && ok; i++) {
        J2DPane* group = s_btnIcons.screen->search(kTags[i]);
        if (group == nullptr) {
            ok = false;
            break;
        }

        J2DPane* pics[QA_BTN_MAX_LAYERS];
        int picCount = 0;
        collect_picture_panes(group, pics, picCount, QA_BTN_MAX_LAYERS);
        if (picCount == 0) {
            ok = false;
            break;
        }

        JGeometry::TBox2<f32> bounds[QA_BTN_MAX_LAYERS];
        for (int l = 0; l < picCount; l++) {
            bounds[l] = static_cast<J2DPicture*>(pics[l])->getBounds();
        }
        const f32 circle = (bounds[0].f.x - bounds[0].i.x < bounds[0].f.y - bounds[0].i.y)
                               ? (bounds[0].f.x - bounds[0].i.x)
                               : (bounds[0].f.y - bounds[0].i.y);
        if (circle < 1.0f) {
            ok = false;
            break;
        }
        s_btnIcons.scale[i] = QA_BTN_CIRCLE_PX / circle;
        const f32 refCx = (bounds[0].i.x + bounds[0].f.x) * 0.5f;
        const f32 refCy = (bounds[0].i.y + bounds[0].f.y) * 0.5f;

        for (int l = 0; l < picCount; l++) {
            J2DPicture* panePic = static_cast<J2DPicture*>(pics[l]);
            ResTIMG* timg = nullptr;
            if (panePic->getTexture(0) != nullptr) {
                timg = const_cast<ResTIMG*>(panePic->getTexture(0)->getTexInfo());
            }
            if (timg == nullptr) {
                continue;
            }

            QaBtnLayer& layer = s_btnIcons.layers[i][s_btnIcons.layerCount[i]];
            layer.pic = JKR_NEW J2DPicture(timg);
            if (layer.pic == nullptr) {
                continue;
            }
            layer.pic->setBlackWhite(panePic->getBlack(), panePic->getWhite());
            layer.pic->setCornerColor(panePic->corner(0), panePic->corner(1),
                                      panePic->corner(2), panePic->corner(3));
            if (pics[l] == bBtnPane) {
                layer.pic->setWhite(JUtility::TColor(195, 63, 63, 255));
            }

            layer.w = bounds[l].f.x - bounds[l].i.x;
            layer.h = bounds[l].f.y - bounds[l].i.y;
            layer.ox = (bounds[l].i.x + layer.w * 0.5f) - refCx;
            layer.oy = (bounds[l].i.y + layer.h * 0.5f) - refCy;
            if (l > 0) {
                const f32 maxDim = layer.w > layer.h ? layer.w : layer.h;
                if (maxDim <= circle * 0.7f) {
                    layer.ox = 0.0f;
                    layer.oy = 0.0f;
                }
            }
            s_btnIcons.layerCount[i]++;

        }
        if (i == 2) {
            for (int l2 = 1; l2 < s_btnIcons.layerCount[2]; ++l2) {
                s_btnIcons.layers[2][l2].ox -= -0.5f;
                s_btnIcons.layers[2][l2].oy -= -1.0f;
            }
        }

        if (s_btnIcons.layerCount[i] == 0) {
            ok = false;
            break;
        }

    }

    if (oldHeap != nullptr) {
        mDoExt_setCurrentHeap(oldHeap);
    }

    if (!ok) {
        reset_button_icons();
        return false;
    }

    s_btnIcons.loaded = true;
    return true;
}

static void reset_msg_window();
void quick_access_edit_shutdown() {
    reset_button_icons();
    s_btnIconsAttempted = false;
    reset_msg_window();
}

bool qa_hint_button_ready() {
    return load_button_icons();
}

f32 qa_hint_button_width(int idx, f32 h) {
    if (!load_button_icons() || idx < 0 || idx > 3) {
        return 0.0f;
    }
    return QA_BTN_CIRCLE_PX * (h / QA_HINT_ICON_H);
}

void qa_draw_hint_button(int idx, f32 x, f32 y, f32 h, u8 alpha) {
    if (!load_button_icons() || idx < 0 || idx > 3) {
        return;
    }

    const f32 w = qa_hint_button_width(idx, h);
    const f32 s = s_btnIcons.scale[idx] * (h / QA_HINT_ICON_H);
    const f32 cx = x + w * 0.5f;
    const f32 cy = y + h * 0.5f;
    for (int l = 0; l < s_btnIcons.layerCount[idx]; l++) {
        QaBtnLayer& layer = s_btnIcons.layers[idx][l];
        const f32 lw = layer.w * s;
        const f32 lh = layer.h * s;
        layer.pic->setAlpha(alpha);
        layer.pic->draw(cx + layer.ox * s - lw * 0.5f, cy + layer.oy * s - lh * 0.5f, lw, lh,
                        false, false, false);
    }
}

static J2DScreen* s_msgWindowScreen = nullptr;
static CPaneMgr* s_msgWindowPane = nullptr;
static bool s_msgWindowAttempted = false;

static void reset_msg_window() {
    JKR_DELETE(s_msgWindowPane);
    JKR_DELETE(s_msgWindowScreen);
    s_msgWindowPane = nullptr;
    s_msgWindowScreen = nullptr;
    s_msgWindowAttempted = false;
}

static bool load_msg_window() {
    if (s_msgWindowPane != nullptr) {
        return true;
    }
    if (s_msgWindowAttempted) {
        return false;
    }
    s_msgWindowAttempted = true;

    JKRArchive* arc = g_dComIfG_gameInfo.play.getMsgArchive(1);
    if (arc == nullptr) {
        s_msgWindowAttempted = false;
        return false;
    }

    s_msgWindowScreen = JKR_NEW J2DScreen();
    if (s_msgWindowScreen == nullptr) {
        return false;
    }
    if (!s_msgWindowScreen->setPriority("zelda_message_window_new.blo", 0x20000, arc)) {
        JKR_DELETE(s_msgWindowScreen);
        s_msgWindowScreen = nullptr;
        return false;
    }
    dPaneClass_showNullPane(s_msgWindowScreen);

    s_msgWindowPane = JKR_NEW CPaneMgr(s_msgWindowScreen, MULTI_CHAR('n_all'), 3, nullptr);
    if (s_msgWindowPane == nullptr || s_msgWindowPane->getPanePtr() == nullptr) {
        reset_msg_window();
        return false;
    }
    s_msgWindowPane->getPanePtr()->setBasePosition(J2DBasePosition_0);
    return true;
}

void qa_draw_msg_window(f32 x, f32 y, f32 w, f32 h, f32 alphaRate) {
    if (!load_msg_window()) {
        return;
    }
    const f32 initW = s_msgWindowPane->getInitSizeX();
    const f32 initH = s_msgWindowPane->getInitSizeY();
    if (initW <= 0.0f || initH <= 0.0f) {
        return;
    }

    s_msgWindowPane->move(x, y);
    s_msgWindowPane->scale(w / initW, h / initH);
    s_msgWindowPane->setAlphaRate(alphaRate);

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx != nullptr) {
        ctx->setup2D();
    }

    u32 scX = 0, scY = 0, scW = 0, scH = 0;
    GXGetScissor(&scX, &scY, &scW, &scH);
    s_msgWindowScreen->draw(0.0f, 0.0f, ctx);
    GXSetScissor(scX, scY, scW, scH);
}

void qa_invalidate_msg_window() {
    reset_msg_window();
}

static J2DPicture* s_slotFramePic = nullptr;
static ResTIMG* s_slotFrameTex = nullptr;
static JUtility::TColor s_slotFrameBlack;
static JUtility::TColor s_slotFrameWhite;
static JUtility::TColor s_slotFrameCorners[4];
static bool s_slotFrameAttempted = false;

static J2DPicture* build_picture_from_copy(ResTIMG* tex) {
    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    if (rootHeap == nullptr || tex == nullptr) {
        return nullptr;
    }
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(rootHeap);
    J2DPicture* pic = JKR_NEW J2DPicture(tex);
    mDoExt_setCurrentHeap(oldHeap);
    return pic;
}

static void reset_slot_frame() {
    JKR_DELETE(s_slotFramePic);
    s_slotFramePic = nullptr;
    s_slotFrameAttempted = (s_slotFrameTex == nullptr);
}

static bool load_slot_frame() {
    if (s_slotFramePic != nullptr) {
        return true;
    }
    if (s_slotFrameTex != nullptr) {
        s_slotFramePic = build_picture_from_copy(s_slotFrameTex);
        return s_slotFramePic != nullptr;
    }
    if (s_slotFrameAttempted) {
        return false;
    }
    s_slotFrameAttempted = true;

    JKRArchive* arc = g_dComIfG_gameInfo.play.getCollectResArchive();
    if (arc == nullptr) {
        s_slotFrameAttempted = false;
        return false;
    }

    J2DScreen* screen = JKR_NEW J2DScreen();
    if (screen == nullptr) {
        return false;
    }
    if (!screen->setPriority("zelda_collect_soubi_screen.blo", 0x1020000, arc)) {
        JKR_DELETE(screen);
        return false;
    }
    dPaneClass_showNullPane(screen);

    J2DPane* framePane = screen->search(MULTI_CHAR('ken_g_0'));
    s_slotFrameTex = qa_extract_pane_texture(framePane);
    if (s_slotFrameTex != nullptr && framePane != nullptr) {
        J2DPicture* fp = static_cast<J2DPicture*>(framePane);
        s_slotFrameBlack = fp->getBlack();
        s_slotFrameWhite = fp->getWhite();
        s_slotFrameCorners[0] = fp->corner(0);
        s_slotFrameCorners[1] = fp->corner(1);
        s_slotFrameCorners[2] = fp->corner(2);
        s_slotFrameCorners[3] = fp->corner(3);
    }
    JKR_DELETE(screen);

    if (s_slotFrameTex == nullptr) {
        return false;
    }
    s_slotFramePic = build_picture_from_copy(s_slotFrameTex);
    if (s_slotFramePic != nullptr) {
        s_slotFramePic->setBlackWhite(s_slotFrameBlack, s_slotFrameWhite);
        s_slotFramePic->setCornerColor(s_slotFrameCorners[0], s_slotFrameCorners[1],
                                       s_slotFrameCorners[2], s_slotFrameCorners[3]);
    }
    return s_slotFramePic != nullptr;
}

void qa_draw_collection_slot(f32 cx, f32 cy, f32 size, u8 alpha, bool selected, bool assigned) {
    const f32 half = size * 0.5f;
    qa_draw_rounded_rect(cx - half + 2.0f, cy - half + 2.0f, size - 4.0f, size - 4.0f, 5.0f,
                         JUtility::TColor(14, 12, 8,
                                          static_cast<u8>(alpha * (selected ? 0.88f : 0.78f))));
    if (load_slot_frame() && s_slotFramePic != nullptr) {
        if (assigned) {
            s_slotFramePic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                          JUtility::TColor(255, 255, 0, 255));
        } else {
            s_slotFramePic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                          JUtility::TColor(107, 107, 107, 255));
        }
        s_slotFramePic->setAlpha(alpha);
        s_slotFramePic->draw(cx - half, cy - half, size, size, false, false, false);
    }
    if (selected) {
    }
}

static f32 s_hintHitL[4] = {};
static f32 s_hintHitR[4] = {};
static f32 s_hintHitY = 0.0f;
static bool s_hintHitValid = false;

static void draw_hint_row(f32 centerX, f32 y, u8 alpha) {
    struct HintEntry {
        int iconIdx;
        const char* label;
        const char* fallback;
    };
    static const HintEntry kHints[4] = {
        { 0, "Add/Remove", "A Add/Remove" },
        { 1, "Clear",      "Y Clear" },
        { 2, "Back",       "X Back" },
        { 3, "Close",      "B Close" },
    };

    const bool icons = load_button_icons();
    const f32 fontW = 8.0f;
    const f32 fontH = 10.0f;
    const f32 iconGap = 5.0f;
    const f32 groupGap = 26.0f;

    f32 segW[4];
    f32 total = groupGap * 3.0f;
    for (int i = 0; i < 4; i++) {
        segW[i] = qa_get_text_width(icons ? kHints[i].label : kHints[i].fallback, fontW);
        if (icons) {
            segW[i] += qa_hint_button_width(kHints[i].iconIdx, QA_HINT_ICON_H) + iconGap;
        }
        total += segW[i];
    }

    f32 x = centerX - total * 0.5f;
    for (int i = 0; i < 4; i++) {
        s_hintHitL[i] = x;
        if (icons) {
            qa_draw_hint_button(kHints[i].iconIdx, x, y - 11.0f, QA_HINT_ICON_H, alpha);
            x += qa_hint_button_width(kHints[i].iconIdx, QA_HINT_ICON_H) + iconGap;
        }
        JUtility::TColor creamTop(255, 248, 210, alpha);
        JUtility::TColor goldBot(235, 185, 65, alpha);
        const char* label = icons ? kHints[i].label : kHints[i].fallback;
        qa_draw_text(label, x, y, fontW, fontH, creamTop, goldBot, alpha);
        s_hintHitR[i] = x + qa_get_text_width(label, fontW);
        x += segW[i] + groupGap;
    }
    s_hintHitY = y;
    s_hintHitValid = true;
}

using QaPointerBeginContextFn = void (*)(int);
using QaPointerHitRectFn = bool (*)(f32, f32, f32, f32, f32);
using QaPointerSetHoverTargetFn = void (*)(u16);
using QaPointerConsumeClickFn = bool (*)();

static QaPointerBeginContextFn s_pointerBeginContext = nullptr;
static QaPointerHitRectFn s_pointerHitRect = nullptr;
static QaPointerSetHoverTargetFn s_pointerSetHoverTarget = nullptr;
static QaPointerConsumeClickFn s_pointerConsumeClick = nullptr;
static f32 s_pointerLayoutW = 0.0f;
static f32 s_pointerLayoutH = 0.0f;

static const int kQaPointerContext = 0x5141;
static const u16 kQaPointerSlotTarget = 0x100;
static const u16 kQaPointerHintTarget = 0x200;
static const u16 kQaPointerCustomizeTarget = 0x300;
static const f32 kQaPointerHintPadX = 8.0f;
static const f32 kQaPointerHintTop = 16.0f;
static const f32 kQaPointerHintBottom = 10.0f;
static const f32 kQaPointerSlotHalf = 26.0f;

template <class Fn>
static void resolve_pointer_fn(const HookService* hook_svc, ModContext* mod_ctx, const char* name,
                               Fn* out) {
    void* addr = nullptr;
    if (hook_svc->resolve == nullptr ||
        hook_svc->resolve(mod_ctx, name, &addr, nullptr) != MOD_OK || addr == nullptr) {
        return;
    }
    *out = reinterpret_cast<Fn>(addr);
}

void quick_access_edit_pointer_install(const HookService* hook_svc, ModContext* mod_ctx) {
    if (hook_svc == nullptr || mod_ctx == nullptr) {
        return;
    }
    resolve_pointer_fn(hook_svc, mod_ctx, "dusk::menu_pointer::begin_context", &s_pointerBeginContext);
    resolve_pointer_fn(hook_svc, mod_ctx, "dusk::menu_pointer::hit_rect", &s_pointerHitRect);
    resolve_pointer_fn(hook_svc, mod_ctx, "dusk::menu_pointer::set_hover_target",
                       &s_pointerSetHoverTarget);
    resolve_pointer_fn(hook_svc, mod_ctx, "dusk::menu_pointer::consume_click", &s_pointerConsumeClick);
}

static f32 s_menuHintL = 0.0f;
static f32 s_menuHintR = 0.0f;
static f32 s_menuHintY = 0.0f;
static f32 s_menuHintAnchorX = 0.0f;
static f32 s_menuHintAnchorY = 0.0f;
static bool s_menuHintValid = false;

void qa_pointer_clear_menu_hint() {
    s_menuHintValid = false;
}

void qa_pointer_set_menu_hint(f32 left, f32 right, f32 y, f32 anchorX, f32 anchorY) {
    s_menuHintL = left;
    s_menuHintR = right;
    s_menuHintY = y;
    s_menuHintAnchorX = anchorX;
    s_menuHintAnchorY = anchorY;
    s_menuHintValid = true;
}

static bool pointer_hits_cell(f32 cx, f32 cy, f32 halfW, f32 halfH, f32 anchorX, f32 anchorY,
                              f32 scale) {
    const f32 x = anchorX + (cx - anchorX) * scale;
    const f32 y = anchorY + (cy - anchorY) * scale;
    return s_pointerHitRect(x - halfW * scale, y - halfH * scale, x + halfW * scale,
                            y + halfH * scale, 0.0f);
}

static bool pointer_hits_hint(f32 left, f32 right, f32 y, f32 anchorX, f32 anchorY, f32 scale) {
    const f32 cy = y + (kQaPointerHintBottom - kQaPointerHintTop) * 0.5f;
    const f32 halfH = (kQaPointerHintTop + kQaPointerHintBottom) * 0.5f;
    const f32 halfW = (right - left) * 0.5f + kQaPointerHintPadX;
    return pointer_hits_cell((left + right) * 0.5f, cy, halfW, halfH, anchorX, anchorY, scale);
}

static void menu_hint_pointer_update() {
    if (!s_menuOpen || !s_menuHintValid || s_menuAlpha < 0.01f) {
        return;
    }
    s_pointerBeginContext(kQaPointerContext);
    if (!pointer_hits_hint(s_menuHintL, s_menuHintR, s_menuHintY, s_menuHintAnchorX,
                           s_menuHintAnchorY, qa_user_hud_scale())) {
        return;
    }
    s_pointerSetHoverTarget(kQaPointerCustomizeTarget);
    if (s_pointerConsumeClick()) {
        qa_enter_edit_mode();
    }
}

void quick_access_edit_pointer_update() {
    if (s_pointerBeginContext == nullptr || s_pointerHitRect == nullptr ||
        s_pointerSetHoverTarget == nullptr || s_pointerConsumeClick == nullptr) {
        return;
    }
    if (!s_editMode) {
        menu_hint_pointer_update();
        return;
    }
    if (s_menuAlpha < 0.01f || s_pointerLayoutW <= 0.0f) {
        return;
    }
    s_pointerBeginContext(kQaPointerContext);

    const f32 scale = qa_user_hud_scale();
    const f32 centerX = s_pointerLayoutW * 0.5f;

    if (s_hintHitValid) {
        for (int i = 0; i < 4; i++) {
            if (!pointer_hits_hint(s_hintHitL[i], s_hintHitR[i], s_hintHitY, centerX,
                                   s_pointerLayoutH, scale)) {
                continue;
            }
            s_pointerSetHoverTarget(static_cast<u16>(kQaPointerHintTarget + i));
            if (!s_pointerConsumeClick()) {
                return;
            }
            switch (i) {
            case 0:
                if (!quick_access_edit_toggle_current()) {
                    Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                }
                break;
            case 1:
                quick_access_edit_clear_slot();
                break;
            case 2:
                qa_edit_pointer_back();
                break;
            default:
                qa_edit_pointer_close();
                break;
            }
            return;
        }
    }
    const f32 slotsSpan = (QA_QUICK_SLOTS - 1) * 56.0f;
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        const f32 cx = centerX - slotsSpan * 0.5f + static_cast<f32>(i) * 56.0f;
        if (!pointer_hits_cell(cx, SLOTS_ROW_Y, kQaPointerSlotHalf, kQaPointerSlotHalf, centerX,
                               0.0f, scale)) {
            continue;
        }
        s_pointerSetHoverTarget(static_cast<u16>(kQaPointerSlotTarget + i));
        if (s_editSlot != i) {
            s_editSlot = i;
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        s_pointerConsumeClick();
        return;
    }

    const f32 gridSpan = (QA_EDIT_COLS - 1) * CELL_SPACING_X;
    for (int i = 0; i < s_editCount; i++) {
        const f32 cx = centerX - gridSpan * 0.5f + static_cast<f32>(i % QA_EDIT_COLS) * CELL_SPACING_X;
        const f32 cy = GRID_TOP_Y + static_cast<f32>(i / QA_EDIT_COLS) * CELL_SPACING_Y;
        if (!pointer_hits_cell(cx, cy, CELL_SPACING_X * 0.5f, CELL_SPACING_Y * 0.5f, centerX, 0.0f,
                               scale)) {
            continue;
        }
        s_pointerSetHoverTarget(static_cast<u16>(i));
        if (s_editCursor != i) {
            s_editCursor = i;
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        if (s_pointerConsumeClick() && !quick_access_edit_toggle_current()) {
            Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        return;
    }
}

void quick_access_edit_draw(f32 screenW, f32 screenH, u8 alpha, f32 glow) {
    (void)glow;
    s_pointerLayoutW = screenW;
    s_pointerLayoutH = screenH;
    s_hintHitValid = false;
    qa_hud_scale_begin(screenW * 0.5f, 0.0f);

    const f32 centerX = screenW * 0.5f;

    const f32 slotSlide = -(1.0f - s_menuAlpha) * 16.0f;
    qa_draw_msg_window(centerX - 140.0f, 34.0f + slotSlide, 280.0f, 52.0f, s_menuAlpha);

    qa_draw_msg_window(centerX - (screenW * 0.5f - 30.0f), 125.0f, screenW - 60.0f, 100.0f,
                    s_menuAlpha);

    u8 selectedCount = static_cast<u8>(qa_custom_count());
    char title[64];
    std::snprintf(title, sizeof(title), "Customize Quick Slots   %d/4", static_cast<int>(selectedCount));
    draw_centered_label(title, 4.0f, 10.0f, 13.0f, alpha,
                        JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha),
                        centerX - 26.0f);

    const f32 slotsSpan = (QA_QUICK_SLOTS - 1) * 56.0f;
    const int currentSlot = quick_access_edit_current_slot();
    const u8 assigned = qa_strip_assigned_item();
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        u8 itemNo = qa_custom_item(i);
        s_previewScale[i] += ((i == currentSlot ? 1.16f : 1.0f) - s_previewScale[i]) * 0.28f;
        const bool slotAssigned =
            (itemNo != QA_ITEM_NONE) && (itemNo == assigned);
        draw_quick_slot_box(centerX - slotsSpan * 0.5f + static_cast<f32>(i) * 56.0f,
                            SLOTS_ROW_Y, itemNo, slotAssigned,
                            i == currentSlot, alpha, s_previewScale[i]);
    }

    if (currentSlot >= 0) {
        dSelect_cursor_c* cursor = qa_sel_cursor(1);
        if (cursor != nullptr) {
            const f32 slotCx = centerX - slotsSpan * 0.5f + static_cast<f32>(currentSlot) * 56.0f;
            cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            cursor->setPos(slotCx, SLOTS_ROW_Y);
            cursor->setAlphaRate(s_menuAlpha);
            cursor->draw();
            J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
            if (port != nullptr) {
                port->setup2D();
            }
        }
    }

    if (s_editCount <= 0) {
        draw_centered_label("No items", GRID_TOP_Y, 9.0f, 11.5f, alpha,
                            JUtility::TColor(255, 248, 210, alpha), JUtility::TColor(235, 185, 65, alpha), centerX);
        qa_hud_scale_end();
        return;
    }

    f32 cursorX = centerX;
    f32 cursorY = GRID_TOP_Y;
    const f32 gridSpan = (QA_EDIT_COLS - 1) * CELL_SPACING_X;

    for (int i = 0; i < s_editCount; i++) {
        int row = i / QA_EDIT_COLS;
        int col = i % QA_EDIT_COLS;
        const f32 cx = centerX - gridSpan * 0.5f + static_cast<f32>(col) * CELL_SPACING_X;
        const f32 cy = GRID_TOP_Y + static_cast<f32>(row) * CELL_SPACING_Y;

        bool inSet = qa_custom_contains_family(s_editItems[i]);
        u8 iconAlpha = inSet ? static_cast<u8>(alpha * 0.40f) : alpha;
        const f32 scale = inSet ? 0.82f : 1.0f;

        qa_draw_collection_slot(cx, cy, 44.0f * scale, alpha, false, inSet);

        J2DPicture* pic = nullptr;
        ResTIMG* img = nullptr;
        J2DPicture* pic2 = nullptr;
        if (qa_get_item_icon(s_editItems[i], &pic, &img, &pic2)) {
            f32 targetW = 30.0f * scale;
            f32 targetH = 30.0f * scale;
            if (img != nullptr && img->width > 0 && img->height > 0 && img->width != img->height) {
                if (img->width > img->height) {
                    targetH = targetW * (static_cast<f32>(img->height) / static_cast<f32>(img->width));
                } else {
                    targetW = targetH * (static_cast<f32>(img->width) / static_cast<f32>(img->height));
                }
            }
            const f32 iconX = cx - targetW * 0.5f;
            const f32 iconY = cy - targetH * 0.5f;
            pic->setAlpha(iconAlpha);
            pic->draw(iconX, iconY, targetW, targetH, false, false, false);
            if (pic2 != nullptr) {
                pic2->setAlpha(iconAlpha);
                pic2->draw(iconX, iconY, targetW, targetH, false, false, false);
            }

            if (qa_is_item_available(s_editItems[i])) {
                qa_draw_item_ammo(s_editItems[i], iconX, iconY, targetW, targetH, iconAlpha);
            }
        }

        if (i == s_editCursor) {
            cursorX = cx;
            cursorY = cy;
        }
    }

    dSelect_cursor_c* cursor = qa_sel_cursor(0);
    if (cursor != nullptr) {
        cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
        cursor->setPos(cursorX, cursorY);
        cursor->setAlphaRate(s_menuAlpha);
        cursor->draw();
        J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
        if (port != nullptr) {
            port->setup2D();
        }
    }

    qa_hud_scale_end();
    qa_hud_scale_begin(screenW * 0.5f, screenH);
    draw_hint_row(centerX, static_cast<f32>(screenH) - 24.0f, static_cast<u8>(alpha * 0.95f));
    qa_hud_scale_end();
}
