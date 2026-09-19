#pragma once

#include <types.h>

#include "mods/service.hpp"
#include "mods/svc/config.h"

struct ConfigService;

/* Physical buttons that can be bound, in the order used by every SELECT control. */
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
    CTRL_BTN_COUNT,
};

extern const char* const kControlsButtonLabels[CTRL_BTN_COUNT];

/* Remappable mod inputs; each maps to one int config var (index into kControlsButtonLabels).
 * The Z-slot item button and the map portal stay fixed on Z. */
enum ControlsBinding {
    CTRL_BIND_MIDNA = 0,       /* calls Midna while the Z slot is enabled (default: D-Pad Left) */
    CTRL_BIND_QUICK_ACCESS,    /* Quick Access tap/hold button (default: D-Pad Down) */
    CTRL_BIND_BOTTLES,         /* Bottle Quick Access tap/hold button (default: L) */
    CTRL_BIND_BOSSRUSH_RETRY,  /* retries a Boss Rush fight (default: D-Pad Right) */
    CTRL_BIND_COUNT,
};

extern int g_controlsBinding[CTRL_BIND_COUNT];
extern ConfigVarHandle g_controlsVars[CTRL_BIND_COUNT];

/* PAD_* flag bit of the button currently bound to `b`. */
u16 controls_binding_bit(int b);
bool controls_binding_held(int b);
bool controls_binding_pressed(int b);

ModResult init_controls_config(const ConfigService* cfg, ModContext* ctx);
