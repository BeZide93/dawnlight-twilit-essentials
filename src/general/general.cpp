#include "general.hpp"
#include "horse_cam.hpp"
#include "always.hpp"
#include "human_warp.hpp"
#include "faster_midna_cancel.hpp"
#include "hud_auto_fade.hpp"
#include "sprint_fov_kick.hpp"

ModResult init_general(const HookService* hook_svc, ModError* error) {
    if (!hook_svc) return MOD_ERROR;
    init_skip_cutscenes(hook_svc, error);
#if 0
    /* Disabled for Dusklight 2.0: timescale-based fast forward no longer behaves correctly
     * (game clock runs the simulation independently of the aurora timescale the mod sets).
     * Re-enable together with the UI toggle in mod.cpp. */
    init_fast_forward_cutscenes(hook_svc, error);
#endif
    init_dominion_sword(hook_svc, error);
    init_always(hook_svc, error);
    init_human_warp(hook_svc, error);
    init_faster_midna_cancel(hook_svc, error);
    init_faster_transitions(hook_svc, error);
    init_lockon_letterbox(hook_svc, error);
    init_hud_auto_fade(hook_svc, error);
    init_sprint_fov_kick(hook_svc, error);
    return MOD_OK;
}

void update_general(const LogService* log_svc, ModContext* mod_ctx) {
    update_horse_cam();
#if 0
    update_fast_forward_cutscenes(log_svc, mod_ctx);
#else
    (void)log_svc;
    (void)mod_ctx;
#endif
    update_dominion_sword();
    update_human_warp(log_svc, mod_ctx);
    update_hud_auto_fade();
    update_sprint_fov_kick();
}

void shutdown_general() {
    shutdown_skip_cutscenes();
#if 0
    shutdown_fast_forward_cutscenes();
#endif
    shutdown_dominion_sword();
    shutdown_human_warp();
    shutdown_faster_midna_cancel();
    shutdown_faster_transitions();
    shutdown_lockon_letterbox();
    shutdown_hud_auto_fade();
    shutdown_sprint_fov_kick();
}
