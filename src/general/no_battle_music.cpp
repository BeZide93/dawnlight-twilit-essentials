#include "no_battle_music.hpp"

#include "mods/svc/hook.hpp"

#include "Z2AudioLib/Z2SeqMgr.h"

bool g_configNoBattleMusic = true;

DEFINE_HOOK(&Z2SeqMgr::setBattleDistState, NoBattleMusicDistStateHook);
DEFINE_HOOK(&Z2SeqMgr::startBattleBgm, NoBattleMusicStartHook);

namespace {

constexpr u8 kNoEnemyNear = 3;

HookAction on_set_battle_dist_state_pre(ModContext*, void* args, void*, void*) {
    if (g_configNoBattleMusic && args != nullptr) {
        mods::arg_ref<u8>(args, 1) = kNoEnemyNear;
    }
    return HOOK_CONTINUE;
}

HookAction on_start_battle_bgm_pre(ModContext*, void*, void*, void*) {
    return g_configNoBattleMusic ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
}

}

ModResult init_no_battle_music(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<NoBattleMusicStartHook>(hook_svc, on_start_battle_bgm_pre);
    return mods::hook::add_pre<NoBattleMusicDistStateHook>(hook_svc, on_set_battle_dist_state_pre);
}

void shutdown_no_battle_music() {}
