#pragma once

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/hook.hpp"

extern bool g_configStaminaEnabled;
extern int  g_configStaminaMax;
extern int  g_configStaminaRegen;

extern bool g_configStaminaSrcAttacks;
extern bool g_configStaminaSrcJumpSpin;
extern bool g_configStaminaSrcRolls;
extern bool g_configStaminaSrcClimb;
extern bool g_configStaminaSrcHang;
extern bool g_configStaminaSrcSwim;
extern bool g_configStaminaSrcPushPull;
extern bool g_configStaminaSrcWolfDash;
extern bool g_configStaminaSrcHiddenSkills;

extern int g_configStaminaCostAttack;
extern int g_configStaminaCostJumpAttack;
extern int g_configStaminaCostSpin;
extern int g_configStaminaCostRoll;
extern int g_configStaminaCostSidestep;
extern int g_configStaminaCostClimb;
extern int g_configStaminaCostHang;
extern int g_configStaminaCostCrawl;
extern int g_configStaminaCostSwim;
extern int g_configStaminaCostPushPull;
extern int g_configStaminaCostWolfDash;
extern int g_configStaminaCostSprint;
extern int g_configStaminaCostWolfSprint;
extern int g_configStaminaCostSwimSprint;
extern int g_configStaminaCostHiddenSkills;

ModResult init_stamina(const HookService* hook_svc, ModError* error);
void update_stamina(const LogService* log_svc, ModContext* mod_ctx);
void shutdown_stamina();
