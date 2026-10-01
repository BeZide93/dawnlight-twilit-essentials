#pragma once

#include "mods/api.h"
#include "mods/svc/config.h"
#include "mods/svc/ui.h"

#include <string>

namespace settings_transfer {

constexpr const char* kFileName = "te_settings.json";

void install_tracking(const ConfigService** service);
void uninstall_tracking(const ConfigService** service);

std::string export_json();

void add_transfer_button(ModContext* ctx, UiElementHandle pane);

}
