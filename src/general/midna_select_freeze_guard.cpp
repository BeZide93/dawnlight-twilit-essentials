#include "midna_select_freeze_guard.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_msg_scrn_3select.h"
#include "d/d_msg_scrn_talk.h"

DEFINE_HOOK(&dMsgScrnTalk_c::selectAnimeMove, MidnaSelectAnimeMoveHook);

static void on_select_anime_move_post(ModContext*, void* args, void* ret, void*) {
    if (ret == nullptr || args == nullptr) return;

    if (*static_cast<bool*>(ret)) return;

    dMsgScrnTalk_c* talk = mods::arg<dMsgScrnTalk_c*>(args, 0);
    if (talk == nullptr) return;

    dMsgScrn3Select_c* select = talk->mpSelect_c;

    if (select == nullptr || select->mProcess >= dMsgScrn3Select_c::PROC_MAX_e) {
        *static_cast<bool*>(ret) = true;
        return;
    }

    talk->presentAnims();
}

ModResult init_midna_select_freeze_guard(const HookService* hook_svc, ModError*) {
    return MOD_OK;
    if (hook_svc == nullptr) return MOD_ERROR;
    return mods::hook::add_post<MidnaSelectAnimeMoveHook>(hook_svc, on_select_anime_move_post);
}

void shutdown_midna_select_freeze_guard() {}
