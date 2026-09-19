#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

extern bool g_configLockonNoLetterbox;

ModResult init_lockon_letterbox(const HookService* hook_svc, ModError* error);
void shutdown_lockon_letterbox();
