// Minimal dusklight-collection-lib consumer: adds one custom sword slot.
//
// Build this together with the Dusklight SDK (see CMakeLists.txt). Place the
// referenced assets inside this mod's res/ directory:
//   res/models/clctres/MySword.arc      (BMD archive, file id used below)
//   res/textures/clctres/my_sword.bti   (slot icon)

#include <collection_lib/collection_lib.hpp>

#include "global.h"
#include "mods/service.hpp"

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_OPTIONAL_SERVICE(SaveService, svc_save);

// Defines the mod_ctx global + the mod metadata records. Exactly once per mod.
DEFINE_MOD();

static bool starter_always() { return true; }   // example: no real save gating

static void register_slots() {  // invoked by the lib on init and every screen build
    // Claim the vanilla first columns (wooden sword / ordon shield / ordon clothes).
    collectionlib_add_vanilla_sword_slot({.unlocked = &starter_always});
    collectionlib_add_vanilla_shield_slot({.unlocked = &starter_always});
    collectionlib_add_vanilla_tunic_slot({.unlocked = &starter_always,
        .name = "Ordon Clothes",
        .description = "The clothes Link wore at the beginning of his journey.",
        .icon = get_ordon_clothes_texture()});
    collectionlib_register_slot({
        CE_SWORD, 5,
        "Example Sword",
        "A sword added by the collection-lib example mod.",
        "textures/clctres/my_sword.bti",
        "models/clctres/MySword.arc", 0x0003,
        0x0004,                          // sheath model file id (0xFFFF = no sheath)
        0.0f, 0.0f, 0.0f,                // offset (x/y/z)
        0.0f, 0.0f, 0.0f,                // rotation (degrees)
        1.0f,                            // scale
        dItemNo_WEAR_KOKIRI_e,           // baseClothes (tunics only; ignored for swords)
        0xFFFFFFFFu,                     // padColor (tunics only; 0xFFFFFFFF = vanilla)
        nullptr,                         // unlocked gate: nullptr = always available
    });
}

MOD_EXPORT ModResult mod_initialize(ModError*) {
    if (svc_hook == nullptr) {
        return MOD_ERROR;
    }

    // Policy callbacks (the library holds no settings of its own - the mod
    // decides; here: everything on).
    collectionlib_set_keep_ordon_shield_policy([]() { return true; });
    collectionlib_set_unequip_policy([]() { return true; });

    collectionlib_set_register_callback(&register_slots);
    return collectionlib_init(svc_hook, svc_log, svc_save, mod_ctx);
}

MOD_EXPORT ModResult mod_update(ModError*) {
    collectionlib_update();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    collectionlib_shutdown();
    return MOD_OK;
}
