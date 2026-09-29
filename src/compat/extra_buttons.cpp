#include "extra_buttons.hpp"

#include "../util.hpp"

#include "SSystem/SComponent/c_counter.h"
#include "m_Do/m_Do_controller_pad.h"

namespace {

constexpr const char* kExtraButtonsModId = "com.olivia.extrabuttons";

u32 s_frame = 0xFFFFFFFFu;
bool s_enabled = false;

}

bool extra_buttons_enabled() {
    const u32 frame = g_Counter.mCounter0;
    if (s_frame != frame) {
        s_frame = frame;
        s_enabled = is_mod_enabled(kExtraButtonsModId);
    }
    return s_enabled;
}

bool extra_buttons_swap_combo() {
    if (!extra_buttons_enabled()) {
        return false;
    }
    const interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    return (pad.mButtonFlags & PAD_TRIGGER_R) != 0 && (pad.mPressedButtonFlags & PAD_TRIGGER_Z) != 0;
}
