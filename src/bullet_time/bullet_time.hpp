#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

extern bool g_configBulletTimeEnabled;
extern bool g_configBulletTimeFirstPerson;

void bullet_time_apply_enabled();

ModResult init_bullet_time(const HookService* hook_svc, const LogService* log_svc, ModError* error);
void update_bullet_time(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_bullet_time();
