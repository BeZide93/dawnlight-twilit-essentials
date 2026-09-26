#pragma once

#include "z_common.hpp"
#include "../boss_rush/boss_rush.hpp"
#include "../boss_rush/boss_rush_midna.hpp"

bool g_configCustomZButtonEnabled = false;
bool g_configZButtonEnabled = false;

ModContext* g_zModCtx = nullptr;

u8 g_zInventorySlot = 0xFF;
u8 g_zMixSlot = 0xFF;
u8 g_zPendingZSlot = 0xFF;

bool g_dpadLeftHeld = false;
bool g_dpadLeftTrig = false;
bool g_physZHeld = false;
bool g_physZTrig = false;

bool g_inSetSelectItemIndex = false;
dMenu_Ring_c* g_activeRing = nullptr;

alignas(32) static u8 s_staticTexBufMain[2][0x1000];
alignas(32) static u8 s_staticTexBufShine[2][0x1000];

u8* g_zTexBufMain[2] = { s_staticTexBufMain[0], s_staticTexBufMain[1] };
u8* g_zTexBufShine[2] = { s_staticTexBufShine[0], s_staticTexBufShine[1] };
u8 g_zTexBufIdx = 0;

J2DPicture* g_cachedZMainPic = nullptr;
f32 g_cachedZW = 32.0f;
f32 g_cachedZH = 32.0f;
u8 g_lastLoadedZItem = 0xFF;
bool g_zHasSecondLayer = false;

J2DPicture* g_drawDigitPic[3] = { nullptr, nullptr, nullptr };
dKantera_icon_c* g_zKanteraIcon = nullptr;

void ensure_z_buffers() {
    g_zTexBufMain[0] = s_staticTexBufMain[0];
    g_zTexBufMain[1] = s_staticTexBufMain[1];
    g_zTexBufShine[0] = s_staticTexBufShine[0];
    g_zTexBufShine[1] = s_staticTexBufShine[1];
}

void sync_z_item_state() {
    if (g_inSetSelectItemIndex) return;
    g_inSetSelectItemIndex = true;

    u8 zSlot = dComIfGs_getSelectItemIndex(2);
    if (zSlot == 0xFF || zSlot >= 24) {
        g_zInventorySlot = 0xFF;
        g_zMixSlot = 0xFF;
        dComIfGs_setSelectItemIndex(2, 0xFF);
        dComIfGs_setMixItemIndex(2, 0xFF);
        g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_NONE_e);
        g_inSetSelectItemIndex = false;
        return;
    }

    u8 rawItem = dComIfGs_getItem(zSlot, false);
    if (rawItem == 0xFF || rawItem == 0x00 || rawItem == dItemNo_NONE_e) {
        g_zInventorySlot = 0xFF;
        dComIfGs_setSelectItemIndex(2, 0xFF);
        dComIfGs_setMixItemIndex(2, 0xFF);
        g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_NONE_e);
        g_inSetSelectItemIndex = false;
        return;
    }

    g_zInventorySlot = zSlot;
    u8 zMix = dComIfGs_getMixItemIndex(2);
    g_zMixSlot = zMix;

    u8 combinedItem = combine_select_item(rawItem, zMix);

    dComIfGs_setSelectItemIndex(2, zSlot);
    dComIfGs_setMixItemIndex(2, zMix);
    g_dComIfG_gameInfo.play.setSelectItem(2, combinedItem);

    g_inSetSelectItemIndex = false;
}

void ensure_z_slot_initialized() {
    static bool s_initializedSave = false;

    if (isTitleOrMainMenu()) {
        s_initializedSave = false;
        g_zInventorySlot = 0xFF;
        g_zMixSlot = 0xFF;
        return;
    }

    if (!s_initializedSave) {
        s_initializedSave = true;
        u8 savedItemIdx = dComIfGs_getSelectItemIndex(2);
        if (savedItemIdx == 0xFF || savedItemIdx >= 24 ||
            (savedItemIdx == 0 && dComIfGs_getSelectItemIndex(0) == 0xFF && dComIfGs_getSelectItemIndex(1) == 0xFF)) {
            g_zInventorySlot = 0xFF;
            g_zMixSlot = 0xFF;
            dComIfGs_setSelectItemIndex(2, 0xFF);
            dComIfGs_setMixItemIndex(2, 0xFF);
            g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_NONE_e);
            return;
        }

        u8 itm = dComIfGs_getItem(savedItemIdx, false);
        if (itm != 0xFF && itm != 0x00 && itm != dItemNo_NONE_e) {
            g_zInventorySlot = savedItemIdx;
            sync_z_item_state();
            return;
        } else {
            g_zInventorySlot = 0xFF;
            g_zMixSlot = 0xFF;
            dComIfGs_setSelectItemIndex(2, 0xFF);
            dComIfGs_setMixItemIndex(2, 0xFF);
            g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_NONE_e);
            return;
        }
    }

    sync_z_item_state();
}

