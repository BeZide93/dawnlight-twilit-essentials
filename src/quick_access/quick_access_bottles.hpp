#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

struct SaveService;

ModResult init_quick_access_bottles(const HookService* hook_svc, const SaveService* save_svc,
                                    ModContext* mod_ctx, ModError* error);
void shutdown_quick_access_bottles();
