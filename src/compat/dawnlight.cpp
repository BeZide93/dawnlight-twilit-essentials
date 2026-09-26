#include "dawnlight.hpp"

#include "../util.hpp"

namespace {
constexpr const char* kDawnlightModId = "dev.bezide.dawnlight";
}

bool dawnlight_touch_ui_active() {
    return is_mod_enabled(kDawnlightModId) &&
           mod_config_bool(kDawnlightModId, "dawnlight-touch-ui", false);
}
