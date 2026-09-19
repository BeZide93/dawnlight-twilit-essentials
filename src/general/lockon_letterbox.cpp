#include "lockon_letterbox.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"

bool g_configLockonNoLetterbox = false;

DEFINE_HOOK(&dCamera_c::CalcTrimSize, GeneralLockonLetterboxCalcTrimHook);

static void on_calc_trim_post(ModContext*, void* args, void*, void*) {
    if (!g_configLockonNoLetterbox || !args) return;

    if (dComIfGp_evmng_cameraPlay() || dComIfGp_getEvent()->runCheck()) return;

    dAttention_c* attn = dComIfGp_getAttention();
    if (attn == nullptr || !attn->Lockon()) return;

    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr) return;

    cam->SetTrimTypeForce(0);
    cam->mTrimHeight = 0.0f;
}

ModResult init_lockon_letterbox(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_post<GeneralLockonLetterboxCalcTrimHook>(hook_svc, on_calc_trim_post);
    return MOD_OK;
}

void shutdown_lockon_letterbox() {}
