#include "quick_access_bottles.hpp"
#include "quick_access_internal.hpp"

#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_s_play.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#define private public
#define protected public
#include "d/d_meter2_draw.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#undef protected
#undef private
#include "d/d_select_cursor.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "../z_button/z_common.hpp"
#include "../controls/controls.hpp"
#include "mods/svc/save.h"

#include <dolphin/gx.h>

#include <cmath>
#include <cstdio>

bool g_configBottlesQuickAccessEnabled = false;

static const f32 QB_BOX_HALF = 20.0f;
static const f32 QB_BOX_SPACING = 56.0f;
static const f32 QB_BAR_Y = 60.0f;
static const f32 QB_WHEEL_RADIUS = 92.0f;

static const int QB_REPEAT_START_INTERVAL = 8;
static const int QB_REPEAT_ACCEL_STEP = 1;
static const int QB_REPEAT_MIN_INTERVAL = 2;

static ModContext* s_bottleModCtx = nullptr;
static const SaveService* s_bottleSaveSvc = nullptr;
static SaveObserverHandle s_bottleSaveObserver = 0;
static const char* kBottleBlobName = "quickAccessBottlesV1";

struct BottleBlob {
    u8 assignedSlot;
    u8 pad[7];
};
static_assert(sizeof(BottleBlob) == 8, "blob size");

static bool s_bottleMenuOpen = false;
static int s_bottleSelectedSlot = SLOT_NONE;
static int s_holdFramesL = 0;
static bool s_cancelLatchL = false;
static u8 s_assignedSlot = 0xFF;
static f32 s_bottleMenuAlpha = 0.0f;
static f32 s_bottleGlowTimer = 0.0f;
static f32 s_slotScale[QA_QUICK_SLOTS] = { 1.0f, 1.0f, 1.0f, 1.0f };

struct BottleRepeat {
    int dir;
    int timer;
    int count;
};
static BottleRepeat s_stickRepeat;
static BottleRepeat s_shoulderRepeat;

static bool s_hotkeyActive = false;

static dMeter2Draw_c* s_lastDraw = nullptr;
static J2DScreen* s_lastScreen = nullptr;

static u8 bottle_item(int idx) {
    if (idx < 0 || idx >= 4) {
        return dItemNo_NONE_e;
    }
    return dComIfGs_getItem(SLOT_11 + idx, false);
}

static bool bottle_owned(int idx) {
    return bottle_item(idx) != dItemNo_NONE_e;
}

static int bottle_count() {
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (bottle_owned(i)) {
            n++;
        }
    }
    return n;
}

static void bottles_reset_assignment() {
    s_assignedSlot = 0xFF;
}

static void bottles_store() {
    if (s_bottleSaveSvc == nullptr || s_bottleModCtx == nullptr) {
        return;
    }
    BottleBlob blob{};
    blob.assignedSlot = s_assignedSlot;
    s_bottleSaveSvc->set_blob(s_bottleModCtx, kBottleBlobName, &blob, sizeof(blob));
}

static void on_bottles_new_save(ModContext*, uint32_t, void*) {
    bottles_reset_assignment();
}

static void on_bottles_save_loaded(ModContext* ctx, uint32_t, void*) {
    bottles_reset_assignment();
    if (s_bottleSaveSvc == nullptr) {
        return;
    }

    BottleBlob blob{};
    size_t size = sizeof(blob);
    if (s_bottleSaveSvc->get_blob(ctx, kBottleBlobName, &blob, &size) == MOD_OK &&
        size == sizeof(blob) && blob.assignedSlot < 4) {
        s_assignedSlot = blob.assignedSlot;
    }
}

static bool bottles_is_title_or_main_menu() {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    if (player == nullptr) {
        return true;
    }
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr) {
        if (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0) {
            return true;
        }
    }
    return false;
}

static bool bottles_is_wolf() {
    daAlink_c* player = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    return (player != nullptr && player->checkWolf());
}

