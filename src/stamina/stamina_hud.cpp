#include "stamina_hud.hpp"
#include "stamina_internal.hpp"
#include "stamina_kantera.hpp"
#include "stamina_radial.hpp"
#include "../boss_bar/boss_bar.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "mods/svc/config.h"

#define private public
#define protected public
#include "d/d_meter2_draw.h"
#undef private
#undef protected

#include <cstdint>

float g_configStaminaBarX = 0.0f;
float g_configStaminaBarY = 0.0f;

ConfigVarHandle g_staminaBarVars[2] = {};
ConfigVarHandle g_staminaBarStyleVar = 0;
int g_configStaminaBarStyle = 1;

enum StaminaBarStyle {
    kStaminaBarStyleDefault = 0,
    kStaminaBarStyleWheel,
};

const char* const kStaminaBarStyleLabels[] = {"Default", "BotW Wheel"};
const size_t kStaminaBarStyleCount = sizeof(kStaminaBarStyleLabels) / sizeof(kStaminaBarStyleLabels[0]);

DEFINE_HOOK(&dMeter2Draw_c::draw, StaminaMeterDrawHook);
DEFINE_HOOK(&dMeter2Draw_c::drawKanteraScreen, StaminaKanteraScreenHook);

static f32 s_display = 100.0f;
static int s_showTimer = 0;
static f32 s_alpha = 0.0f;
static f32 s_pulse = 0.0f;
static f32 s_emptyFlash = 0.0f;
static f32 s_exhaustBlend = 0.0f;
static int s_exhaustPhase = 0;
static f32 s_trail = 100.0f;
static int s_trailHold = 0;

static constexpr int kTrailHoldFrames = 18;
static constexpr f32 kTrailCatchUp = 0.14f;
static constexpr f32 kTrailCaughtUpEpsilon = 0.5f;

static constexpr int kStaminaBarPreviewFrames = 120;
static int s_staminaBarPreviewFrames = 0;

bool stamina_hud_radial_style() { return g_configStaminaBarStyle == kStaminaBarStyleWheel; }

void stamina_bar_preview_request() { s_staminaBarPreviewFrames = kStaminaBarPreviewFrames; }
void stamina_bar_preview_cancel() { s_staminaBarPreviewFrames = 0; }

static void on_stamina_bar_style_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                         const ConfigVarValue*, void*) {
    if (value == nullptr) return;
    g_configStaminaBarStyle = static_cast<int>(value->int_value);
    stamina_bar_preview_request();
    boss_bar_preview_cancel();
}

static void on_stamina_bar_pos_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                       const ConfigVarValue*, void* user_data) {
    if (value == nullptr) return;
    const f32 v = static_cast<f32>(value->int_value);
    if (user_data != nullptr) {
        g_configStaminaBarX = v;
    } else {
        g_configStaminaBarY = v;
    }
    stamina_bar_preview_request();
    boss_bar_preview_cancel();
}

ModResult init_stamina_bar_config(const ConfigService* cfg, ModContext* ctx) {
    if (cfg == nullptr) return MOD_OK;

    const struct { const char* name; bool isX; } vars[] = {
        { "staminaBarX", true },
        { "staminaBarY", false },
    };
    for (int i = 0; i < 2; i++) {
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = vars[i].name;
        d.type = CONFIG_VAR_INT;
        d.default_int = 0;
        if (cfg->register_var(ctx, &d, &g_staminaBarVars[i]) == MOD_OK) {
            int64_t val = 0;
            cfg->get_int(ctx, g_staminaBarVars[i], &val);
            if (vars[i].isX) {
                g_configStaminaBarX = static_cast<f32>(val);
            } else {
                g_configStaminaBarY = static_cast<f32>(val);
            }
            cfg->subscribe(ctx, g_staminaBarVars[i], on_stamina_bar_pos_changed,
                           reinterpret_cast<void*>(static_cast<intptr_t>(vars[i].isX)), nullptr);
        }
    }

    ConfigVarDesc styleDesc = CONFIG_VAR_DESC_INIT;
    styleDesc.name = "staminaBarStyle";
    styleDesc.type = CONFIG_VAR_INT;
    styleDesc.default_int = kStaminaBarStyleWheel;
    if (cfg->register_var(ctx, &styleDesc, &g_staminaBarStyleVar) == MOD_OK) {
        int64_t style = kStaminaBarStyleWheel;
        cfg->get_int(ctx, g_staminaBarStyleVar, &style);
        g_configStaminaBarStyle = static_cast<int>(style);
        cfg->subscribe(ctx, g_staminaBarStyleVar, on_stamina_bar_style_changed, nullptr, nullptr);
    }
    return MOD_OK;
}

static bool in_gameplay_for_draw() {
    if (dMeter2Info_getWindowStatus() != 0) return false;
    if (dComIfGp_isPauseFlag()) return false;
    if (dComIfGp_event_runCheck()) return false;
    if (dMeter2Info_isShopTalkFlag() || dMsgObject_isTalkNowCheck()) return false;
    return true;
}

