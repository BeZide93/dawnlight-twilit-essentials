#pragma once

#include "mods/svc/hook.h"

void init_stamina_swordcharge(const HookService* hook_svc);
void shutdown_stamina_swordcharge();
float stamina_swordcharge_drain_mul();
