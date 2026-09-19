#include "boss_rush_dpad.hpp"

#include "boss_rush.hpp"
#include "../quick_access/quick_access.hpp"

#include "mods/hook.hpp"
#include "mods/service.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>
#include <cstring>

DEFINE_HOOK(&mDoCPd_c::read, BossRushDpadReadHook);

static bool fight_label_live(bool countFrame = false);

static void dpad_read_post(ModContext*, void*, void*, void*) {
    const bool inFight = boss_rush_is_fight_engaged();
    const bool inWarp = is_boss_rush_active() &&
                        (is_boss_rush_transition_in_flight() || boss_rush_settle_window_active());
    if (!inFight && !inWarp) {
        return;
    }

    if (quick_access_is_active()) {
        return;
    }

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const bool pressed = (pad.mPressedButtonFlags & PAD_BUTTON_RIGHT) != 0;
    pad.mPressedButtonFlags &= ~PAD_BUTTON_RIGHT;
    pad.mButtonFlags &= ~PAD_BUTTON_RIGHT;

    if (pressed && fight_label_live()) {
        boss_rush_request_retry();
    }
}

static const u16 kTextboxTypeId = 19;

static char s_origMapLabel[64] = {};
static bool s_origCaptured   = false;
static bool s_labelIsCustom  = false;

static bool s_fightLive = false;

static bool s_labelPrevEngaged = false;
static f32  s_lastCrossAlpha = -1.0f;
static bool s_forcedCrossRow = false;

static bool fight_label_live(bool) {
    const bool engaged = boss_rush_is_fight_engaged();
    if (!engaged) {
        if (s_labelPrevEngaged) {
            s_labelPrevEngaged = false;
            s_fightLive = false;
        }
        return false;
    }
    s_labelPrevEngaged = true;
    if (!is_boss_rush_transition_in_flight() && !dComIfGp_event_runCheck()) {
        s_fightLive = true;
    }
    return s_fightLive;
}

static void set_subtree_strings(J2DPane* pane, const char* str) {
    if (pane == nullptr) return;
    if (pane->getTypeID() == kTextboxTypeId) {
        J2DTextBox* tb = static_cast<J2DTextBox*>(pane);
        const char* cur = tb->getStringPtr();
        if (cur == nullptr || std::strcmp(cur, str) != 0) {
            tb->setString(str);
        }
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        set_subtree_strings(child, str);
    }
}

static const char* first_subtree_string(J2DPane* pane) {
    if (pane == nullptr) return nullptr;
    if (pane->getTypeID() == kTextboxTypeId) {
        const char* s = static_cast<J2DTextBox*>(pane)->getStringPtr();
        if (s != nullptr && s[0] != '\0') return s;
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        if (const char* s = first_subtree_string(child)) return s;
    }
    return nullptr;
}

DEFINE_HOOK(&dMeter2Draw_c::draw, BossRushDpadMeterHook);

DEFINE_HOOK(&dMeter2Info_isMapOpenCheck, BossRushMapOpenCheckHook);

static HookAction boss_rush_map_open_check_pre(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) return HOOK_CONTINUE;
    if (!boss_rush_is_fight_engaged()) return HOOK_CONTINUE;
    if (boss_rush_is_hud_menu_blocking()) return HOOK_CONTINUE;
    if (!fight_label_live()) return HOOK_CONTINUE;
    *static_cast<bool*>(retval) = true;
    return HOOK_SKIP_ORIGINAL;
}

static void force_map_slot_visible(dMeter2Draw_c* draw) {
    if (draw->mpTextM != nullptr) {
        draw->mpTextM->setAlphaRate(1.0f);
    }
    for (int i = 0; i < 5; ++i) {
        if (draw->mpJujiM[i] != nullptr) {
            draw->mpJujiM[i]->setAlphaRate(1.0f);
        }
    }
}

static HookAction dpad_meter_pre(ModContext*, void* args, void*, void*) {
    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    s_lastCrossAlpha = (draw != nullptr && draw->mpButtonCrossParent != nullptr)
                           ? draw->mpButtonCrossParent->getAlphaRate()
                           : -1.0f;
    const bool crossShown = s_lastCrossAlpha > 0.0f;
    if (draw == nullptr || !fight_label_live()) {
        s_forcedCrossRow = false;
        return HOOK_CONTINUE;
    }

    s_forcedCrossRow = false;
    if (!crossShown && !boss_rush_is_hud_menu_blocking() && !quick_access_is_active()) {
        if (draw->mpButtonCrossParent != nullptr) {
            draw->mpButtonCrossParent->setAlphaRate(1.0f);
            s_forcedCrossRow = true;
        }
    }
    if (crossShown || s_forcedCrossRow) {
        force_map_slot_visible(draw);
    }
    return HOOK_CONTINUE;
}

static void dpad_meter_post(ModContext*, void* args, void*, void*) {
    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (!draw || !draw->getMainScreenPtr()) return;

    J2DPane* group = draw->getMainScreenPtr()->search(MULTI_CHAR('m_text_n'));
    if (group == nullptr) return;

    const char* want = nullptr;
    if (fight_label_live(true)) {
        want = "RETRY";
    }

    if (want == nullptr) {
        if (s_labelIsCustom && s_origCaptured) {
            set_subtree_strings(group, s_origMapLabel);
        }
        s_labelIsCustom = false;
        return;
    }

    if (!s_origCaptured) {
        const char* cur = first_subtree_string(group);
        if (cur != nullptr && std::strcmp(cur, "RETRY") != 0 &&
            std::strcmp(cur, "COMPENDIUM") != 0) {
            std::strncpy(s_origMapLabel, cur, sizeof(s_origMapLabel) - 1);
            s_origMapLabel[sizeof(s_origMapLabel) - 1] = '\0';
            s_origCaptured = true;
        }
    }

    set_subtree_strings(group, want);
    s_labelIsCustom = true;
}

ModResult init_boss_rush_dpad(const HookService* hook_svc, const LogService*, ModContext*) {
    if (!hook_svc) return MOD_OK;
    mods::hook::add_post<BossRushDpadReadHook>(hook_svc, dpad_read_post);
    mods::hook::add_pre<BossRushDpadMeterHook>(hook_svc, dpad_meter_pre);
    mods::hook::add_post<BossRushDpadMeterHook>(hook_svc, dpad_meter_post);
    mods::hook::add_pre<BossRushMapOpenCheckHook>(hook_svc, boss_rush_map_open_check_pre);
    return MOD_OK;
}

void shutdown_boss_rush_dpad() {
    if (s_labelIsCustom && s_origCaptured) {
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* draw = (meter != nullptr) ? meter->getMeterDrawPtr() : nullptr;
        J2DScreen* scrn = (draw != nullptr) ? draw->getMainScreenPtr() : nullptr;
        J2DPane* group = (scrn != nullptr) ? scrn->search(MULTI_CHAR('m_text_n')) : nullptr;
        if (group != nullptr) {
            set_subtree_strings(group, s_origMapLabel);
        }
    }
    s_origCaptured = false;
    s_labelIsCustom = false;
}
