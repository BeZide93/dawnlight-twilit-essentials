#pragma once

#include <types.h>

#include "mods/service.hpp"
#include "mods/svc/config.h"

struct ConfigService;
struct HookService;

/* Physical buttons that can be bound, in the order used by every SELECT control. L2/R2 are
 * the analog trigger axes and stay readable even when PAD_TRIGGER_L/R are remapped. */
enum ControlsButton {
    CTRL_BTN_Z = 0,
    CTRL_BTN_L,
    CTRL_BTN_R,
    CTRL_BTN_A,
    CTRL_BTN_B,
    CTRL_BTN_X,
    CTRL_BTN_Y,
    CTRL_BTN_DPAD_UP,
    CTRL_BTN_DPAD_DOWN,
    CTRL_BTN_DPAD_LEFT,
    CTRL_BTN_DPAD_RIGHT,
    CTRL_BTN_L3,
    CTRL_BTN_R3,
    CTRL_BTN_L2,
    CTRL_BTN_R2,
    CTRL_BTN_COUNT,
};

extern const char* const kControlsButtonLabels[CTRL_BTN_COUNT];

/* Remappable mod inputs; each maps to one int config var (index into kControlsButtonLabels).
 * The Z-slot item button and the map portal stay fixed on Z, the Midna button stays fixed on
 * D-Pad Left, and the Boss Rush retry stays fixed on D-Pad Right. */
enum ControlsBinding {
    CTRL_BIND_QUICK_ACCESS = 0, /* Quick Access tap/hold button (default: D-Pad Down) */
    CTRL_BIND_BOTTLES,          /* Bottle Quick Access tap/hold button (default: L) */
    CTRL_BIND_SPRINT,           /* shared by human, wolf and swim sprint (default: A) */
    CTRL_BIND_COUNT,
};

extern int g_controlsBinding[CTRL_BIND_COUNT];
extern ConfigVarHandle g_controlsVars[CTRL_BIND_COUNT];

/* PAD_* flag bit of the button currently bound to `b`; 0 for buttons that never reach the
 * game's button masks (stick clicks). */
u32 controls_binding_bit(int b);
bool controls_binding_held(int b);
bool controls_binding_pressed(int b);

ModResult init_controls_config(const ConfigService* cfg, const HookService* hook_svc,
                               ModContext* ctx);
