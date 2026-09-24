#include "collection_menu.hpp"
#include "../util.hpp"

#include <collection_lib/collection_lib.hpp>

bool g_configCollectionStarterEquip = false;
bool g_configCollectionKeepOrdonShield = false;
bool g_configCollectionShowOrdonHero = false;
bool g_configCollectionOrdonHeroAlways = false;

static cl::Page* s_ordonHeroPage = nullptr;

static bool player_has_hero_clothes() {
    return dComIfGs_isCollectClothes(KOKIRI_CLOTHES_FLAG) ||
           dComIfGs_isItemFirstBit(dItemNo_WEAR_KOKIRI_e);
}

static bool player_has_ordon_shield() {
    return dComIfGs_isCollectShield(COLLECT_WOODEN_SHIELD) ||
           dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e);
}

void sync_collection_ordon_hero_page() {
    if (g_configCollectionShowOrdonHero && s_ordonHeroPage == nullptr) {
        cl::Page* p2 = new cl::Page();
        p2->add(cl::heart());
        p2->add(cl::crystal());
        p2->add(cl::fused_shadow());
        s_ordonHeroPage = p2;
    } else if (!g_configCollectionShowOrdonHero && s_ordonHeroPage != nullptr) {
        delete s_ordonHeroPage;
        s_ordonHeroPage = nullptr;
    }
}

static inline int add_vanilla_sword_slot(const CollectionVanillaSlotDef& def) {
    return collectionlib_add_vanilla_slot(1, def);
}
static inline int add_vanilla_shield_slot(const CollectionVanillaSlotDef& def) {
    return collectionlib_add_vanilla_slot(2, def);
}
static inline int add_vanilla_tunic_slot(const CollectionVanillaSlotDef& def) {
    return collectionlib_add_vanilla_slot(3, def);
}

void register_custom_swords() {

}

void register_custom_shields() {
    if (!g_configCollectionShowOrdonHero) {
        return;
    }
    if (!g_configCollectionOrdonHeroAlways && !player_has_ordon_shield()) {
        return;
    }
    collectionlib_add_next_shield_slot({
        CE_SHIELD, 0,
        "Reinforced Shield",
        "A traditional Ordon shield reinforced with metal. Stronger and will never burn.",
        "textures/clctres/reinforced_shield.bti",
        nullptr,
        "models/clctres/ReinforcedShield.arc", 0x0003,
    });

}

void register_custom_tunics() {
    if (!g_configCollectionShowOrdonHero) {
        return;
    }
    if (!g_configCollectionOrdonHeroAlways && !player_has_hero_clothes()) {
        return;
    }
    collectionlib_add_next_tunic_slot({
        CE_TUNIC, 0,
        "Ordon Hero",
        "Traditional clothes worn by the hero of Ordon. Simple, but made for adventure.",
        "textures/clctres/ordonhero.bti",
        nullptr,
        "models/clctres/OrdonHero.arc", 0x000C,
        0xFFFF,
        0, 0, 0, 0, 0, 0, 1.0f,
        dItemNo_WEAR_KOKIRI_e,
        0xC8A05Au,
    });
}

ModResult init_collection_menu(const HookService* hook_svc, const LogService* log_svc,
                               const SaveService* save_svc, ModContext* mod_ctx,
                               ModError* error) {
    sync_collection_ordon_hero_page();

    collectionlib_set_keep_ordon_shield_policy([]() { return g_configCollectionKeepOrdonShield; });

    collectionlib_set_register_callback([]() {
        if (g_configCollectionStarterEquip) {
            add_vanilla_sword_slot({
                .unlocked = []() {
                    if (!g_configCollectionStarterEquip) return false;
                    const u8 eq = custom_equip_active(CE_SWORD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipSword();
                    return dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e) || (eq == dItemNo_WOOD_STICK_e) ||
                           dComIfGs_isItemFirstBit(dItemNo_SWORD_e) || (eq == dItemNo_SWORD_e) ||
                           dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) || (eq == dItemNo_MASTER_SWORD_e) ||
                           dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) || (eq == dItemNo_LIGHT_SWORD_e);
                },
                .equipped = []() { return dComIfGs_getSelectEquipSword() == dItemNo_WOOD_STICK_e; },
            });
        }

        if (g_configCollectionStarterEquip) {
            add_vanilla_shield_slot({
                .unlocked = []() {
                    if (!g_configCollectionStarterEquip) return false;
                    if (g_configCollectionKeepOrdonShield) {
                        return dComIfGs_isCollectShield(COLLECT_WOODEN_SHIELD) ||
                            dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e);
                    }
                    const u8 eq = custom_equip_active(CE_SHIELD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipShield();
                    return dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) || (eq == dItemNo_WOOD_SHIELD_e);
                },
                .equipped = []() { return dComIfGs_getSelectEquipShield() == dItemNo_WOOD_SHIELD_e; },
            });
        }

        if (g_configCollectionStarterEquip) {
            const bool linkle = is_mod_installed("com.ditrey.linkle");
            add_vanilla_tunic_slot({
                .unlocked = []() { return g_configCollectionStarterEquip; },
                .equipped = []() { return dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e; },
                .name = linkle ? "Linkle's Clothes" : "Ordon Clothes",
                .description = linkle
                    ? "The clothes Linkle wore at the beginning of her journey in Ordon Village."
                    : "The clothes Link wore at the beginning of his journey in Ordon Village.",
                .icon = get_ordon_clothes_texture(),
            });
        }

        register_custom_swords();
        register_custom_shields();
        register_custom_tunics();
    });

    return collectionlib_init(hook_svc, log_svc, save_svc, mod_ctx, error);
}

void update_collection_menu(const LogService*, ModContext*) {
    collectionlib_update();
}

void shutdown_collection_menu() {
    delete s_ordonHeroPage;
    s_ordonHeroPage = nullptr;
    collectionlib_shutdown();
}

void request_collection_menu_reload() {
    collectionlib_request_reload();
}
