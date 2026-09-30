#include "controls.hpp"
#include "../compat/twilight_hd.hpp"

#include "m_Do/m_Do_controller_pad.h"

#include <dolphin/pad.h>

#include "mods/svc/ui.h"

struct SDL_Gamepad;

#include "mods/hook.hpp"
#include "mods/svc/hook.h"

const char* const kControlsButtonLabels[CTRL_BTN_COUNT] = {
    "Z", "L", "R", "A", "B", "X", "Y",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "L3", "R3", "L2", "R2",
    "L1 / LB", "R1 / RB",
};

static const int kControlsDefaultBinding[CTRL_BIND_COUNT] = {
    CTRL_BTN_DPAD_DOWN, CTRL_BTN_L, CTRL_BTN_A,
};

static const char* const kControlsVarNames[CTRL_BIND_COUNT] = {
    "controlsQuickAccessButton",
    "controlsBottlesButton",
    "controlsSprintButton",
};

int g_controlsBinding[CTRL_BIND_COUNT] = {
    CTRL_BTN_DPAD_DOWN, CTRL_BTN_L, CTRL_BTN_A,
};

ConfigVarHandle g_controlsVars[CTRL_BIND_COUNT] = {};

const char* const kControlsMidnaLabels[CTRL_MIDNA_COUNT] = {"D-Pad Left", "L"};
ConfigVarHandle g_controlsMidnaVar = 0;
static int s_midnaButton = CTRL_MIDNA_DPAD_LEFT;

constexpr s32 kSdlLeftShoulderButton = 9;
constexpr s32 kSdlRightShoulderButton = 10;
using GetSdlGamepadButtonFn = bool (*)(SDL_Gamepad*, int);
static GetSdlGamepadButtonFn s_getSdlGamepadButton = nullptr;

bool controls_midna_on_l() {
    return s_midnaButton == CTRL_MIDNA_L;
}

static bool sdl_button_raw_held(s32 button) {
    const s32 index = PADGetIndexForPort(PAD_1);
    SDL_Gamepad* gamepad = index < 0 ? nullptr : PADGetSDLGamepadForIndex(static_cast<u32>(index));
    if (gamepad != nullptr && s_getSdlGamepadButton != nullptr) {
        return s_getSdlGamepadButton(gamepad, button);
    }
    return PADGetNativeButtonPressed(PAD_1) == button;
}

static bool l_shoulder_raw_held() {
    return sdl_button_raw_held(kSdlLeftShoulderButton);
}

bool controls_l_shoulder_raw_held() {
    return l_shoulder_raw_held();
}

static bool ui_blocks_game_input() {
    bool visible = true;
    if (svc_ui == nullptr || svc_ui->is_any_document_visible == nullptr ||
        svc_ui->is_any_document_visible(mod_ctx, &visible) != MOD_OK) {
        return true;
    }
    return visible;
}

static bool s_lShoulderSwallowed = false;

bool controls_l_shoulder_held() {
    const bool raw = l_shoulder_raw_held();
    if (ui_blocks_game_input()) {
        s_lShoulderSwallowed = raw;
        return false;
    }
    if (!raw) {
        s_lShoulderSwallowed = false;
    }
    return raw && !s_lShoulderSwallowed;
}

u32 controls_l_shoulder_pad_mask() {
    u32 count = 0;
    PADButtonMapping* mappings = PADGetButtonMappings(PAD_1, &count);
    u32 mask = 0;
    for (u32 i = 0; mappings != nullptr && i < count; ++i) {
        if (mappings[i].nativeButton == static_cast<u32>(kSdlLeftShoulderButton)) {
            mask |= mappings[i].padButton;
        }
    }
    return mask;
}

static void on_controls_midna_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                      const ConfigVarValue*, void*) {
    if (value == nullptr) {
        return;
    }
    const int v = static_cast<int>(value->int_value);
    s_midnaButton = (v >= 0 && v < CTRL_MIDNA_COUNT) ? v : CTRL_MIDNA_DPAD_LEFT;
}

static int clamp_button_index(int idx, int binding) {
    if (idx < 0 || idx >= CTRL_BTN_COUNT) {
        return kControlsDefaultBinding[binding];
    }
    return idx;
}

bool controls_binding_blocked(int b) {
    if (b < 0 || b >= CTRL_BIND_COUNT || !twilight_hd_dpad_shortcuts()) {
        return false;
    }
    switch (clamp_button_index(g_controlsBinding[b], b)) {
    case CTRL_BTN_DPAD_UP:
    case CTRL_BTN_DPAD_DOWN:
    case CTRL_BTN_DPAD_LEFT:
    case CTRL_BTN_DPAD_RIGHT:
        return true;
    default:
        return false;
    }
}