static bool bottles_is_menu_or_pause() {
    u8 windowStatus = dMeter2Info_getWindowStatus();
    return (windowStatus != 0) || dComIfGp_isPauseFlag() || dScnPly_c::isPause()
           || dComIfGp_event_runCheck() || dMeter2Info_isShopTalkFlag()
           || dMsgObject_isTalkNowCheck() || bottles_is_wolf();
}

static void bottles_play_error_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void bottles_play_ok_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void bottles_play_cursor_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void bottles_reset_repeat() {
    s_stickRepeat = BottleRepeat{};
    s_shoulderRepeat = BottleRepeat{};
}

static void close_bottle_menu() {
    s_bottleMenuOpen = false;
    s_holdFramesL = 0;
    bottles_reset_repeat();
}

static void open_bottle_menu() {
    s_bottleMenuOpen = true;
    bottles_reset_repeat();

    /* Start with nothing selected; the player picks a slot with the stick. */
    s_bottleSelectedSlot = SLOT_NONE;

    qa_invalidate_msg_window();
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 0.9f, 1.2f, -1.0f, -1.0f, 0);
}

static void eat_bottle_trigger(interface_of_controller_pad& pad) {
    const u16 bottleBit = controls_binding_bit(CTRL_BIND_BOTTLES);
    if (bottleBit == PAD_TRIGGER_L) {
        pad.mTriggerLeft = 0.0f;
        pad.mTrigLockL = false;
        pad.mHoldLockL = false;
    }
    pad.mPressedButtonFlags &= ~bottleBit;
    pad.mButtonFlags &= ~bottleBit;
}

static void eat_all_triggers(interface_of_controller_pad& pad) {
    pad.mTriggerLeft = 0.0f;
    pad.mTriggerRight = 0.0f;
    pad.mTrigLockL = false;
    pad.mTrigLockR = false;
    pad.mHoldLockL = false;
    pad.mHoldLockR = false;
    pad.mPressedButtonFlags &= ~(PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_TRIGGER_Z);
    pad.mButtonFlags &= ~(PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_TRIGGER_Z);
}

static void bottles_update_axis_repeat(int dir, BottleRepeat& state, void (*step)(int)) {
    if (dir != 0) {
        if (dir != state.dir) {
            state.dir = dir;
            state.timer = 0;
            state.count = 0;
            step(dir);
        } else {
            state.timer++;
            int interval = QB_REPEAT_START_INTERVAL - state.count * QB_REPEAT_ACCEL_STEP;
            if (interval < QB_REPEAT_MIN_INTERVAL) {
                interval = QB_REPEAT_MIN_INTERVAL;
            }
            if (state.timer >= interval) {
                state.timer = 0;
                state.count++;
                step(dir);
            }
        }
    } else {
        state = BottleRepeat{};
    }
}

static void bottles_cycle_step(int dir) {
    s_bottleSelectedSlot = (s_bottleSelectedSlot + dir + QA_QUICK_SLOTS) % QA_QUICK_SLOTS;
    bottles_play_cursor_se();
}

static void bottles_suppress_pad(interface_of_controller_pad& pad) {
    const u16 qaBit = controls_binding_bit(CTRL_BIND_QUICK_ACCESS);
    pad.mPressedButtonFlags &= ~qaBit;
    pad.mButtonFlags &= ~qaBit;

    pad.mCStickPosX = 0.0f;
    pad.mCStickPosY = 0.0f;
    pad.mCStickValue = 0.0f;

    if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
        pad.mButtonFlags &= ~PAD_BUTTON_A;
    }
}

bool quick_access_bottles_hotkey_active() {
    return s_hotkeyActive;
}

static const int QB_ITEM_PROC_KANDELAAR_POUR = 8;
static const int QB_ITEM_PROC_COMMON_CHANGE_ITEM = 12;

static u8 s_pendingSlot = 0xFF;
static u8 s_pendingItem = 0;
static u16 s_pendingProc = 0;
static int s_pendingFrames = 0;
static const int QB_PENDING_MAX_FRAMES = 900;

static u8 s_preSelectIndex = 0xFF;
static u8 s_preSelectPlay = 0;

