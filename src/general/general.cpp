#include "general.hpp"
#include "horse_cam.hpp"
#include "always.hpp"
#include "human_warp.hpp"
#include "faster_midna_cancel.hpp"
#include "hud_auto_fade.hpp"
#include "sprint_fov_kick.hpp"

#include "d/d_com_inf_game.h"

#include <cstring>

bool is_boss_rush_active();

ModResult init_general(const HookService* hook_svc, ModError* error) {
    if (!hook_svc) return MOD_ERROR;
    init_skip_cutscenes(hook_svc, error);
#if 1

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
    init_drowning_warning(hook_svc, error);
    return MOD_OK;
}

static void repair_horseback_battle_flag(const LogService* log_svc, ModContext* mod_ctx) {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, "title") == 0 ||
        std::strcmp(stage, "F_SP102") == 0) {
        return;
    }

    if (is_boss_rush_active()) {
        return;
    }

    if (!dComIfGs_isEventBit(dSv_event_flag_c::M_031) ||
        dComIfGs_isEventBit(dSv_event_flag_c::M_052)) {
        return;
    }

    dComIfGs_onEventBit(dSv_event_flag_c::M_052);
    if (log_svc != nullptr && log_svc->debug != nullptr) {
        log_svc->debug(mod_ctx,
                       "general: set M_052 (horseback battle clear), save had M_031 && !M_052");
    }
}

void update_general(const LogService* log_svc, ModContext* mod_ctx) {
    repair_horseback_battle_flag(log_svc, mod_ctx);
    update_horse_cam();
#if 0
    update_fast_forward_cutscenes(log_svc, mod_ctx);
#endif
    update_dominion_sword();
    update_human_warp(log_svc, mod_ctx);
    update_hud_auto_fade();
    update_sprint_fov_kick();
    update_drowning_warning();
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
    shutdown_drowning_warning();
}
