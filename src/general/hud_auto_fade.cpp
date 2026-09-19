#include "hud_auto_fade.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_map.h"
#include "d/d_kantera_icon_meter.h"
#include "d/d_msg_object.h"
#include "d/d_s_play.h"
#include "JSystem/J2DGraph/J2DScreen.h"

#include <vector>

bool g_configHudAutoFadeEnabled = true;
float g_configHudAutoFadeIdleSeconds = 8.0f;
float g_configHudAutoFadeFadeSeconds = 1.2f;
float g_configHudAutoFadeRestAlpha = 0.0f;

static bool s_hudFadeReady = false;

namespace {

struct FadedAlpha {
    J2DPane* pane;
    u8 alpha;
};

constexpr f32 kMoveEpsilonSq = 0.25f;
constexpr f32 kTickHz = 30.0f;

std::vector<FadedAlpha> s_scaled;
dMeter2Draw_c* s_rateMeter = nullptr;
f32 s_rate1 = 0.0f;
f32 s_rate2 = 0.0f;

f32 s_fade = 1.0f;
f32 s_target = 1.0f;
int s_idleTicks = 0;
cXyz s_lastPos;
bool s_lastPosValid = false;

f32 rest_alpha() {
    f32 a = g_configHudAutoFadeRestAlpha;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return a;
}

f32 fade_eff() {
    const f32 f = s_fade;
    return f * f * (3.0f - 2.0f * f);
}

bool hud_gameplay() {
    if (dMeter2Info_getWindowStatus() != 0) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    if (dComIfGp_event_runCheck()) return false;
    if (dMeter2Info_isShopTalkFlag() || dMsgObject_isTalkNowCheck()) return false;
    return true;
}

bool collect_pane(J2DPane* pane) {
    if (pane == nullptr) return false;
    for (u32 i = 0; i < s_scaled.size(); i++) {
        if (s_scaled[i].pane == pane) return false;
    }
    FadedAlpha entry;
    entry.pane = pane;
    entry.alpha = pane->getAlpha();
    s_scaled.push_back(entry);
    return true;
}

void collect_tree(J2DPane* pane) {
    if (pane == nullptr) return;
    collect_pane(pane);
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane())
    {
        collect_tree(child);
    }
}

void hud_fade_restore() {
    for (u32 i = 0; i < s_scaled.size(); i++) {
        if (s_scaled[i].pane != nullptr) {
            s_scaled[i].pane->setAlpha(s_scaled[i].alpha);
        }
    }
    s_scaled.clear();
    if (s_rateMeter != nullptr) {
        s_rateMeter->mMeterAlphaRate[1] = s_rate1;
        s_rateMeter->mMeterAlphaRate[2] = s_rate2;
        s_rateMeter = nullptr;
    }
}

void hud_fade_apply(dMeter2_c* meter) {
    const f32 eff = fade_eff();
    if (eff >= 0.999f) {
        hud_fade_restore();
        return;
    }

    dMeter2Draw_c* draw = meter->mpMeterDraw;
    if (draw == nullptr) return;

    collect_tree(draw->mpScreen);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            collect_pane(draw->mpItemNumTex[i][j]);
        }
        if (draw->mpKanteraMeter[i] != nullptr && draw->mpKanteraMeter[i]->mpKanteraIcon != nullptr) {
            collect_tree(draw->mpKanteraMeter[i]->mpKanteraIcon->getScreen());
        }
    }

    if (draw->mMeterAlphaRate[1] != 0.0f || draw->mMeterAlphaRate[2] != 0.0f) {
        s_rateMeter = draw;
        s_rate1 = draw->mMeterAlphaRate[1];
        s_rate2 = draw->mMeterAlphaRate[2];
        draw->mMeterAlphaRate[1] = s_rate1 * eff;
        draw->mMeterAlphaRate[2] = s_rate2 * eff;
    }

    for (u32 i = 0; i < s_scaled.size(); i++) {
        if (s_scaled[i].pane != nullptr) {
            s_scaled[i].pane->setAlpha((u8)(s_scaled[i].alpha * eff));
        }
    }
}

}

DEFINE_HOOK(&dMeter2_c::_execute, HudAutoFadeMeterExecute);
DEFINE_HOOK_SYMBOL("dMeter2Draw_c::present", void(dMeter2Draw_c*), HudAutoFadeMeterPresent);
DEFINE_HOOK_SYMBOL("dMeter2_c::presentAnims", void(dMeter2_c*), HudAutoFadeMeterPresentAnims);
DEFINE_HOOK_SYMBOL("dMeter2_c::presentMap", void(dMeter2_c*), HudAutoFadeMeterPresentMap);
DEFINE_HOOK(&dMeter2_c::_delete, HudAutoFadeMeterDelete);

