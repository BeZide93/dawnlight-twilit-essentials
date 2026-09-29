#include "tp_classic_buttons.hpp"

#include "../util.hpp"

#include "SSystem/SComponent/c_counter.h"

namespace {

constexpr const char* kTpClassicButtonsModId = "org.dusklight.tp_classic_buttons";

u32 s_frame = 0xFFFFFFFFu;
bool s_enabled = false;

}

bool tp_classic_buttons_enabled() {
    const u32 frame = g_Counter.mCounter0;
    if (s_frame != frame) {
        s_frame = frame;
        s_enabled = is_mod_enabled(kTpClassicButtonsModId);
    }
    return s_enabled;
}
