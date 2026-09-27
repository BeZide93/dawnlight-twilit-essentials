#include "collection_menu.hpp"
#include "../util.hpp"
#include "../compat/twilight_hd.hpp"

#include <collection_lib/collection_lib.hpp>

#include <cstring>

bool g_configCollectionStarterEquip = false;
bool g_configCollectionKeepOrdonShield = false;
bool g_configCollectionShowOrdonHero = false;
bool g_configCollectionOrdonHeroAlways = false;

static cl::Page* s_ordonHeroPage = nullptr;
static bool s_linkleActive = false;

constexpr const char* kLinkleModId = "com.ditrey.linkle";

bool collection_linkle_active() {
    return is_mod_enabled(kLinkleModId);
}

// The Ordon Hero gear is made for Link's model, so it stays off with Linkle.
bool collection_ordon_hero_enabled() {
    return g_configCollectionShowOrdonHero && !collection_linkle_active();
}

constexpr const char* kOrdonHeroTunicName = "Ordon Hero";
constexpr const char* kReinforcedShieldName = "Reinforced Shield";

// Gear equipped before Linkle was turned on comes off, it has no Linkle model.
static void unequip_ordon_hero_gear() {
    const CustomEquipKind kinds[] = {CE_TUNIC, CE_SHIELD};
    const char* const names[] = {kOrdonHeroTunicName, kReinforcedShieldName};
    for (int i = 0; i < 2; ++i) {
        const int id = collectionlib_active_id(kinds[i]);
        const CustomEquipDef* def = id >= 0 ? custom_equip_get(id) : nullptr;
        if (def != nullptr && def->name != nullptr && std::strcmp(def->name, names[i]) == 0) {
            collectionlib_clear(kinds[i]);
        }
    }
}

static bool player_has_hero_clothes() {
    return dComIfGs_isCollectClothes(KOKIRI_CLOTHES_FLAG) ||
           dComIfGs_isItemFirstBit(dItemNo_WEAR_KOKIRI_e);
}

static bool player_has_ordon_shield() {
    return dComIfGs_isCollectShield(COLLECT_WOODEN_SHIELD) ||
           dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e);
}

void sync_collection_ordon_hero_page() {
    const bool enabled = collection_ordon_hero_enabled();
    if (enabled && s_ordonHeroPage == nullptr) {
        cl::Page* p2 = new cl::Page();
        p2->add(cl::heart());
        p2->add(cl::crystal());
        p2->add(cl::fused_shadow());
        s_ordonHeroPage = p2;
    } else if (!enabled && s_ordonHeroPage != nullptr) {
        delete s_ordonHeroPage;
        s_ordonHeroPage = nullptr;
    }
}

static bool starter_sword_unlocked() {
    const u8 eq = custom_equip_active(CE_SWORD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipSword();
    return dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e) || (eq == dItemNo_WOOD_STICK_e) ||
           dComIfGs_isItemFirstBit(dItemNo_SWORD_e) || (eq == dItemNo_SWORD_e) ||
           dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) || (eq == dItemNo_MASTER_SWORD_e) ||
           dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) || (eq == dItemNo_LIGHT_SWORD_e);
}

static bool starter_shield_unlocked() {
    if (g_configCollectionKeepOrdonShield) {
        return player_has_ordon_shield();
    }
    const u8 eq = custom_equip_active(CE_SHIELD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipShield();
    return dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) || (eq == dItemNo_WOOD_SHIELD_e);
}

static void register_starter_gear() {
    if (!g_configCollectionStarterEquip) {
        return;
    }
    get_slot(1, 1).insert({
        .kind = CE_SWORD,
        .baseItem = dItemNo_WOOD_STICK_e,
        .unlocked = &starter_sword_unlocked,
    });
    get_slot(2, 1).insert({
        .kind = CE_SHIELD,
        .baseItem = dItemNo_WOOD_SHIELD_e,
        .unlocked = &starter_shield_unlocked,
    });

    const bool linkle = collection_linkle_active();
    get_slot(3, 1).insert({
        .kind = CE_TUNIC,
        .name = linkle ? "Linkle's Clothes" : "Ordon Clothes",
        .description = linkle
            ? "The clothes Linkle wore at the beginning\nof her journey in Ordon Village."
            : "The clothes Link wore at the beginning\nof his journey in Ordon Village.",
        .iconBti = linkle ? "textures/ordon_clothes_linkle.bti" : "textures/ordon_clothes.bti",
        .baseItem = dItemNo_WEAR_CASUAL_e,
    });
}

void register_custom_swords() {

}

void register_custom_shields() {
    if (!collection_ordon_hero_enabled()) {
        return;
    }
    if (!g_configCollectionOrdonHeroAlways && !player_has_hero_clothes()) {
        return;
    }
    collectionlib_add_next_shield_slot({
        CE_SHIELD, 0,
        kReinforcedShieldName,
        "A traditional Ordon shield reinforced with metal. Stronger and will never burn.",
        "textures/clctres/reinforced_shield.bti",
        nullptr,
        "models/clctres/ReinforcedShield.arc", 0x0003,
    });

}

void register_custom_tunics() {
    if (!collection_ordon_hero_enabled()) {
        return;
    }
    if (!g_configCollectionOrdonHeroAlways && !player_has_hero_clothes()) {
        return;
    }
    collectionlib_add_next_tunic_slot({
        CE_TUNIC, 0,
        kOrdonHeroTunicName,
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
    s_linkleActive = collection_linkle_active();
    sync_collection_ordon_hero_page();

    collectionlib_set_keep_ordon_shield_policy([]() { return g_configCollectionKeepOrdonShield; });
    collectionlib_set_hd_layout_policy(&twilight_hd_collection);

    collectionlib_set_register_callback([]() {
        register_starter_gear();
        register_custom_swords();
        register_custom_shields();
        register_custom_tunics();
    });

    return collectionlib_init(hook_svc, log_svc, save_svc, mod_ctx, error);
}

void update_collection_menu(const LogService*, ModContext*) {
    // Linkle can be switched on or off while the game runs.
    const bool linkle = collection_linkle_active();
    if (linkle != s_linkleActive) {
        s_linkleActive = linkle;
        sync_collection_ordon_hero_page();
        request_collection_menu_reload();
    }
    if (linkle) {
        unequip_ordon_hero_gear();
    }
    if (g_configCollectionStarterEquip && g_configCollectionKeepOrdonShield &&
        dComIfGs_isCollectShield(COLLECT_WOODEN_SHIELD) &&
        !dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) {
        dComIfGs_onItemFirstBit(dItemNo_WOOD_SHIELD_e);
    }
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
