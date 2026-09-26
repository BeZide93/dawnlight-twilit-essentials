#include "epona.hpp"

#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

DEFINE_HOOK(&daHorse_c::setStickData, EponaStickDataHook);
DEFINE_HOOK(&daHorse_c::execute, EponaExecuteHook);
DEFINE_HOOK(&daHorse_c::setLashCnt, EponaLashCntHook);

namespace {

constexpr s16 kLashPoolMax = 6;
constexpr s16 kAutoGallopHeading = 0x3000;
constexpr f32 kAutoGallopStickValue = 0.9f;
constexpr s16 kAutoGallopLashTime = 2;

}

bool g_configEponaEnabled = false;

ConfigVarHandle g_varEponaEnabled = 0;
ConfigVarHandle g_varEponaTurnRatePct = 0;
ConfigVarHandle g_varEponaTopSpeedPct = 0;
ConfigVarHandle g_varEponaUnlimitedSpurs = 0;
ConfigVarHandle g_varEponaAutoGallop = 0;

static bool s_unlimitedSpurs = true;
static bool s_autoGallop = true;
static f32 s_turnRate = 1.25f;
static f32 s_topSpeed = 1.25f;

static void on_epona_enabled_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                     const ConfigVarValue*, void*) {
    if (value) {
        g_configEponaEnabled = value->bool_value;
    }
}

static void on_epona_turn_rate_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                       const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 100) pct = 100;
        if (pct > 300) pct = 300;
        s_turnRate = static_cast<f32>(pct) / 100.0f;
    }
}

static void on_epona_top_speed_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                       const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 100) pct = 100;
        if (pct > 200) pct = 200;
        s_topSpeed = static_cast<f32>(pct) / 100.0f;
    }
}

static void on_epona_unlimited_spurs_changed(ModContext*, ConfigVarHandle,
                                             const ConfigVarValue* value, const ConfigVarValue*,
                                             void*) {
    if (value) {
        s_unlimitedSpurs = value->bool_value;
    }
}

static void on_epona_auto_gallop_changed(ModContext*, ConfigVarHandle,
                                         const ConfigVarValue* value, const ConfigVarValue*,
                                         void*) {
    if (value) {
        s_autoGallop = value->bool_value;
    }
}

static void on_stick_data_post(ModContext*, void* args, void*, void*) {
    if (!g_configEponaEnabled) {
        return;
    }
    daHorse_c* horse = mods::arg<daHorse_c*>(args, 0);
    if (horse->checkStateFlg0(daHorse_c::daHorse_FLG0(daHorse_c::FLG0_RODEO_MODE |
                                                      daHorse_c::FLG0_UNK_10000000))) {
        return;
    }

    horse->field_0x16c2 = static_cast<s16>(std::min<f32>(
        0x7FFF, static_cast<f32>(horse->field_0x16c2) * s_turnRate));
}

static HookAction on_execute_pre(ModContext*, void* args, void*, void*) {
    if (!g_configEponaEnabled) {
        return HOOK_CONTINUE;
    }
    daHorse_c* horse = mods::arg<daHorse_c*>(args, 0);
    if (s_topSpeed == 1.0f) {
        return HOOK_CONTINUE;
    }

    const daHorse_hio_c1& hio = daHorse_hio_c0::m;
    const bool kakariko = horse->checkStateFlg0(daHorse_c::FLG0_UNK_2000) != 0;
    const f32 baseMax = kakariko ? hio.kakariko_max_speed : hio.max_speed;
    const f32 baseLashAdd = kakariko ? hio.kakariko_add_lash_speed : hio.add_lash_speed;

    horse->m_normalMaxSpeedF = baseMax * s_topSpeed;
    horse->m_lashAddSpeed = baseLashAdd;
    horse->m_lashMaxSpeedF = horse->m_normalMaxSpeedF + baseLashAdd;
    horse->field_0x1764 = hio.walk_to_fastwalk_rate * horse->m_normalMaxSpeedF;
    return HOOK_CONTINUE;
}

