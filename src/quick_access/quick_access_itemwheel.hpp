#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/save.h"

enum QaHideWheelMode {
    QA_HIDE_WHEEL_OFF = 0,
    QA_HIDE_WHEEL_EXCEPT_BOTTLES = 1,
    QA_HIDE_WHEEL_INCLUDING_BOTTLES = 2,
};

extern int g_configQuickAccessHideWheelMode;

ModResult init_quick_access_itemwheel(const HookService* hook_svc, const SaveService* save_svc,
                                      ModContext* ctx, ModError* error);
void update_quick_access_itemwheel();
void shutdown_quick_access_itemwheel();

void quick_access_itemwheel_refresh();
void quick_access_itemwheel_sync_ring_archive();
