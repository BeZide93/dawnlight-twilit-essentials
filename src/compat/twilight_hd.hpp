#pragma once

#include <types.h>
#include "mods/svc/hook.h"

class J2DScreen;
class dMeter2Draw_c;

bool twilight_hd_enabled();
bool twilight_hd_third_item_slot();
bool twilight_hd_collection();
bool twilight_hd_dpad_shortcuts();
f32 twilight_hd_overall_scale();

f32 twilight_hd_top_meter_center_y();
bool twilight_hd_gauge_visible(dMeter2Draw_c* draw);
bool twilight_hd_meter_frame_bounds(J2DScreen* screen, f32& left, f32& top, f32& right,
                                    f32& bottom);

constexpr int32_t kTwilightHdRunBefore = 100;
constexpr int32_t kTwilightHdRunAfter = -100;

inline HookOptions twilight_hd_hook_order(int32_t priority) {
    HookOptions options = HOOK_OPTIONS_INIT;
    options.priority = priority;
    return options;
}