static HookAction on_meter_execute_pre(ModContext*, void*, void*, void*) {
    hud_fade_restore();
    return HOOK_CONTINUE;
}

static HookAction on_present_pre(ModContext*, void*, void*, void*) {
    hud_fade_restore();
    return HOOK_CONTINUE;
}

static void on_present_anims_post(ModContext*, void* args, void*, void*) {
    dMeter2_c* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr) return;
    if (!g_configHudAutoFadeEnabled || !s_hudFadeReady) {
        hud_fade_restore();
        return;
    }
    hud_fade_apply(meter);
}

static void on_present_map_post(ModContext*, void* args, void*, void*) {
    dMeter2_c* meter = mods::arg<dMeter2_c*>(args, 0);
    if (meter == nullptr || meter->mpMap == nullptr) return;
    if (!g_configHudAutoFadeEnabled || !s_hudFadeReady) return;

    const f32 eff = fade_eff();
    if (eff >= 0.999f) return;
    meter->mpMap->setMapAlpha((u8)(meter->mpMap->mMapAlpha * eff));
}

static void on_meter_delete_post(ModContext*, void*, void*, void*) {
    s_scaled.clear();
    s_rateMeter = nullptr;
    s_fade = 1.0f;
    s_target = 1.0f;
    s_idleTicks = 0;
    s_lastPosValid = false;
}

void update_hud_auto_fade() {
    if (!g_configHudAutoFadeEnabled) {
        s_fade = 1.0f;
        s_target = 1.0f;
        s_idleTicks = 0;
        s_lastPosValid = false;
        return;
    }

    bool moved = false;
    const bool gameplay = hud_gameplay();
    fopAc_ac_c* player = gameplay ? dComIfGp_getPlayer(0) : nullptr;
    if (player != nullptr) {
        const cXyz& pos = player->current.pos;
        if (s_lastPosValid) {
            const f32 dx = pos.x - s_lastPos.x;
            const f32 dy = pos.y - s_lastPos.y;
            const f32 dz = pos.z - s_lastPos.z;
            moved = (dx * dx + dy * dy + dz * dz) > kMoveEpsilonSq;
        }
        s_lastPos = pos;
        s_lastPosValid = true;
    } else {
        s_lastPosValid = false;
    }

    if (!gameplay || moved) {
        s_idleTicks = 0;
    } else {
        s_idleTicks++;
    }

    int idleLimit = (int)(g_configHudAutoFadeIdleSeconds * kTickHz);
    if (idleLimit < 0) idleLimit = 0;
    s_target = s_idleTicks >= idleLimit ? rest_alpha() : 1.0f;

    const f32 fadeSeconds = g_configHudAutoFadeFadeSeconds > 0.05f
                                ? g_configHudAutoFadeFadeSeconds
                                : 0.05f;
    const f32 step = (1.0f / kTickHz) / fadeSeconds;
    if (s_fade < s_target) {
        s_fade = s_target - s_fade < step ? s_target : s_fade + step;
    } else if (s_fade > s_target) {
        s_fade = s_fade - s_target < step ? s_target : s_fade - step;
    }
}

ModResult init_hud_auto_fade(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;

    if (mods::hook::add_pre<HudAutoFadeMeterExecute>(hook_svc, on_meter_execute_pre) != MOD_OK ||
        mods::hook::add_pre<HudAutoFadeMeterPresent>(hook_svc, on_present_pre) != MOD_OK ||
        mods::hook::add_post<HudAutoFadeMeterPresentAnims>(hook_svc, on_present_anims_post) != MOD_OK ||
        mods::hook::add_post<HudAutoFadeMeterPresentMap>(hook_svc, on_present_map_post) != MOD_OK ||
        mods::hook::add_post<HudAutoFadeMeterDelete>(hook_svc, on_meter_delete_post) != MOD_OK)
    {
        return MOD_ERROR;
    }

    s_fade = 1.0f;
    s_target = 1.0f;
    s_idleTicks = 0;
    s_lastPosValid = false;
    s_scaled.clear();
    s_rateMeter = nullptr;

    s_hudFadeReady = true;
    return MOD_OK;
}

void shutdown_hud_auto_fade() {
    s_hudFadeReady = false;
    hud_fade_restore();
}