static bool bottles_lantern_equipped() {
    for (int i = 0; i < 3; i++) {
        const u8 sel = dComIfGp_getSelectItem(i);
        if (sel == dItemNo_KANTERA_e || sel == dItemNo_KANTERA2_e) {
            return true;
        }
        if (i == 2) {
            const u8 idx = dComIfGs_getSelectItemIndex(2);
            if (idx < 24) {
                const u8 item = dComIfGs_getItem(idx, false);
                if (item == dItemNo_KANTERA_e || item == dItemNo_KANTERA2_e) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void bottles_finish_pending() {
    const u8 result = (s_pendingItem == dItemNo_MILK_BOTTLE_e)
                          ? static_cast<u8>(dItemNo_HALF_MILK_BOTTLE_e)
                          : static_cast<u8>(dItemNo_EMPTY_BOTTLE_e);
    dComIfGs_setBottleItemIn(s_pendingItem, result);

    if (dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN) != s_preSelectIndex) {
        dComIfGs_setSelectItemIndex(SELECT_ITEM_DOWN, s_preSelectIndex);
    }
    if (dComIfGp_getSelectItem(SELECT_ITEM_DOWN) != s_preSelectPlay) {
        g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_DOWN, s_preSelectPlay);
    }

    if (dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN) == SLOT_11 + s_pendingSlot) {
        dComIfGs_setSelectItemIndex(SELECT_ITEM_DOWN, 0xFF);
        g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_DOWN, dItemNo_NONE_e);
        if (g_zInventorySlot == SLOT_11 + s_pendingSlot) {
            g_zInventorySlot = 0xFF;
            g_zMixSlot = 0xFF;
        }
    }

    s_pendingSlot = 0xFF;
}

static void bottles_use_bottle(int slotIdx) {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr) {
        return;
    }

    const u8 item = bottle_item(slotIdx);
    if (item == dItemNo_NONE_e) {
        bottles_play_error_se();
        return;
    }

    const bool isOil = link->checkOilBottleItem(item);
    if (isOil && !bottles_lantern_equipped()) {
        bottles_play_error_se();
        return;
    }

    const u16 procBefore = link->mProcID;
    const u8 playBefore = dComIfGp_getSelectItem(SELECT_ITEM_DOWN);
    s_preSelectIndex = dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN);
    s_preSelectPlay = playBefore;

    g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_DOWN, item);

    int proc = link->checkNewItemChange(SELECT_ITEM_DOWN);
    if (isOil && proc == QB_ITEM_PROC_COMMON_CHANGE_ITEM) {
        proc = QB_ITEM_PROC_KANDELAAR_POUR;
    }

    if (proc == 0) {
        bottles_play_error_se();
        return;
    }

    link->changeItemTriggerKeepProc(SELECT_ITEM_DOWN, proc);
    g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_DOWN, playBefore);

    if (link->mProcID != procBefore) {
        s_pendingSlot = static_cast<u8>(slotIdx);
        s_pendingItem = item;
        s_pendingProc = link->mProcID;
        s_pendingFrames = 0;
        bottles_play_ok_se();
    } else {
        bottles_play_error_se();
    }
}

DEFINE_HOOK(&mDoCPd_c::read, PadReadBottlesHook);

static void bottles_radial_select(f32 stickX, f32 stickY, f32 stickMag);

