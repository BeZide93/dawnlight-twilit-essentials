#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"

ModResult init_always(const HookService* hook_svc, ModError* error);
