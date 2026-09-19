#include "dominion_sword.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

bool g_configGeneralDominionSword = false;

static const u16 kAnmRodd      = 0x202;
static const u16 kAnmSwordIdle = 0x255;

static const u16 kEquipSwordDrawn = 0x103;

static const int kAnmCutVertical   = 0x62;
static const int kAnmCutLeft       = 0x63;
static const int kAnmCutRight      = 0x64;
static const int kAnmCutStab       = 0x66;
static const int kAnmCopyRodSwing  = 0x178;

DEFINE_HOOK(&daAlink_c::setUpperAnime,  DomSwordSetUpperAnimeHook);
DEFINE_HOOK(&daAlink_c::setItemAction,  DomSwordSetItemActionHook);
DEFINE_HOOK(&daAlink_c::setSingleAnime, DomSwordSetSingleAnimeHook);

static bool sword_drawn_on_foot(daAlink_c* a) {
    return a != nullptr && !a->checkWolf() && a->mEquipItem == kEquipSwordDrawn;
}

static bool in_sword_action(daAlink_c* a) {
    u16 p = a->mProcID;
    return (p >= 0x21 && p <= 0x31) || a->checkEquipAnime();
}

static void enforce_rodd_idle(daAlink_c* a) {
    if (in_sword_action(a) || a->checkUpperAnime(kAnmRodd)) return;
    a->setUpperAnimeBaseSpeed(kAnmRodd, 0.0f, 3.0f);
}

static HookAction on_dom_sword_set_upper_anime_pre(ModContext*, void* args, void*, void*) {
    if (!g_configGeneralDominionSword || !args) return HOOK_CONTINUE;
    daAlink_c* a = mods::arg<daAlink_c*>(args, 0);
    if (!sword_drawn_on_foot(a)) return HOOK_CONTINUE;

    u16& resIdx = mods::arg_ref<u16>(args, 1);
    if (resIdx == kAnmSwordIdle) {
        resIdx = kAnmRodd;
    }
    return HOOK_CONTINUE;
}

static HookAction on_dom_sword_set_item_action_pre(ModContext*, void* args, void*, void*) {
    if (!g_configGeneralDominionSword || !args) return HOOK_CONTINUE;
    daAlink_c* a = mods::arg<daAlink_c*>(args, 0);
    if (!sword_drawn_on_foot(a)) return HOOK_CONTINUE;

    enforce_rodd_idle(a);

    if (a->checkUpperAnime(kAnmRodd)) {
        a->setCopyRodControllUpperSpeedRate();
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static HookAction on_dom_sword_set_single_anime_pre(ModContext*, void* args, void*, void*) {
    if (!g_configGeneralDominionSword || !args) return HOOK_CONTINUE;
    daAlink_c* a = mods::arg<daAlink_c*>(args, 0);
    if (!sword_drawn_on_foot(a)) return HOOK_CONTINUE;

    int& anmID = mods::arg_ref<int>(args, 1);
    if (anmID == kAnmCutVertical || anmID == kAnmCutLeft ||
        anmID == kAnmCutRight    || anmID == kAnmCutStab) {
        anmID = kAnmCopyRodSwing;
    }
    return HOOK_CONTINUE;
}

void update_dominion_sword() {}

ModResult init_dominion_sword(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<DomSwordSetUpperAnimeHook>(hook_svc, on_dom_sword_set_upper_anime_pre);
    mods::hook::add_pre<DomSwordSetItemActionHook>(hook_svc, on_dom_sword_set_item_action_pre);
    mods::hook::add_pre<DomSwordSetSingleAnimeHook>(hook_svc, on_dom_sword_set_single_anime_pre);
    return MOD_OK;
}

void shutdown_dominion_sword() {}
