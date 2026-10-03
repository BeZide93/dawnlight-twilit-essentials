#pragma once

#include <types.h>

#include "mods/service.hpp"
#include "mods/svc/config.h"

struct ConfigService;
struct HookService;

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
    CTRL_BTN_LB,
    CTRL_BTN_RB,
    CTRL_BTN_COUNT,
};

extern const char* const kControlsButtonLabels[CTRL_BTN_COUNT];

enum ControlsBinding {
    CTRL_BIND_QUICK_ACCESS = 0,
    CTRL_BIND_SPRINT,
    CTRL_BIND_COUNT,
};

enum ControlsQuickAccessOption {
    CTRL_QA_DPAD_DOWN = 0,
    CTRL_QA_DPAD_LEFT,
    CTRL_QA_L3,
    CTRL_QA_R3,
    CTRL_QA_L2,
    CTRL_QA_R2,
    CTRL_QA_OPTION_COUNT,
};

extern const char* const kControlsQuickAccessLabels[CTRL_QA_OPTION_COUNT];

enum ControlsSprintOption {
    CTRL_SPRINT_A = 0,
    CTRL_SPRINT_L3,
    CTRL_SPRINT_R3,
    CTRL_SPRINT_L2,
    CTRL_SPRINT_R2,
    CTRL_SPRINT_OPTION_COUNT,
};

extern const char* const kControlsSprintLabels[CTRL_SPRINT_OPTION_COUNT];

enum ControlsSprintMode {
    CTRL_SPRINT_MODE_HOLD = 0,
    CTRL_SPRINT_MODE_TOGGLE,
    CTRL_SPRINT_MODE_COUNT,
};

extern const char* const kControlsSprintModeLabels[CTRL_SPRINT_MODE_COUNT];
extern ConfigVarHandle g_controlsSprintModeVar;

extern int g_controlsBinding[CTRL_BIND_COUNT];
extern ConfigVarHandle g_controlsVars[CTRL_BIND_COUNT];

enum ControlsMidnaButton {
    CTRL_MIDNA_DPAD_LEFT = 0,
    CTRL_MIDNA_L,
    CTRL_MIDNA_COUNT,
};

extern const char* const kControlsMidnaLabels[CTRL_MIDNA_COUNT];
extern ConfigVarHandle g_controlsMidnaVar;

bool controls_midna_on_l();
bool controls_l_shoulder_held();
bool controls_l_shoulder_raw_held();
u32 controls_l_shoulder_pad_mask();
bool controls_l_physical_held();

bool controls_hd_minimap_on_right();
bool controls_hd_collection_on_left();
int controls_quick_access_option();
int controls_quick_access_default_option();
void controls_set_quick_access_option(int option);
int controls_sprint_option();
void controls_set_sprint_option(int option);
bool controls_sprint_held();
void update_controls();

int controls_binding_button(int b);
bool controls_binding_blocked(int b);
u32 controls_binding_bit(int b);
bool controls_binding_held(int b);
bool controls_binding_pressed(int b);

ModResult init_controls_config(const ConfigService* cfg, const HookService* hook_svc,
                               ModContext* ctx);
