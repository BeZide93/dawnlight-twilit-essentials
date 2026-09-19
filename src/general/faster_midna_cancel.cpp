#include "faster_midna_cancel.hpp"

#include "mods/svc/hook.hpp"

#include "d/actor/d_a_midna.h"

bool g_configFasterMidnaCancel = true;

DEFINE_HOOK(&daMidna_c::execute, MidnaExecuteHook);

static void on_midna_execute_post(ModContext*, void* args, void*, void*) {
    if (!g_configFasterMidnaCancel || !args) return;
    daMidna_c* midna = mods::arg<daMidna_c*>(args, 0);
    if (midna == nullptr) return;

    if ((midna->field_0x84e == 1 || midna->field_0x84e == 2) &&
        midna->checkStateFlg0(daMidna_c::FLG0_UNK_1000000))
    {
        midna->field_0x84e = 3;
        midna->setMatrix();

    }
}

ModResult init_faster_midna_cancel(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_post<MidnaExecuteHook>(hook_svc, on_midna_execute_post);
    return MOD_OK;
}

void shutdown_faster_midna_cancel() {}