static void on_pad_read_bottles_post(ModContext*, void*, void*, void*) {
    s_hotkeyActive = false;

    if (s_pendingSlot != 0xFF) {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link == nullptr || link->mProcID != s_pendingProc ||
            ++s_pendingFrames >= QB_PENDING_MAX_FRAMES) {
            bottles_finish_pending();
        }
    }

    if (!g_configBottlesQuickAccessEnabled || bottles_is_title_or_main_menu()) {
        close_bottle_menu();
        s_cancelLatchL = false;
        return;
    }

    if (quick_access_is_active() || s_editMode) {
        if (s_bottleMenuOpen) {
            close_bottle_menu();
        }
        s_holdFramesL = 0;
        return;
    }

    if (bottles_is_menu_or_pause()) {
        close_bottle_menu();
        return;
    }

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const u16 bottleBit = controls_binding_bit(CTRL_BIND_BOTTLES);
    const bool lPhys = (pad.mButtonFlags & bottleBit) != 0;

    bool lHeld = lPhys;
    if (s_cancelLatchL) {
        if (!lPhys) {
            s_cancelLatchL = false;
        }
        lHeld = false;
    }

    if (s_bottleMenuOpen) {
        s_hotkeyActive = true;
        eat_bottle_trigger(pad);

        if (g_configQuickAccessAppearance == QA_APPEARANCE_RADIAL) {
            bottles_radial_select(pad.mCStickPosX, pad.mCStickPosY,
                                  std::sqrt(pad.mCStickPosX * pad.mCStickPosX +
                                            pad.mCStickPosY * pad.mCStickPosY));
        } else {
            const f32 stickX = pad.mCStickPosX;
            const f32 kDeadzone = 0.35f;
            const int stickDir = (stickX >= kDeadzone) ? 1 : (stickX <= -kDeadzone) ? -1 : 0;
            bottles_update_axis_repeat(stickDir, s_stickRepeat, bottles_cycle_step);

            if ((pad.mPressedButtonFlags & PAD_BUTTON_LEFT) != 0) {
                pad.mPressedButtonFlags &= ~PAD_BUTTON_LEFT;
                pad.mButtonFlags &= ~PAD_BUTTON_LEFT;
                bottles_cycle_step(-1);
            }
            if ((pad.mPressedButtonFlags & PAD_BUTTON_RIGHT) != 0) {
                pad.mPressedButtonFlags &= ~PAD_BUTTON_RIGHT;
                pad.mButtonFlags &= ~PAD_BUTTON_RIGHT;
                bottles_cycle_step(1);
            }
            bottles_update_axis_repeat((pad.mButtonFlags & PAD_TRIGGER_R) ? 1 : 0,
                                       s_shoulderRepeat, bottles_cycle_step);
        }

        pad.mButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);
        pad.mPressedButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);

        if ((pad.mPressedButtonFlags & PAD_BUTTON_B) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
            pad.mButtonFlags &= ~PAD_BUTTON_B;
            s_cancelLatchL = true;
            close_bottle_menu();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            bottles_suppress_pad(pad);
            eat_all_triggers(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
            pad.mButtonFlags &= ~PAD_BUTTON_A;
            s_cancelLatchL = true;
            close_bottle_menu();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            bottles_suppress_pad(pad);
            eat_all_triggers(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_X) != 0 && g_configQuickAccessEnabled) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_X;
            pad.mButtonFlags &= ~PAD_BUTTON_X;
            close_bottle_menu();
            qa_enter_edit_mode();
            bottles_suppress_pad(pad);
            eat_all_triggers(pad);
            return;
        }

        if (!lHeld) {
            close_bottle_menu();
            if (s_bottleSelectedSlot >= 0 && s_bottleSelectedSlot < QA_QUICK_SLOTS) {
                if (bottle_owned(s_bottleSelectedSlot)) {
                    if (s_assignedSlot != static_cast<u8>(s_bottleSelectedSlot)) {
                        s_assignedSlot = static_cast<u8>(s_bottleSelectedSlot);
                        bottles_store();
                    }
                    bottles_use_bottle(s_bottleSelectedSlot);
                } else {
                    bottles_play_error_se();
                }
            }
        }

        bottles_suppress_pad(pad);
        eat_all_triggers(pad);
        return;
    }

    if (lHeld) {
        s_hotkeyActive = true;
        eat_bottle_trigger(pad);

        const u16 qaBit = controls_binding_bit(CTRL_BIND_QUICK_ACCESS);
        pad.mPressedButtonFlags &= ~qaBit;
        pad.mButtonFlags &= ~qaBit;

        s_holdFramesL++;
        if (s_holdFramesL >= QA_TAP_FRAMES && bottle_count() > 0) {
            open_bottle_menu();
        }
        return;
    }
    if (s_holdFramesL > 0) {
        s_holdFramesL = 0;
        if (s_assignedSlot < 4 && bottle_owned(s_assignedSlot)) {
        } else {
        }
    }
}

