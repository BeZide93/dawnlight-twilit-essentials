#include "always.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_collect.h"
#include "d/d_save.h"

#include "global.h"
#include "mods/svc/hook.hpp"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/TColor.h"

DEFINE_HOOK_SYMBOL("dMenu_Collect2D_c::setHeartPiece",
                   void(dMenu_Collect2D_c*), CollectSetHeartPieceHook);

static void on_collect_set_heart_piece_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    dMenu_Collect2D_c* self = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (self == nullptr || self->mpScreen == nullptr) return;

    static const u64 heartTags[4] = {
        MULTI_CHAR('heart_1n'), MULTI_CHAR('heart_2n'),
        MULTI_CHAR('heart_3n'), MULTI_CHAR('heart_4n'),
    };

    const u16 maxLife = dComIfGs_getMaxLife();
    const bool complete = maxLife > 15 && (maxLife % 5) == 0;

    if (!complete) return;
    for (const u64 tag : heartTags) {
        J2DPane* pane = self->mpScreen->search(tag);
        if (pane == nullptr) continue;
        pane->show();
        static_cast<J2DPicture*>(pane)->setCornerColor(JUtility::TColor(255, 210, 90, 255));
    }
}

ModResult init_always(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_post<CollectSetHeartPieceHook>(hook_svc, on_collect_set_heart_piece_post);
    return MOD_OK;
}
