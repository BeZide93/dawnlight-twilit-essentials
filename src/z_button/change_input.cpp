#pragma once

#include "change_input.hpp"
#include "z_mobile.hpp"
#include "z_item_actions.hpp"
#include "../quick_access/quick_access.hpp"
#include "../controls/controls.hpp"

DEFINE_HOOK(&mDoCPd_c::read, PadReadHook);
DEFINE_HOOK(&daAlink_c::setStickData, SetStickDataHook);
DEFINE_HOOK(&daAlink_c::midnaTalkTrigger, MidnaTalkTriggerHook);

void on_pad_read_post(ModContext*, void*, void*, void*) {
    if (isTitleOrMainMenu()) {
        return;
    }

    // Iron Boots come off as soon as they are no longer assigned to any
    // button (e.g. another item was placed over them on X). Polled after the
    // game update so only settled assignment states are observed.
    check_iron_boots_unequip_on_overwrite();

    if (!g_configCustomZButtonEnabled) {
        return;
    }

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const u16 midnaBit = controls_binding_bit(CTRL_BIND_MIDNA);

    u8 windowStatus = dMeter2Info_getWindowStatus();
    bool isMenuOrPause = (windowStatus != 0) || dMeter2Info_getPauseStatus() != 0 || dComIfGp_isPauseFlag()
                         || dComIfGp_event_runCheck() || dMeter2Info_isShopTalkFlag() || dMsgObject_isTalkNowCheck();

    if (isMenuOrPause) {
        g_dpadLeftHeld = false;
        g_dpadLeftTrig = false;
        return;
    }

    const bool quickAccessOpen = quick_access_is_active();
    g_dpadLeftHeld = !quickAccessOpen && (pad.mButtonFlags & midnaBit) != 0;
    g_dpadLeftTrig = !quickAccessOpen && (pad.mPressedButtonFlags & midnaBit) != 0;

    if (!quickAccessOpen) {
        if (g_dpadLeftHeld) pad.mButtonFlags &= ~midnaBit;
        if (g_dpadLeftTrig) pad.mPressedButtonFlags &= ~midnaBit;
    }

    JUTGamePad* rawGamePad = JUTGamePad::getGamePad(0);
    u32 rawTrig = rawGamePad ? rawGamePad->getTrigger() : 0;
    u32 rawHold = rawGamePad ? rawGamePad->getButton() : 0;

    bool physZHeld = (rawHold & PAD_TRIGGER_Z) != 0;
    bool physZTrig = (rawTrig & PAD_TRIGGER_Z) != 0;

    if (quickAccessOpen) {
        physZHeld = false;
        physZTrig = false;
    }

    g_physZHeld = physZHeld;
    g_physZTrig = physZTrig;

    pad.mButtonFlags &= ~PAD_TRIGGER_Z;
    pad.mPressedButtonFlags &= ~PAD_TRIGGER_Z;

    ensure_z_slot_initialized();

    if (!quickAccessOpen && !isWolfPlayer() && g_zInventorySlot != 0xFF) {
        u8 zItem = dComIfGs_getItem(g_zInventorySlot, false);
        if (zItem != 0xFF && zItem != 0x00 && zItem != dItemNo_NONE_e && zItem != 0x72) {
            daAlink_c* alink = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
            if (alink != nullptr && !alink->checkWolf() && dComIfGs_getLife() > 0) {
                if (physZHeld || physZTrig) {
                    alink->mSelectItemId = 2;
                }
            }
        }
    }
}

void on_set_stick_data_post(ModContext*, void* args, void*, void*) {
    if (!g_configCustomZButtonEnabled || !args) {
        return;
    }

    daAlink_c* alink = mods::arg<daAlink_c*>(args, 0);

    z_mobile_hb_tick(alink);

    if (alink != nullptr && !alink->checkWolf()) {
        u8 windowStatus = dMeter2Info_getWindowStatus();
        if (windowStatus == 0 && !quick_access_is_active()) {
            JUTGamePad* rawGamePad = JUTGamePad::getGamePad(0);
            if (rawGamePad != nullptr) {
                bool physZHeld = (rawGamePad->getButton() & PAD_TRIGGER_Z) != 0;
                bool physZTrig = (rawGamePad->getTrigger() & PAD_TRIGGER_Z) != 0;

                sync_play_select_item(2);
                u8 zItem = resolved_select_item(2);
                if (zItem != 0xFF && zItem != 0x00 && zItem != dItemNo_NONE_e) {
                    if (physZHeld) {
                        alink->mItemButton |= 0x04;
                    }
                    if (physZTrig) {
                        alink->mItemTrigger |= 0x04;
                    }
                }
            }
        }
    }
}

HookAction on_midna_talk_trigger_pre(ModContext*, void* args, void* ret, void*) {
    if (!g_configCustomZButtonEnabled || !args || !ret) {
        return HOOK_CONTINUE;
    }

    const daAlink_c* link = mods::arg<const daAlink_c*>(args, 0);
    if (link == nullptr) {
        return HOOK_CONTINUE;
    }

    *reinterpret_cast<BOOL*>(ret) = g_dpadLeftTrig ? 1 : 0;
    return HOOK_SKIP_ORIGINAL;
}