static void bottles_draw_icon(J2DPicture* pic, ResTIMG* img, f32 sx, f32 sy, f32 scale, u8 alpha,
                              J2DPicture* pic2 = nullptr) {
    f32 targetW = 34.0f * scale;
    f32 targetH = 34.0f * scale;
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
}

static void draw_bottle_hint(f32 centerX, f32 screenH, u8 alpha) {
    const f32 hintY = screenH - 24.0f;
    const char* releaseText = "Release: Use Bottle";
    const char* cancelText = "Cancel";
    const f32 fontW = 8.0f;
    const f32 fontH = 10.0f;
    const f32 groupGap = 18.0f;
    const f32 iconGap = 5.0f;
    const f32 iconH = 17.0f;

    const bool iconsReady = qa_hint_button_ready();
    const f32 iconW = iconsReady ? qa_hint_button_width(3, iconH) : 0.0f;
    const char* cancelLabel = iconsReady ? cancelText : "B Cancel";

    const f32 releaseW = qa_get_text_width(releaseText, fontW);
    const f32 cancelW = qa_get_text_width(cancelLabel, fontW);
    const f32 cancelSegW = cancelW + (iconsReady ? iconW + iconGap : 0.0f);
    const f32 total = releaseW + groupGap + cancelSegW;
    f32 x = centerX - total * 0.5f;

    JUtility::TColor creamTop(255, 248, 210, alpha);
    JUtility::TColor goldBot(235, 185, 65, alpha);
    qa_draw_text(releaseText, x, hintY, fontW, fontH, creamTop, goldBot, alpha);
    x += releaseW + groupGap;
    if (iconsReady) {
        qa_draw_hint_button(3, x, hintY - 12.0f, iconH, alpha);
        x += iconW + iconGap;
    }
    qa_draw_text(cancelLabel, x, hintY, fontW, fontH, creamTop, goldBot, alpha);
}

static void draw_bottle_bar(f32 screenW, f32 screenH, u8 alpha) {
    const f32 centerX = screenW * 0.5f;
    const f32 centerY = QB_BAR_Y;

    const f32 slide = -(1.0f - s_bottleMenuAlpha) * 16.0f;
    const u8 slotAlpha = static_cast<u8>(s_bottleMenuAlpha * 255.0f);

    const f32 totalSpan = static_cast<f32>(QA_QUICK_SLOTS - 1) * QB_BOX_SPACING;
    const f32 windowW = totalSpan + 112.0f;

    qa_draw_msg_window(centerX - windowW * 0.5f, 34.0f + slide, windowW, 52.0f, s_bottleMenuAlpha);

    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        const bool owned = bottle_owned(i);
        const u8 itemNo = bottle_item(i);
        const bool isSelected = (s_bottleSelectedSlot == i);

        s_slotScale[i] += ((isSelected ? 1.16f : 1.0f) - s_slotScale[i]) * 0.28f;
        const f32 half = QB_BOX_HALF * s_slotScale[i];

        const f32 cx = centerX - totalSpan * 0.5f + static_cast<f32>(i) * QB_BOX_SPACING;
        const f32 cy = centerY + slide;

        qa_draw_collection_slot(cx, cy, half * 2.0f, slotAlpha, isSelected, false);

        J2DPicture* pic = nullptr;
        ResTIMG* img = nullptr;
        J2DPicture* pic2 = nullptr;
        if (owned && qa_get_item_icon(itemNo, &pic, &img, &pic2)) {
            bottles_draw_icon(pic, img, cx, cy, s_slotScale[i] * 0.94f, slotAlpha, pic2);
        }
    }

    if (s_bottleSelectedSlot >= 0 && s_bottleSelectedSlot < QA_QUICK_SLOTS) {
        dSelect_cursor_c* cursor = qa_sel_cursor(0);
        if (cursor != nullptr) {
            const f32 cx = centerX - totalSpan * 0.5f + static_cast<f32>(s_bottleSelectedSlot) * QB_BOX_SPACING;
            const f32 cy = centerY + slide;
            cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            cursor->setPos(cx, cy);
            cursor->setAlphaRate(s_bottleMenuAlpha);
            cursor->draw();
            J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
            if (port != nullptr) {
                port->setup2D();
            }
        }
    }

    if (s_bottleSelectedSlot >= 0 && s_bottleSelectedSlot < QA_QUICK_SLOTS) {
        const u8 sel = bottle_item(s_bottleSelectedSlot);
        if (sel != dItemNo_NONE_e) {
            char labelBuf[64] = "";
            qa_item_label(sel, labelBuf, sizeof(labelBuf));
            if (labelBuf[0] != '\0') {
                const f32 fontW = 9.0f;
                const f32 fontH = 11.5f;
                f32 textW = qa_get_text_width(labelBuf, fontW);
                const f32 textX = centerX - textW * 0.5f;
                const f32 textY = QB_BAR_Y - QB_BOX_HALF - 22.0f + slide;

                JUtility::TColor creamTop(255, 248, 210, alpha);
                JUtility::TColor goldBot(235, 185, 65, alpha);
                qa_draw_text(labelBuf, textX, textY + 10.0f, fontW, fontH, creamTop, goldBot, alpha);
            }
        }
    }

    draw_bottle_hint(centerX, screenH, alpha);
}

