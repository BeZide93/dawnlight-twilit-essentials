#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

#include "mods/service.hpp"
#include "mods/svc/log.h"

#include "twilit_essentials/stamina.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
// optional, so the mod still loads without TE (pointer is just null then)
IMPORT_OPTIONAL_SERVICE(TwilitEssentialsStaminaService, svc_te_stamina);

static constexpr float kDrawCostPerSecond = 12.0f;
static constexpr float kTickSeconds = 1.0f / 30.0f; // mod_update runs at 30 ticks/s

// so the error sound only plays once per draw
static bool s_denied = false;

static bool is_bow_item(u16 item) {
    return item == dItemNo_BOW_e || item == dItemNo_BOMB_ARROW_e || item == dItemNo_HAWK_ARROW_e;
}

// reload = pulling the string, charge wait = holding it drawn
static bool is_drawing_bow() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    return link != nullptr && !link->checkWolf() && is_bow_item(link->mEquipItem) &&
           (link->checkBowReloadAnime() || link->checkBowChargeWaitAnime());
}

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError*) {
    // touch the meta records or msvc strips them
    (void)&mod_meta_header_record;
    (void)&mod_meta_import_svc_log;
    (void)&mod_meta_import_svc_te_stamina;
    if (svc_log) {
        svc_log->info(mod_ctx, svc_te_stamina ? "Stamina Service Example: using Twilit Essentials stamina"
                                              : "Stamina Service Example: Twilit Essentials stamina service not found");
    }
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    if (!svc_te_stamina) return MOD_OK;
    if (!is_drawing_bow()) {
        s_denied = false;
        return MOD_OK;
    }
    // drain is fine with "not enough", it just clamps at 0.
    // CONFLICT means link is exhausted and nothing was taken
    const ModResult result = svc_te_stamina->drain(mod_ctx, kDrawCostPerSecond * kTickSeconds);
    if (result == MOD_CONFLICT && !s_denied) {
        svc_te_stamina->deny(mod_ctx);
        s_denied = true;
    }
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    s_denied = false;
    return MOD_OK;
}

}