u32 controls_binding_bit(int b) {
    if (b < 0 || b >= CTRL_BIND_COUNT || controls_binding_blocked(b)) {
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

static s32 controls_sdl_button(int button) {
    if (button == CTRL_BTN_LB) return kSdlLeftShoulderButton;
    if (button == CTRL_BTN_RB) return kSdlRightShoulderButton;
    return -1;
}

static u32 controls_ext_button_bit(int button) {
    if (button == CTRL_BTN_L3) return PAD_BUTTON_LEFT_STICK;
    if (button == CTRL_BTN_R3) return PAD_BUTTON_RIGHT_STICK;
    return 0;
}

static bool controls_trigger_held(bool left) {
    JUTGamePad* gamePad = JUTGamePad::getGamePad(PAD_1);
    if (gamePad == nullptr) {
        return false;
    }
    const int raw = left ? gamePad->getAnalogL() : gamePad->getAnalogR();
    const PADDeadZones* deadZones = PADGetDeadZones(PAD_1);
    const int zone = (deadZones != nullptr) ? (left ? deadZones->leftTriggerActivationZone
                                                    : deadZones->rightTriggerActivationZone)
                                            : 31150;
    return raw * 32767 > zone * 255;
}

bool controls_l_physical_held() {
    JUTGamePad* gamePad = JUTGamePad::getGamePad(PAD_1);
    if (gamePad != nullptr && (gamePad->getButton() & PAD_TRIGGER_L) != 0) {
        return true;
    }
    return controls_trigger_held(true) || l_shoulder_raw_held();
}

DEFINE_HOOK(&mDoCPd_c::read, ControlsPadRead);

static u32 s_extHeldPrev = 0;
static u32 s_extHeldCur  = 0;
static bool s_sdlHeldPrev[CTRL_BIND_COUNT] = {};
static bool s_sdlHeldCur[CTRL_BIND_COUNT] = {};

static bool sdl_binding_raw_held(int b) {
    const s32 sdlButton = controls_sdl_button(clamp_button_index(g_controlsBinding[b], b));
    return sdlButton >= 0 && !ui_blocks_game_input() && sdl_button_raw_held(sdlButton);
}

static void controls_pad_read_post(ModContext*, void*, void*, void*) {
    s_extHeldPrev = s_extHeldCur;
    s_extHeldCur = JUTGamePad::mPadStatus[PAD_1].extButton;
    for (int b = 0; b < CTRL_BIND_COUNT; b++) {
        s_sdlHeldPrev[b] = s_sdlHeldCur[b];
        s_sdlHeldCur[b] = sdl_binding_raw_held(b);
    }
}

bool controls_binding_held(int b) {
    if (b < 0 || b >= CTRL_BIND_COUNT || controls_binding_blocked(b)) {
        return false;
    }
    const int button = clamp_button_index(g_controlsBinding[b], b);
    switch (button) {
    case CTRL_BTN_L2: return controls_trigger_held(true);
    case CTRL_BTN_R2: return controls_trigger_held(false);
    case CTRL_BTN_LB:
    case CTRL_BTN_RB: return s_sdlHeldCur[b];
    default: break;
    }
    const u32 bit = controls_binding_bit(b);
    if (bit != 0) {
        return (mDoCPd_c::getCpadInfo(PAD_1).mButtonFlags & bit) != 0;
    }
    return (JUTGamePad::mPadStatus[PAD_1].extButton & controls_ext_button_bit(button)) != 0;
}

bool controls_binding_pressed(int b) {
    if (b < 0 || b >= CTRL_BIND_COUNT || controls_binding_blocked(b)) {
        return false;
    }
    if (controls_sdl_button(clamp_button_index(g_controlsBinding[b], b)) >= 0) {
        return s_sdlHeldCur[b] && !s_sdlHeldPrev[b];
    }
    const u32 bit = controls_binding_bit(b);
    if (bit != 0) {
        return (mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags & bit) != 0;
    }
    return (s_extHeldCur & ~s_extHeldPrev &
            controls_ext_button_bit(clamp_button_index(g_controlsBinding[b], b))) != 0;
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

ModResult init_controls_config(const ConfigService* cfg, const HookService* hook_svc,
                               ModContext* ctx) {
    if (hook_svc) {
        mods::hook::add_post<ControlsPadRead>(hook_svc, controls_pad_read_post);
        void* addr = nullptr;
        if (hook_svc->resolve != nullptr &&
            hook_svc->resolve(ctx, "SDL_GetGamepadButton", &addr, nullptr) == MOD_OK &&
            addr != nullptr) {
            s_getSdlGamepadButton = reinterpret_cast<GetSdlGamepadButtonFn>(addr);
        }
    }
    if (cfg == nullptr) {
        return MOD_OK;
    }

    {
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = "controlsMidnaButton";
        d.type = CONFIG_VAR_INT;
        d.default_int = CTRL_MIDNA_DPAD_LEFT;
        if (cfg->register_var(ctx, &d, &g_controlsMidnaVar) == MOD_OK) {
            int64_t v = CTRL_MIDNA_DPAD_LEFT;
            cfg->get_int(ctx, g_controlsMidnaVar, &v);
            s_midnaButton = (v >= 0 && v < CTRL_MIDNA_COUNT) ? static_cast<int>(v)
                                                              : CTRL_MIDNA_DPAD_LEFT;
            cfg->subscribe(ctx, g_controlsMidnaVar, on_controls_midna_changed, nullptr, nullptr);
        }
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
