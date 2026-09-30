#include "collection_menu.hpp"
#include "../util.hpp"
#include "../compat/twilight_hd.hpp"

#include <collection_lib/collection_lib.hpp>
#include "d/actor/d_a_alink.h"
#include "d/d_meter2_info.h"
#include "Z2AudioLib/Z2AudioMgr.h"

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
    collectionlib_set_unequipped_tunic("/res/Object/alSumou.arc", 0xFFFF, dItemNo_WEAR_KOKIRI_e, 0xF0A878u);

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

ResTIMG* cl_load_icon(const char* path, IconArcRef iconArc);
bool custom_equip_toggle(int id);
void collectionlib_run_slot_registration();

DEFINE_HOOK(&daAlink_c::execute, CollectionTunicChangeExecuteHook);

static bool s_safeClothesChange = false;

static HookAction on_tunic_change_execute_pre(ModContext*, void* args, void*, void*) {
    if (!s_safeClothesChange) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return HOOK_CONTINUE;
    if (link->getClothesChangeWaitTimer() == 0) {
        s_safeClothesChange = false;
        return HOOK_CONTINUE;
    }
    link->loadModelDVD();
    if (link->getClothesChangeWaitTimer() == 0) {
        s_safeClothesChange = false;
        return HOOK_CONTINUE;
    }
    return HOOK_SKIP_ORIGINAL;
}

static void start_safe_clothes_change(daAlink_c* link) {
    s_safeClothesChange = true;
    link->setClothesChange(0);
}

static bool link_floating_still(daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_SWIM_WAIT &&
           link->checkNoResetFlg0(daPy_py_c::FLG0_SWIM_UP);
}

static bool link_can_change_clothes(daAlink_c* link) {
    if (link == nullptr || link->checkWolf() || link->getClothesChangeWaitTimer() != 0 ||
        link->checkEventRun() || link->checkRide()) {
        return false;
    }
    if (link_floating_still(link)) return true;
    return !link->checkPlayerFly() && link->mLinkAcch.ChkGroundHit();
}

static int find_ordon_hero_tunic_id() {
    for (int pass = 0; pass < 2; ++pass) {
        for (int id = 0; id < custom_equip_count(); ++id) {
            const CustomEquipDef* def = custom_equip_get(id);
            if (def != nullptr && def->kind == CE_TUNIC && def->name != nullptr &&
                std::strcmp(def->name, kOrdonHeroTunicName) == 0) {
                return id;
            }
        }
        if (pass == 0) collectionlib_run_slot_registration();
    }
    return -1;
}

static u8 native_tunic_item(int tunic) {
    switch (tunic) {
    case COLLECTION_TUNIC_ORDON: return dItemNo_WEAR_CASUAL_e;
    case COLLECTION_TUNIC_HERO: return dItemNo_WEAR_KOKIRI_e;
    case COLLECTION_TUNIC_ZORA: return dItemNo_WEAR_ZORA_e;
    case COLLECTION_TUNIC_MAGIC: return dItemNo_ARMOR_e;
    default: return dItemNo_NONE_e;
    }
}

static bool ordon_hero_tunic_active() {
    if (!custom_equip_active(CE_TUNIC)) return false;
    const CustomEquipDef* def = custom_equip_get(custom_equip_active_id(CE_TUNIC));
    return def != nullptr && def->name != nullptr &&
           std::strcmp(def->name, kOrdonHeroTunicName) == 0;
}

bool collection_tunic_unlocked(int tunic) {
    switch (tunic) {
    case COLLECTION_TUNIC_ORDON:
        return g_configCollectionStarterEquip ||
               dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e;
    case COLLECTION_TUNIC_HERO:
        return player_has_hero_clothes();
    case COLLECTION_TUNIC_ZORA:
        return dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e);
    case COLLECTION_TUNIC_MAGIC:
        return dComIfGs_isItemFirstBit(dItemNo_ARMOR_e);
    case COLLECTION_TUNIC_ORDON_HERO:
        return collection_ordon_hero_enabled() &&
               (g_configCollectionOrdonHeroAlways || player_has_hero_clothes());
    default:
        return false;
    }
}

bool collection_tunic_equipped(int tunic) {
    if (tunic == COLLECTION_TUNIC_ORDON_HERO) return ordon_hero_tunic_active();
    if (custom_equip_active(CE_TUNIC)) return false;
    const u8 item = native_tunic_item(tunic);
    return item != dItemNo_NONE_e && dComIfGs_getSelectEquipClothes() == item;
}

bool collection_tunic_equip(int tunic) {
    if (!collection_tunic_unlocked(tunic)) return false;
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (collection_tunic_equipped(tunic)) {
        if (!link_can_change_clothes(link)) return false;
        const u8 before = dComIfGs_getSelectEquipClothes();
        if (!collectionlib_unequip_tunic()) return true;
        if (dComIfGs_getSelectEquipClothes() != before) start_safe_clothes_change(link);
        return true;
    }
    if (!link_can_change_clothes(link)) return false;

    if (tunic == COLLECTION_TUNIC_ORDON_HERO) {
        const int id = find_ordon_hero_tunic_id();
        const u8 before = dComIfGs_getSelectEquipClothes();
        if (id < 0 || !custom_equip_toggle(id)) return false;
        if (dComIfGs_getSelectEquipClothes() != before) start_safe_clothes_change(link);
        return true;
    }

    const u8 item = native_tunic_item(tunic);
    if (item == dItemNo_NONE_e) return false;
    if (custom_equip_active(CE_TUNIC)) collectionlib_clear(CE_TUNIC);
    dMeter2Info_setCloth(item, false);
    start_safe_clothes_change(link);
    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
    return true;
}

const char* collection_tunic_name(int tunic) {
    switch (tunic) {
    case COLLECTION_TUNIC_ORDON: return collection_linkle_active() ? "Linkle's Clothes" : "Ordon Clothes";
    case COLLECTION_TUNIC_HERO: return "Hero's Clothes";
    case COLLECTION_TUNIC_ZORA: return "Zora Armor";
    case COLLECTION_TUNIC_MAGIC: return "Magic Armor";
    case COLLECTION_TUNIC_ORDON_HERO: return kOrdonHeroTunicName;
    default: return "";
    }
}

u8 collection_tunic_icon_item(int tunic) {
    return tunic == COLLECTION_TUNIC_ORDON ? static_cast<u8>(dItemNo_NONE_e) : native_tunic_item(tunic);
}

ResTIMG* collection_tunic_icon(int tunic) {
    switch (tunic) {
    case COLLECTION_TUNIC_ORDON:
        return cl_load_icon(collection_linkle_active() ? "textures/ordon_clothes_linkle.bti"
                                                       : "textures/ordon_clothes.bti",
                            nullptr);
    case COLLECTION_TUNIC_ORDON_HERO: return cl_load_icon("textures/clctres/ordonhero.bti", nullptr);
    default: return nullptr;
    }
}

ModResult init_collection_menu(const HookService* hook_svc, const LogService* log_svc,
                               const SaveService* save_svc, ModContext* mod_ctx,
                               ModError* error) {
    s_linkleActive = collection_linkle_active();
    sync_collection_ordon_hero_page();

    collectionlib_set_unequip_policy([]() { return true; });
    collectionlib_set_keep_ordon_shield_policy([]() { return g_configCollectionKeepOrdonShield; });
    collectionlib_set_hd_layout_policy(&twilight_hd_collection);

    if (hook_svc != nullptr) {
        mods::hook::add_pre<CollectionTunicChangeExecuteHook>(hook_svc, on_tunic_change_execute_pre);
    }

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