u8 find_slot_for_item(u8 itemNo) {
    if (itemNo == 0xFF || itemNo == dItemNo_NONE_e) {
        return 0xFF;
    }
    if (itemNo < 24 && dComIfGs_getItem(itemNo, false) != dItemNo_NONE_e) {
        return itemNo;
    }
    for (u8 slot = 0; slot < 24; slot++) {
        if (dComIfGs_getItem(slot, false) == itemNo) {
            return slot;
        }
    }
    return 0xFF;
}

bool is_bomb_item(u8 itemNo) {
    return itemNo == 0x4F || itemNo == 0x50 || itemNo == 0x51 || itemNo == 0x70 || itemNo == 0x71 || itemNo == 0x72;
}

bool g_zDimX = false;
bool g_zDimY = false;

bool z_items_dimmed() {
    if (dMeter2Info_getWindowStatus() == 2) {
        return false;
    }
    return !dMeter2Info_isUseButton(METER2_USEBUTTON_X) && !dMeter2Info_isUseButton(METER2_USEBUTTON_Y);
}

u8 z_item_icon_alpha() {
    if (z_mobile_wants_midona_host()) {
        return 255;
    }
    if (daAlink_c::checkRoom()) {
        return g_drawHIO.mButtonXYItemDimAlpha;
    }
    return z_items_dimmed() ? g_drawHIO.mButtonXYItemDimAlpha : 255;
}

u8 z_button_base_alpha() {
    if (daAlink_c::checkRoom()) {
        return g_drawHIO.mButtonXYBaseDimAlpha;
    }
    return z_items_dimmed() ? g_drawHIO.mButtonXYBaseDimAlpha : 255;
}

bool z_item_is_lantern(u8 itemNo) {
    return itemNo == dItemNo_KANTERA_e || itemNo == dItemNo_KANTERA2_e;
}

bool z_item_ammo(u8 itemNo, int& count, int& maxCount) {
    count = -1;
    maxCount = -1;

    if (itemNo == dItemNo_BOMB_ARROW_e || itemNo == 0x59 || is_bomb_item(itemNo)) {
        u8 bombSlot = dComIfGs_getSelectMixItemNoArrowIndex(2);
        u8 bagIdx = (bombSlot >= 15 && bombSlot < 18) ? (bombSlot - 15) : 0;
        u8 bombType = dComIfGs_getItem((u8)(bagIdx + 15), false);
        count = dComIfGs_getBombNum(bagIdx);
        maxCount = dComIfGs_getBombMax(bombType);
    }
    else if (itemNo == dItemNo_BOW_e || itemNo == dItemNo_HAWK_ARROW_e || itemNo == 0x43 ||
             itemNo == 0x53 || itemNo == 0x54 || itemNo == 0x55 || itemNo == 0x56 || itemNo == 0x5A) {
        count = dComIfGs_getArrowNum();
        maxCount = dComIfGs_getArrowMax();
    }
    else if (itemNo == 0x4B || itemNo == 0x76) {
        count = dComIfGs_getPachinkoNum();
        maxCount = dComIfGs_getPachinkoMax();
    }
    else if (itemNo == dItemNo_BEE_CHILD_e || itemNo == dItemNo_BEE_ROD_e) {
        u8 bottleSlot = dComIfGs_getSelectItemIndex(2);
        u8 bottleIdx = (bottleSlot >= 11 && bottleSlot < 15) ? (bottleSlot - 11) : 0;
        count = dComIfGs_getBottleNum(bottleIdx);
        maxCount = 10;
    }

    return count >= 0;
}

bool isWolfPlayer() {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    return player != nullptr && player->checkWolf();
}

bool isTitleOrMainMenu() {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    if (player == nullptr) {
        return true;
    }
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr) {
        if (std::strcmp(stageName, "F_SP102") == 0 ||
            std::strcmp(stageName, "title") == 0 ||
            std::strcmp(stageName, "opening") == 0 ||
            std::strcmp(stageName, "name") == 0) {
            return true;
        }
    }
    return false;
}

bool isMidnaUnlocked() {
    if (is_boss_rush_active()) {
        return true;
    }
    if (isWolfPlayer()) {
        return true;
    }
    if (dComIfGs_getTransformStatus() != 0) {
        return true;
    }
    if (dComIfGs_isEventBit(0x0520) || dComIfGs_isEventBit(0x0510) ||
        dComIfGs_isEventBit(0x0501) || dComIfGs_isEventBit(0x0640) ||
        dComIfGs_isEventBit(0x0504) || dComIfGs_isEventBit(0x0502)) {
        return true;
    }
    return false;
}