f32 stamina_bar_alpha() {
    if (s_staminaBarPreviewFrames > 0) return 1.0f;
    if (!g_configStaminaEnabled || !in_gameplay_for_draw()) return 0.0f;
    return s_alpha;
}

void stamina_hud_notify_spend() {
    s_showTimer = 50;
    s_pulse = 1.0f;
    if (s_trail - s_display < kTrailCaughtUpEpsilon) s_trailHold = kTrailHoldFrames;
}

void stamina_hud_notify_drain() {
    s_showTimer = 45;
    s_pulse = 1.0f;
}

void stamina_hud_notify_deny() {
    s_showTimer = 50;
    s_emptyFlash = 1.0f;
}

void stamina_hud_notify_exhaust() { s_showTimer = 60; }

void stamina_hud_notify_recover() { s_pulse = 1.0f; }

void stamina_hud_idle_tick() {
    if (s_showTimer > 0) s_showTimer--;
}

void stamina_hud_begin_tick() {
    if (s_staminaBarPreviewFrames > 0) s_staminaBarPreviewFrames--;
}

void stamina_hud_refill(f32 maxValue) {
    s_display = s_trail = maxValue;
    s_trailHold = 0;
}

void stamina_hud_reset(f32 maxValue) {
    stamina_hud_refill(maxValue);
    s_showTimer = 0;
    s_alpha = s_pulse = s_emptyFlash = 0.0f;
    s_exhaustBlend = 0.0f;
}

void stamina_hud_update(f32 stamina, f32 maxValue, bool exhausted) {
    s_exhaustPhase++;
    const f32 exhaustTarget = exhausted ? 1.0f : 0.0f;
    s_exhaustBlend += (exhaustTarget - s_exhaustBlend) * 0.15f;
    if (s_exhaustBlend < 0.003f) s_exhaustBlend = 0.0f;

    s_display += (stamina - s_display) * 0.28f;
    if (s_display < 0.0f) s_display = 0.0f;
    if (s_display > maxValue) s_display = maxValue;
    if (s_trailHold > 0) {
        s_trailHold--;
    } else {
        s_trail += (s_display - s_trail) * kTrailCatchUp;
    }
    if (s_trail < s_display) s_trail = s_display;
    if (s_trail > maxValue) s_trail = maxValue;
    s_pulse *= 0.82f;       if (s_pulse < 0.003f)      s_pulse = 0.0f;
    s_emptyFlash *= 0.90f;  if (s_emptyFlash < 0.003f) s_emptyFlash = 0.0f;

    const bool visible = (s_showTimer > 0) || (stamina < maxValue - 0.5f);
    const f32 target = visible ? 1.0f : 0.0f;
    s_alpha += (target - s_alpha) * (target > s_alpha ? 0.22f : 0.12f);
    if (s_alpha < 0.001f) s_alpha = 0.0f;
    if (s_alpha > 1.0f) s_alpha = 1.0f;
}

static void draw_style(dMeter2Draw_c* draw, f32 alpha, f32 value, f32 trail, f32 maxValue, bool preview) {
    StaminaHudFrame frame;
    frame.alpha = alpha;
    frame.value = value;
    frame.trail = trail;
    frame.max = maxValue;
    frame.pulse = s_pulse;
    frame.emptyFlash = s_emptyFlash;
    frame.exhaustBlend = s_exhaustBlend;
    frame.exhaustPhase = s_exhaustPhase;
    frame.preview = preview;
    if (stamina_hud_radial_style()) {
        stamina_radial_draw(frame);
    } else {
        stamina_kantera_draw(draw, frame);
    }
}

static void on_stamina_meter_draw_post(ModContext*, void* args, void*, void*) {
    dMeter2Draw_c* draw = args ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;
    if (!draw || !draw->mpKanteraScreen) return;

    stamina_kantera_begin_draw(draw);

    const f32 maxValue = stamina_impl::max_value();
    if (s_staminaBarPreviewFrames > 0) {
        const f32 value = g_configStaminaEnabled ? s_display : maxValue;
        const f32 trail = g_configStaminaEnabled ? s_trail : maxValue;
        draw_style(draw, 1.0f, value, trail, maxValue, true);
        return;
    }

    if (!g_configStaminaEnabled || s_alpha < 0.01f) return;
    if (!in_gameplay_for_draw()) return;

    f32 a = s_alpha;
    if (a > 1.0f) a = 1.0f;
    draw_style(draw, a, s_display, s_trail, maxValue, false);
}

static void on_stamina_kantera_screen_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    stamina_kantera_screen_post(mods::arg<dMeter2Draw_c*>(args, 0), mods::arg<u8>(args, 1));
}

void init_stamina_hud(const HookService* hook_svc, f32 maxValue) {
    stamina_hud_refill(maxValue);
    stamina_radial_init(hook_svc);
    mods::hook::add_post<StaminaMeterDrawHook>(hook_svc, on_stamina_meter_draw_post);
    mods::hook::add_post<StaminaKanteraScreenHook>(hook_svc, on_stamina_kantera_screen_post);
}

void shutdown_stamina_hud(f32 maxValue) {
    stamina_hud_reset(maxValue);
    stamina_kantera_shutdown();
    stamina_radial_shutdown();
}
