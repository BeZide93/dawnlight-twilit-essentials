#include "twilight_hd.hpp"

#include "../util.hpp"

#include "mods/svc/hook.hpp"

#include "SSystem/SComponent/c_counter.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kTwilightHdModId = "org.twilight.hd_hud";

struct TwilightHdState {
    u32 frame = 0xFFFFFFFFu;
    bool valid = false;
    bool enabled = false;
    bool thirdItemSlot = false;
    bool collection = false;
    bool dpadShortcuts = false;
    f32 overallScale = 1.0f;
};

TwilightHdState s_state;

const TwilightHdState& twilight_hd_state() {
    const u32 frame = g_Counter.mCounter0;
    if (s_state.valid && s_state.frame == frame) {
        return s_state;
    }
    s_state.valid = true;
    s_state.frame = frame;
    s_state.enabled = is_mod_enabled(kTwilightHdModId);
    if (!s_state.enabled) {
        s_state.thirdItemSlot = false;
        s_state.collection = false;
        s_state.dpadShortcuts = false;
        s_state.overallScale = 1.0f;
        return s_state;
    }
    s_state.thirdItemSlot = mod_config_bool(kTwilightHdModId, "third-item-slot", true);
    s_state.collection = mod_config_bool(kTwilightHdModId, "collection-screen", true);
    s_state.dpadShortcuts = mod_config_bool(kTwilightHdModId, "dpad-shortcuts", true);
    s64 percent = mod_config_int(kTwilightHdModId, "hud-percent", 100);
    if (percent < 50) percent = 50;
    if (percent > 125) percent = 125;
    s_state.overallScale = static_cast<f32>(percent) / 100.0f;
    return s_state;
}

}

bool twilight_hd_enabled() {
    return twilight_hd_state().enabled;
}

bool twilight_hd_third_item_slot() {
    return twilight_hd_state().thirdItemSlot;
}

bool twilight_hd_collection() {
    return twilight_hd_state().collection;
}

bool twilight_hd_dpad_shortcuts() {
    return twilight_hd_state().dpadShortcuts;
}

f32 twilight_hd_overall_scale() {
    return twilight_hd_state().overallScale;
}

f32 twilight_hd_top_meter_center_y() {
    return mDoGph_gInf_c::getSafeMinYF() + 54.0f;
}

bool twilight_hd_gauge_visible(dMeter2Draw_c* draw) {
    if (draw == nullptr) {
        return false;
    }
    return draw->getMeterGaugeAlphaRate(1) > 0.02f || draw->getMeterGaugeAlphaRate(2) > 0.02f;
}

bool twilight_hd_meter_frame_bounds(J2DScreen* screen, f32& left, f32& top, f32& right,
                                    f32& bottom) {
    if (screen == nullptr) {
        return false;
    }
    constexpr u64 kFrameTags[] = {
        MULTI_CHAR('mw_ll'), MULTI_CHAR('mw_lu'), MULTI_CHAR('mw_rl'),
        MULTI_CHAR('mw_ru'), MULTI_CHAR('mm_base'),
    };
    bool hasBounds = false;
    CPaneMgr manager;
    for (u64 tag : kFrameTags) {
        J2DPane* pane = screen->search(tag);
        if (pane == nullptr) {
            return false;
        }
        Mtx mtx;
        for (u8 i = 0; i < 4; ++i) {
            const Vec vtx = manager.getGlobalVtx(pane, &mtx, i, false, 0);
            if (!std::isfinite(vtx.x) || !std::isfinite(vtx.y)) {
                return false;
            }
            if (!hasBounds) {
                left = right = vtx.x;
                top = bottom = vtx.y;
                hasBounds = true;
                continue;
            }
            left = std::min(left, vtx.x);
            right = std::max(right, vtx.x);
            top = std::min(top, vtx.y);
            bottom = std::max(bottom, vtx.y);
        }
    }
    return hasBounds;
}

DEFINE_HOOK(&mDoCPd_c::read, TwilightHdRDedupePadReadHook);
DEFINE_HOOK(&dMeter2Draw_c::draw, TwilightHdRupeeMeterDrawHook);

namespace {

bool s_prevRHeld = false;
bool s_prevRLockHeld = false;

void on_pad_read_r_dedupe_post(ModContext*, void*, void*, void*) {
    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    if (twilight_hd_third_item_slot()) {
        if (s_prevRHeld) {
            pad.mPressedButtonFlags &= ~PAD_TRIGGER_R;
        }
        if (s_prevRLockHeld) {
            pad.mTrigLockR = false;
        }
    }
    s_prevRHeld = (pad.mButtonFlags & PAD_TRIGGER_R) != 0;
    s_prevRLockHeld = pad.mHoldLockR;
}

bool s_rupeeSwapped = false;
u16 s_rupeeSaved = 0;
u16 s_rupeeShown = 0;

void restore_swapped_rupee() {
    if (!s_rupeeSwapped) {
        return;
    }
    s_rupeeSwapped = false;
    if (dComIfGs_getRupee() == s_rupeeShown) {
        dComIfGs_setRupee(s_rupeeSaved);
    }
}

HookAction on_meter_draw_rupee_pre(ModContext*, void*, void*, void*) {
    restore_swapped_rupee();
    if (!twilight_hd_enabled()) {
        return HOOK_CONTINUE;
    }
    dMeter2_c* meter = g_meter2_info.getMeterClass();
    if (meter == nullptr || meter->mRupeeNum < 0) {
        return HOOK_CONTINUE;
    }
    const u16 saved = dComIfGs_getRupee();
    const u16 shown = static_cast<u16>(meter->mRupeeNum);
    if (shown == saved) {
        return HOOK_CONTINUE;
    }
    s_rupeeSaved = saved;
    s_rupeeShown = shown;
    s_rupeeSwapped = true;
    dComIfGs_setRupee(shown);
    return HOOK_CONTINUE;
}

void on_meter_draw_rupee_post(ModContext*, void*, void*, void*) {
    restore_swapped_rupee();
}

}

ModResult init_twilight_hd_compat(const HookService* hook_svc) {
    if (hook_svc == nullptr) {
        return MOD_ERROR;
    }
    const HookOptions beforeTwilightHd = twilight_hd_hook_order(kTwilightHdRunBefore);
    const HookOptions afterTwilightHd = twilight_hd_hook_order(kTwilightHdRunAfter);
    if (mods::hook::add_post<TwilightHdRDedupePadReadHook>(hook_svc, on_pad_read_r_dedupe_post,
                                                           &afterTwilightHd) != MOD_OK ||
        mods::hook::add_pre<TwilightHdRupeeMeterDrawHook>(hook_svc, on_meter_draw_rupee_pre,
                                                          &beforeTwilightHd) != MOD_OK ||
        mods::hook::add_post<TwilightHdRupeeMeterDrawHook>(hook_svc, on_meter_draw_rupee_post,
                                                           &afterTwilightHd) != MOD_OK)
    {
        return MOD_ERROR;
    }
    return MOD_OK;
}

void shutdown_twilight_hd_compat() {
    restore_swapped_rupee();
}