static void bottles_radial_select(f32 stickX, f32 stickY, f32 stickMag) {
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

    if (newSlot != s_bottleSelectedSlot && newSlot != SLOT_NONE && bottle_owned(newSlot)) {
        s_bottleSelectedSlot = newSlot;
        bottles_play_cursor_se();
    }
}

static void bottles_radial_draw(f32 screenW, f32 screenH, u8 alpha) {
    f32 centerX = screenW * 0.5f;
    f32 centerY = screenH * 0.5f;

    centerY -= (1.0f - s_bottleMenuAlpha) * 16.0f;

    qa_radial_draw_wheel(centerX, centerY, alpha, s_bottleMenuAlpha);

    const f32 slotCoords[QA_QUICK_SLOTS][2] = {
        { centerX, centerY - QB_WHEEL_RADIUS },
        { centerX, centerY + QB_WHEEL_RADIUS },
        { centerX - QB_WHEEL_RADIUS, centerY },
        { centerX + QB_WHEEL_RADIUS, centerY },
    };

    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        const f32 sx = slotCoords[i][0];
        const f32 sy = slotCoords[i][1];
        const bool owned = bottle_owned(i);
        const bool isSelected = (s_bottleSelectedSlot == i);

        s_slotScale[i] += ((isSelected ? 1.16f : 1.0f) - s_slotScale[i]) * 0.28f;
        const f32 scale = s_slotScale[i];

        const u8 slotAlpha = owned ? alpha : static_cast<u8>(alpha * 0.45f);
        qa_draw_collection_slot(sx, sy, QB_BOX_HALF * 2.0f * scale, slotAlpha, isSelected, false);

        J2DPicture* pic = nullptr;
        ResTIMG* img = nullptr;
        J2DPicture* pic2 = nullptr;
        if (owned && qa_get_item_icon(bottle_item(i), &pic, &img, &pic2)) {
            bottles_draw_icon(pic, img, sx, sy, scale * 0.94f, alpha, pic2);
        }
    }

    if (s_bottleSelectedSlot >= 0 && s_bottleSelectedSlot < QA_QUICK_SLOTS) {
        dSelect_cursor_c* cursor = qa_sel_cursor(0);
        if (cursor != nullptr) {
            cursor->setParam(1.0f, 1.0f, 0.1f, 0.6f, 0.5f);
            cursor->setPos(slotCoords[s_bottleSelectedSlot][0], slotCoords[s_bottleSelectedSlot][1]);
            cursor->setAlphaRate(s_bottleMenuAlpha);
            cursor->draw();
            J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
            if (port != nullptr) {
                port->setup2D();
            }
        }
    }

    if (s_bottleSelectedSlot >= 0 && s_bottleSelectedSlot < QA_QUICK_SLOTS && bottle_owned(s_bottleSelectedSlot)) {
        const u8 sel = bottle_item(s_bottleSelectedSlot);
        if (sel != dItemNo_NONE_e) {
            char labelBuf[64] = "";
            qa_item_label(sel, labelBuf, sizeof(labelBuf));
            if (labelBuf[0] != '\0') {
                const f32 fontW = 9.0f;
                const f32 fontH = 11.5f;
                const f32 textW = qa_get_text_width(labelBuf, fontW);
                const f32 textX = slotCoords[s_bottleSelectedSlot][0] - textW * 0.5f;
                const f32 textY = slotCoords[s_bottleSelectedSlot][1] + 32.0f;

                JUtility::TColor creamTop(255, 248, 210, alpha);
                JUtility::TColor goldBot(235, 185, 65, alpha);
                qa_draw_text(labelBuf, textX, textY, fontW, fontH, creamTop, goldBot, alpha);
            }
        }
    }

    draw_bottle_hint(screenW * 0.5f, screenH, alpha);
}

