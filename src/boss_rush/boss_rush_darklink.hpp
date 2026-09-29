#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "mods/svc/hook.h"

#define USE_DARK_LINK 1

class fopAc_ac_c;

constexpr const char* kDarkLinkGalleryName = "Dark Link";

extern bool g_configBossRushDarkLink;

bool boss_rush_darklink_mod_installed();
bool boss_rush_darklink_enabled();
void boss_rush_darklink_draw(const cXyz& pos, const csXyz& angle);
void boss_rush_darklink_unload();

ModResult init_boss_rush_darklink(const HookService* hook_svc);
bool boss_rush_wants_vanilla_darknut();
bool boss_rush_darklink_replaces_darknut(const fopAc_ac_c* darknut);
s16 boss_rush_darklink_actor_profile();
int boss_rush_darklink_mod_state();
bool boss_rush_darklink_fight_started();
void boss_rush_darklink_begin_retry_skip();
void update_boss_rush_darklink_retry_skip();
