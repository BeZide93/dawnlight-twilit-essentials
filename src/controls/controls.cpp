#include "controls.hpp"

#include "m_Do/m_Do_controller_pad.h"

const char* const kControlsButtonLabels[CTRL_BTN_COUNT] = {
    "Z", "L", "R", "A", "B", "X", "Y",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
};

static const int kControlsDefaultBinding[CTRL_BIND_COUNT] = {
    CTRL_BTN_DPAD_LEFT, CTRL_BTN_DPAD_DOWN, CTRL_BTN_L, CTRL_BTN_DPAD_RIGHT,
};

static const char* const kControlsVarNames[CTRL_BIND_COUNT] = {
    "controlsMidnaButton",
    "controlsQuickAccessButton",
    "controlsBottlesButton",
    "controlsBossRushRetryButton",
};

int g_controlsBinding[CTRL_BIND_COUNT] = {
    CTRL_BTN_DPAD_LEFT, CTRL_BTN_DPAD_DOWN, CTRL_BTN_L, CTRL_BTN_DPAD_RIGHT,
};

ConfigVarHandle g_controlsVars[CTRL_BIND_COUNT] = {};

static int clamp_button_index(int idx, int binding) {
    if (idx < 0 || idx >= CTRL_BTN_COUNT) {
        return kControlsDefaultBinding[binding];
    }
    return idx;
}

u16 controls_binding_bit(int b) {
    if (b < 0 || b >= CTRL_BIND_COUNT) {
        return 0;
    }
    switch (clamp_button_index(g_controlsBinding[b], b)) {
    case CTRL_BTN_Z: return PAD_TRIGGER_Z;
    case CTRL_BTN_L: return PAD_TRIGGER_L;
    case CTRL_BTN_R: return PAD_TRIGGER_R;
    case CTRL_BTN_A: return PAD_BUTTON_A;
    case CTRL_BTN_B: return PAD_BUTTON_B;
    case CTRL_BTN_X: return PAD_BUTTON_X;
    case CTRL_BTN_Y: return PAD_BUTTON_Y;
    case CTRL_BTN_DPAD_UP: return PAD_BUTTON_UP;
    case CTRL_BTN_DPAD_DOWN: return PAD_BUTTON_DOWN;
    case CTRL_BTN_DPAD_LEFT: return PAD_BUTTON_LEFT;
    case CTRL_BTN_DPAD_RIGHT: return PAD_BUTTON_RIGHT;
    default: return 0;
    }
}

bool controls_binding_held(int b) {
    return (mDoCPd_c::getCpadInfo(PAD_1).mButtonFlags & controls_binding_bit(b)) != 0;
}

bool controls_binding_pressed(int b) {
    return (mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags & controls_binding_bit(b)) != 0;
}

static void on_controls_binding_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                        const ConfigVarValue*, void* user_data) {
    if (value == nullptr || user_data == nullptr) {
        return;
    }
    const intptr_t binding = reinterpret_cast<intptr_t>(user_data);
    g_controlsBinding[binding] = clamp_button_index(static_cast<int>(value->int_value),
                                                    static_cast<int>(binding));
}

ModResult init_controls_config(const ConfigService* cfg, ModContext* ctx) {
    if (cfg == nullptr) {
        return MOD_OK;
    }

    for (int i = 0; i < CTRL_BIND_COUNT; i++) {
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = kControlsVarNames[i];
        d.type = CONFIG_VAR_INT;
        d.default_int = kControlsDefaultBinding[i];
        if (cfg->register_var(ctx, &d, &g_controlsVars[i]) == MOD_OK) {
            int64_t v = kControlsDefaultBinding[i];
            cfg->get_int(ctx, g_controlsVars[i], &v);
            g_controlsBinding[i] = clamp_button_index(static_cast<int>(v), i);
            cfg->subscribe(ctx, g_controlsVars[i], on_controls_binding_changed,
                           reinterpret_cast<void*>(static_cast<intptr_t>(i)), nullptr);
        }
    }
    return MOD_OK;
}