DEFINE_HOOK(&dMeter2Draw_c::draw, Meter2DrawBottlesHook);

static void on_meter2_draw_bottles_post(ModContext*, void* args, void*, void*) {
    const bool radialStyle = (g_configQuickAccessAppearance == QA_APPEARANCE_RADIAL);

    if (!args || !g_configBottlesQuickAccessEnabled || bottles_is_title_or_main_menu()
        || bottles_is_wolf()) {
        s_bottleMenuAlpha = 0.0f;
        return;
    }

    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (!draw || !draw->getMainScreenPtr()) {
        return;
    }

    J2DScreen* screen = draw->getMainScreenPtr();
    if (draw != s_lastDraw || screen != s_lastScreen) {
        s_lastDraw = draw;
        s_lastScreen = screen;
        qa_reset_icon_caches();
        if (radialStyle) {
            quick_access_radial_reset();
        }
    }

    if (s_bottleMenuOpen) {
        s_bottleMenuAlpha += (1.0f - s_bottleMenuAlpha) * 0.35f;
        if (s_bottleMenuAlpha >= 0.99f) {
            s_bottleMenuAlpha = 1.0f;
        }
    } else {
        s_bottleMenuAlpha += (0.0f - s_bottleMenuAlpha) * 0.12f;
        if (s_bottleMenuAlpha < 0.005f) {
            s_bottleMenuAlpha = 0.0f;
        }
    }

    if (s_bottleMenuAlpha < 0.01f) {
        return;
    }

    s_bottleGlowTimer += 0.045f;
    if (s_bottleGlowTimer >= 6.2831853f) {
        s_bottleGlowTimer -= 6.2831853f;
    }

    const u8 alpha = static_cast<u8>(s_bottleMenuAlpha * 255.0f);

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }

    if (radialStyle) {
        bottles_radial_draw(screen->getWidth(), screen->getHeight(), alpha);
    } else {
        draw_bottle_bar(screen->getWidth(), screen->getHeight(), alpha);
    }
}

ModResult init_quick_access_bottles(const HookService* hook_svc, const SaveService* save_svc,
                                    ModContext* mod_ctx, ModError*) {
    if (hook_svc) {
        mods::hook::add_post<PadReadBottlesHook>(hook_svc, on_pad_read_bottles_post);
        mods::hook::add_post<Meter2DrawBottlesHook>(hook_svc, on_meter2_draw_bottles_post);
    }

    s_bottleSaveSvc = save_svc;
    s_bottleModCtx = mod_ctx;
    if (save_svc != nullptr && mod_ctx != nullptr) {
        save_svc->observe_saves(mod_ctx, on_bottles_new_save, on_bottles_save_loaded, nullptr,
                                nullptr, &s_bottleSaveObserver);
        on_bottles_save_loaded(mod_ctx, 0, nullptr);
    }
    return MOD_OK;
}

void shutdown_quick_access_bottles() {
    close_bottle_menu();
    s_bottleMenuAlpha = 0.0f;
    s_hotkeyActive = false;
    s_cancelLatchL = false;
    s_pendingSlot = 0xFF;
    s_lastDraw = nullptr;
    s_lastScreen = nullptr;
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        s_slotScale[i] = 1.0f;
    }
}
