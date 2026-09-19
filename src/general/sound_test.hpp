#pragma once

#include "global.h"
#include "mods/svc/ui.h"

ModResult tab_sound_test(ModContext* ctx, UiWindowHandle window, UiElementHandle left,
                         UiElementHandle right, void* user_data, ModError* err);

void sound_test_update();