bool is_pause_menu_open(dMeter2Draw_c* draw) {
    const u8 winStatus = g_meter2_info.getWindowStatus();
    if (winStatus == 2) {
        return false;
    }

    if (dComIfGp_event_runCheck()) return true;
    if (dComIfGp_isPauseFlag()) return true;
    if (g_meter2_info.getPauseStatus() != 0) return true;
    if (winStatus != 0) return true;
    if (dMeter2Info_isShopTalkFlag()) return true;
    if (dMsgObject_isTalkNowCheck()) return true;

#if !Z_MOBILE_BUILD
    if (draw == nullptr && g_meter2_info.getMeterClass() != nullptr) {
        draw = g_meter2_info.getMeterClass()->getMeterDrawPtr();
    }
    if (draw != nullptr && draw->getMainScreenPtr() != nullptr) {
        J2DScreen* screen = draw->getMainScreenPtr();
        J2DPane* contPane = screen->search(MULTI_CHAR('cont_n'));
        if (contPane != nullptr && (!contPane->isVisible() || contPane->getAlpha() == 0)) {
            return true;
        }
    }
#else
    (void)draw;
#endif
    return false;
}

bool is_z_item_usable() {
    u8 winStatus = dMeter2Info_getWindowStatus();
    if (winStatus != 0) {
        return true;
    }
    if (isWolfPlayer()) {
        return false;
    }
    if (g_zInventorySlot == 0xFF) {
        return false;
    }
    u8 zItem = dComIfGs_getItem(g_zInventorySlot, false);
    if (zItem == 0xFF || zItem == 0x00 || zItem == dItemNo_NONE_e) {
        return false;
    }
    if (!dMeter2Info_isUseButton(4) && !dMeter2Info_isUseButton(8)) {
        return false;
    }
    return true;
}

void set_pane_influenced_alpha_recursive(J2DPane* pane, bool influenced) {
    if (!pane) return;
    pane->setInfluencedAlpha(influenced, true);
    JSUTreeIterator<J2DPane> it(pane->getFirstChild());
    while (it != pane->getEndChild()) {
        if (it.getObject() != nullptr) {
            set_pane_influenced_alpha_recursive(it.getObject(), influenced);
        }
        ++it;
    }
}

u8 combine_select_item(u8 playItem, u8 mixSlot) {
    if (mixSlot == dItemNo_NONE_e || mixSlot == 0xFF) {
        return playItem;
    }

    u8 saveItem = (mixSlot < 24) ? dComIfGs_getItem(mixSlot, false) : dItemNo_NONE_e;
    if (saveItem == dItemNo_BOW_e || mixSlot == 4) {
        saveItem = playItem;
        playItem = dItemNo_BOW_e;
    } else if (saveItem == dItemNo_FISHING_ROD_1_e) {
        saveItem = playItem;
        playItem = dItemNo_FISHING_ROD_1_e;
    }

    if (playItem == dItemNo_BOW_e) {
        switch (saveItem) {
        case dItemNo_NORMAL_BOMB_e:
        case dItemNo_WATER_BOMB_e:
        case dItemNo_POKE_BOMB_e:
        case dItemNo_BOMB_BAG_LV1_e:
            return dItemNo_BOMB_ARROW_e;
        case dItemNo_HAWK_EYE_e:
            return dItemNo_HAWK_ARROW_e;
        default:
            break;
        }
    } else if (playItem == dItemNo_FISHING_ROD_1_e) {
        switch (saveItem) {
        case dItemNo_BEE_CHILD_e:
            return dItemNo_BEE_ROD_e;
        case dItemNo_WORM_e:
            return dItemNo_WORM_ROD_e;
        case dItemNo_ZORAS_JEWEL_e:
            return dItemNo_JEWEL_ROD_e;
        default:
            break;
        }
    }

    return playItem;
}

u8 resolved_select_item(int index) {
    u8 slot = (index == 2) ? g_zInventorySlot : dComIfGs_getSelectItemIndex(index);
    if (slot == dItemNo_NONE_e || slot == 0xFF || slot >= 24) {
        return dItemNo_NONE_e;
    }

    u8 mixSlot = (index == 2) ? g_zMixSlot : dComIfGs_getMixItemIndex(index);
    return combine_select_item(dComIfGs_getItem(slot, false), mixSlot);
}

void sync_play_select_item(int index) {
    if (index != 2) return;
    g_dComIfG_gameInfo.play.setSelectItem(2, resolved_select_item(2));
}