static void on_lash_cnt_post(ModContext*, void* args, void*, void*) {
    if (!g_configEponaEnabled) {
        return;
    }
    daHorse_c* horse = mods::arg<daHorse_c*>(args, 0);
    if (!horse->checkStateFlg0(daHorse_c::FLG0_UNK_1)) {
        return;
    }

    if (s_unlimitedSpurs) {
        horse->m_lashCnt = kLashPoolMax;
    }

    if (s_autoGallop && !dComIfGp_event_runCheck() &&
        horse->m_padStickValue > kAutoGallopStickValue) {
        const s16 heading =
            static_cast<s16>(horse->m_padStickAngleY - horse->shape_angle.y);
        if (std::abs(static_cast<int>(heading)) < kAutoGallopHeading &&
            horse->m_lashAccelerationTime < kAutoGallopLashTime) {
            horse->m_lashAccelerationTime = kAutoGallopLashTime;
        }
    }
}

ModResult init_epona_config(const ConfigService* config_svc, ModContext* mod_ctx) {
    if (config_svc == nullptr) return MOD_ERROR;

    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;

    desc.name = "eponaEnabled";
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = false;
    if (config_svc->register_var(mod_ctx, &desc, &g_varEponaEnabled) == MOD_OK) {
        config_svc->get_bool(mod_ctx, g_varEponaEnabled, &g_configEponaEnabled);
        config_svc->subscribe(mod_ctx, g_varEponaEnabled, on_epona_enabled_changed, nullptr, nullptr);
    }

    desc.name = "eponaTurnRatePct";
    desc.type = CONFIG_VAR_INT;
    desc.default_bool = false;
    desc.default_int = 125;
    if (config_svc->register_var(mod_ctx, &desc, &g_varEponaTurnRatePct) == MOD_OK) {
        int64_t pct = 125;
        config_svc->get_int(mod_ctx, g_varEponaTurnRatePct, &pct);
        s_turnRate = static_cast<f32>(pct) / 100.0f;
        config_svc->subscribe(mod_ctx, g_varEponaTurnRatePct, on_epona_turn_rate_changed, nullptr, nullptr);
    }

    desc.name = "eponaTopSpeedPct";
    desc.default_int = 125;
    if (config_svc->register_var(mod_ctx, &desc, &g_varEponaTopSpeedPct) == MOD_OK) {
        int64_t pct = 125;
        config_svc->get_int(mod_ctx, g_varEponaTopSpeedPct, &pct);
        s_topSpeed = static_cast<f32>(pct) / 100.0f;
        config_svc->subscribe(mod_ctx, g_varEponaTopSpeedPct, on_epona_top_speed_changed, nullptr, nullptr);
    }

    desc.name = "eponaUnlimitedSpurs";
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = true;
    if (config_svc->register_var(mod_ctx, &desc, &g_varEponaUnlimitedSpurs) == MOD_OK) {
        bool enabled = true;
        config_svc->get_bool(mod_ctx, g_varEponaUnlimitedSpurs, &enabled);
        s_unlimitedSpurs = enabled;
        config_svc->subscribe(mod_ctx, g_varEponaUnlimitedSpurs, on_epona_unlimited_spurs_changed, nullptr, nullptr);
    }

    desc.name = "eponaAutoGallop";
    if (config_svc->register_var(mod_ctx, &desc, &g_varEponaAutoGallop) == MOD_OK) {
        bool enabled = true;
        config_svc->get_bool(mod_ctx, g_varEponaAutoGallop, &enabled);
        s_autoGallop = enabled;
        config_svc->subscribe(mod_ctx, g_varEponaAutoGallop, on_epona_auto_gallop_changed, nullptr, nullptr);
    }

    return MOD_OK;
}

ModResult init_epona(const HookService* hook_svc, ModError* error) {
    if (hook_svc == nullptr) return MOD_ERROR;

    if (mods::hook::add_pre<EponaExecuteHook>(hook_svc, on_execute_pre) != MOD_OK ||
        mods::hook::add_post<EponaStickDataHook>(hook_svc, on_stick_data_post) != MOD_OK ||
        mods::hook::add_post<EponaLashCntHook>(hook_svc, on_lash_cnt_post) != MOD_OK) {
        return mods::set_error(
            error, MOD_ERROR, "failed to hook the horse actor (daHorse_c)");
    }
    return MOD_OK;
}

void shutdown_epona() {}
