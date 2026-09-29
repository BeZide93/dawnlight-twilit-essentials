#pragma once

#include "stamina.hpp"

#include "JSystem/JUtility/TColor.h"

struct StaminaHudFrame {
    f32 alpha;
    f32 value;
    f32 trail;
    f32 max;
    f32 pulse;
    f32 emptyFlash;
    f32 exhaustBlend;
    int exhaustPhase;
    bool preview;
};

inline JUtility::TColor stamina_hud_lerp(JUtility::TColor a, JUtility::TColor b, f32 t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return JUtility::TColor(
        static_cast<u8>(a.r + (b.r - a.r) * t),
        static_cast<u8>(a.g + (b.g - a.g) * t),
        static_cast<u8>(a.b + (b.b - a.b) * t),
        static_cast<u8>(a.a + (b.a - a.a) * t));
}

bool stamina_hud_radial_style();

void stamina_hud_notify_spend();
void stamina_hud_notify_drain();
void stamina_hud_notify_deny();
void stamina_hud_notify_exhaust();
void stamina_hud_notify_recover();
void stamina_hud_idle_tick();

void stamina_hud_begin_tick();
void stamina_hud_update(f32 stamina, f32 maxValue, bool exhausted);
void stamina_hud_refill(f32 maxValue);
void stamina_hud_reset(f32 maxValue);

void init_stamina_hud(const HookService* hook_svc, f32 maxValue);
void shutdown_stamina_hud(f32 maxValue);
