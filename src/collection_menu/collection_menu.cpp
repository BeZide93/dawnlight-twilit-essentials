#include "collection_menu.hpp"
#include "../util.hpp"
#include "../compat/twilight_hd.hpp"
#include "../boss_rush/boss_rush_darklink.hpp"

#include <collection_lib/collection_lib.hpp>
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_mtx.h"
#include "Z2AudioLib/Z2AudioMgr.h"

#include <algorithm>
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

static int s_unequippedTunicId = -1;

void register_custom_tunics() {
    s_unequippedTunicId =
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

static constexpr int kPoseParts = 4;
static constexpr u16 kPoseMaxJoints = 48;

struct LinkPoseCache {
    bool valid = false;
    fpc_ProcID linkId = fpcM_ERROR_PROCESS_ID_e;
    Mtx base[kPoseParts];
    Mtx joints[kPoseParts][kPoseMaxJoints];
    u16 num[kPoseParts] = {};
};

static LinkPoseCache s_poseCache;

static J3DModel* link_pose_part(daAlink_c* link, int part) {
    switch (part) {
    case 0: return link->mpLinkModel;
    case 1: return link->mpLinkHatModel;
    case 2: return link->mpLinkFaceModel;
    default: return link->mpLinkHandModel;
    }
}

static void cache_link_pose(daAlink_c* link) {
    s_poseCache.valid = false;
    if (link->checkWolf()) return;
    for (int part = 0; part < kPoseParts; part++) {
        J3DModel* model = link_pose_part(link, part);
        if (model == nullptr || model->getModelData() == nullptr) return;
        const u16 num = std::min<u16>(model->getModelData()->getJointNum(), kPoseMaxJoints);
        mDoMtx_copy(model->getBaseTRMtx(), s_poseCache.base[part]);
        for (u16 j = 0; j < num; j++) {
            mDoMtx_copy(model->getAnmMtx(j), s_poseCache.joints[part][j]);
        }
        s_poseCache.num[part] = num;
    }
    s_poseCache.linkId = fopAcM_GetID(link);
    s_poseCache.valid = true;
}

static void restore_link_pose(daAlink_c* link) {
    if (!s_poseCache.valid || s_poseCache.linkId != fopAcM_GetID(link) || link->checkWolf()) return;
    for (int part = 0; part < kPoseParts; part++) {
        J3DModel* model = link_pose_part(link, part);
        if (model == nullptr || model->getModelData() == nullptr) continue;
        const u16 num = std::min<u16>(model->getModelData()->getJointNum(), s_poseCache.num[part]);
        model->setBaseTRMtx(s_poseCache.base[part]);
        for (u16 j = 0; j < num; j++) {
            mDoMtx_copy(s_poseCache.joints[part][j], model->getAnmMtx(j));
        }
    }
}

static void step_seamless_clothes_change(daAlink_c* link);

static HookAction on_tunic_change_execute_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = args != nullptr ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    if (link == nullptr) return HOOK_CONTINUE;
    if (link == daAlink_getAlinkActorClass()) {
        if (!s_safeClothesChange && link->getClothesChangeWaitTimer() == 0) cache_link_pose(link);
        step_seamless_clothes_change(link);
    }
    if (!s_safeClothesChange) return HOOK_CONTINUE;
    if (link->getClothesChangeWaitTimer() == 0) {
        s_safeClothesChange = false;
        return HOOK_CONTINUE;
    }
    link->loadModelDVD();
    if (link->getClothesChangeWaitTimer() == 0) {
        s_safeClothesChange = false;
        restore_link_pose(link);
        return HOOK_CONTINUE;
    }
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&daAlink_c::setFootSpeed, CollectionFootSpeedHook);

static bool s_footSyncStale = false;
static bool s_footSyncWasOff = false;

static HookAction on_foot_speed_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    s_footSyncWasOff = link != nullptr && link->field_0x2060 != nullptr &&
                       !link->field_0x2060->getOldFrameFlg();
    return HOOK_CONTINUE;
}

static void on_foot_speed_post(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link != nullptr && s_footSyncStale && !s_footSyncWasOff) {
        link->field_0x33a0 = link->mNormalSpeed;
    }
    s_footSyncStale = s_footSyncWasOff;
}

static void start_safe_clothes_change(daAlink_c* link) {
    s_safeClothesChange = true;
    link->setClothesChange(0);
}

static constexpr u32 kSeamlessHeapSize = 0x600000;
static constexpr u32 kSeamlessRootReserve = 0x800000;
static constexpr int kSeamlessMaxTicks = 300;

struct SeamlessClothesChange {
    bool pending = false;
    bool draining = false;
    bool registered = false;
    u8 item = 0;
    int customId = -1;
    const char* arc = nullptr;
    fpc_ProcID linkId = fpcM_ERROR_PROCESS_ID_e;
    int ticks = 0;
    request_of_phase_process_class phase = {};
};

static SeamlessClothesChange s_seamless;
static JKRExpHeap* s_seamlessHeap = nullptr;

static const char* clothes_arc_name(u8 item) {
    switch (item) {
    case dItemNo_WEAR_CASUAL_e: return "Bmdl";
    case dItemNo_WEAR_KOKIRI_e: return "Kmdl";
    case dItemNo_WEAR_ZORA_e: return "Zmdl";
    case dItemNo_ARMOR_e: return "Mmdl";
    default: return nullptr;
    }
}

static JKRExpHeap* seamless_heap() {
    if (s_seamlessHeap != nullptr) return s_seamlessHeap;
    JKRHeap* root = JKRHeap::getRootHeap();
    if (root == nullptr || static_cast<u32>(root->getFreeSize()) < kSeamlessHeapSize + kSeamlessRootReserve) {
        return nullptr;
    }
    s_seamlessHeap = JKRExpHeap::create(kSeamlessHeapSize, root, false);
    return s_seamlessHeap;
}

static int seamless_poll() {
    const int state = dComIfG_resLoad(&s_seamless.phase, s_seamless.arc, s_seamlessHeap);
    if (state == cPhs_ERROR_e) {
        if (s_seamless.registered) dComIfG_deleteObjectResMain(s_seamless.arc);
        s_seamless.registered = false;
    } else {
        s_seamless.registered = true;
    }
    return state;
}

static void seamless_begin_drain() {
    s_seamless.pending = false;
    s_seamless.draining = s_seamless.registered;
    if (!s_seamless.draining) s_seamless = SeamlessClothesChange{};
}

static void seamless_drain() {
    if (!s_seamless.draining) return;
    const int state = seamless_poll();
    if (state == cPhs_COMPLEATE_e) {
        dComIfG_resDelete(&s_seamless.phase, s_seamless.arc);
        s_seamless = SeamlessClothesChange{};
    } else if (state == cPhs_ERROR_e) {
        s_seamless = SeamlessClothesChange{};
    }
}

static bool start_seamless_clothes_change(daAlink_c* link, u8 item, int customId = -1) {
    if (s_seamless.pending || s_seamless.draining) return false;
    if (item == dComIfGs_getSelectEquipClothes()) return false;
    const char* arc = clothes_arc_name(item);
    if (arc == nullptr || seamless_heap() == nullptr) return false;
    s_seamless = SeamlessClothesChange{};
    s_seamless.pending = true;
    s_seamless.item = item;
    s_seamless.customId = customId;
    s_seamless.arc = arc;
    s_seamless.linkId = fopAcM_GetID(link);
    if (seamless_poll() == cPhs_ERROR_e) {
        s_seamless = SeamlessClothesChange{};
        return false;
    }
    return true;
}

static void finish_seamless_clothes_change(daAlink_c* link) {
    link->mEyeHL1.remove();
    link->mEyeHL2.remove();
    link->mpWlMidnaModel = nullptr;
    link->mpWlMidnaMaskModel = nullptr;
    link->mpWlMidnaHandModel = nullptr;
    link->mpWlMidnaHairModel = nullptr;
    if (!dComIfG_resDelete(&link->mPhaseReq, link->mArcName)) {
        dComIfG_deleteObjectResMain(link->mArcName);
    }
    link->mPhaseReq = s_seamless.phase;
    dMeter2Info_setCloth(s_seamless.item, false);
    if (s_seamless.customId >= 0) {
        custom_equip_activate(s_seamless.customId);
    } else if (custom_equip_active(CE_TUNIC)) {
        collectionlib_clear(CE_TUNIC);
    }
    link->setArcName(link->checkWolf());
    const bool keepBlend = link->field_0x2060 != nullptr && link->field_0x2060->getOldFrameFlg();
    link->changeLink(1);
    if (keepBlend) link->field_0x2060->onOldFrameFlg();
    restore_link_pose(link);
    s_seamless = SeamlessClothesChange{};
}

static void step_seamless_clothes_change(daAlink_c* link) {
    if (!s_seamless.pending) return;
    if (fopAcM_GetID(link) != s_seamless.linkId || link->checkWolf() ||
        link->getClothesChangeWaitTimer() != 0 || link->mProcID == daAlink_c::PROC_METAMORPHOSE ||
        link->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY || ++s_seamless.ticks > kSeamlessMaxTicks) {
        seamless_begin_drain();
        return;
    }
    const int state = seamless_poll();
    if (state == cPhs_COMPLEATE_e) {
        finish_seamless_clothes_change(link);
    } else if (state == cPhs_ERROR_e) {
        const u8 item = s_seamless.item;
        s_seamless = SeamlessClothesChange{};
        dMeter2Info_setCloth(item, false);
        start_safe_clothes_change(link);
    }
}

bool collection_tunic_change_pending() {
    return s_seamless.pending;
}

static bool link_floating_still(daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_SWIM_WAIT &&
           link->checkNoResetFlg0(daPy_py_c::FLG0_SWIM_UP);
}

static bool link_can_change_clothes(daAlink_c* link, bool allowSwimming = false, bool anyPose = false) {
    if (link == nullptr || link->checkWolf() || link->getClothesChangeWaitTimer() != 0 ||
        link->checkEventRun() || link->checkRide() || s_seamless.pending) {
        return false;
    }
    if (anyPose || link_floating_still(link)) return true;
    if (allowSwimming && link->checkModeFlg(daAlink_c::MODE_SWIMMING)) return true;
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

static void tunic_equip_feedback(bool silent) {
    if (silent) return;
    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

static bool start_seamless_custom_tunic(daAlink_c* link, int id) {
    const CustomEquipDef* def = id >= 0 ? custom_equip_get(id) : nullptr;
    if (def == nullptr || def->kind != CE_TUNIC || custom_equip_active_id(CE_TUNIC) == id) return false;
    const u8 base = def->baseItem != dItemNo_NONE_e ? def->baseItem : static_cast<u8>(dItemNo_WEAR_KOKIRI_e);
    return start_seamless_clothes_change(link, base, id);
}

bool collection_tunic_equip(int tunic, unsigned flags) {
    if (!collection_tunic_unlocked(tunic)) return false;
    const bool allowSwimming = (flags & TUNIC_EQUIP_SWIMMING) != 0;
    const bool seamless = (flags & TUNIC_EQUIP_SEAMLESS) != 0;
    const bool silent = (flags & TUNIC_EQUIP_SILENT) != 0;
    const bool anyPose = seamless && (flags & TUNIC_EQUIP_ANY_POSE) != 0;
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (collection_tunic_equipped(tunic)) {
        if (!link_can_change_clothes(link, allowSwimming, anyPose)) return false;
        if (seamless && start_seamless_custom_tunic(link, s_unequippedTunicId)) return true;
        if (anyPose) return false;
        const u8 before = dComIfGs_getSelectEquipClothes();
        if (!collectionlib_unequip_tunic()) return true;
        if (dComIfGs_getSelectEquipClothes() != before) start_safe_clothes_change(link);
        return true;
    }
    if (!link_can_change_clothes(link, allowSwimming, anyPose)) return false;

    if (tunic == COLLECTION_TUNIC_ORDON_HERO) {
        const int id = find_ordon_hero_tunic_id();
        if (seamless && start_seamless_custom_tunic(link, id)) {
            tunic_equip_feedback(silent);
            return true;
        }
        if (anyPose) return false;
        const u8 before = dComIfGs_getSelectEquipClothes();
        if (id < 0 || !custom_equip_toggle(id)) return false;
        if (dComIfGs_getSelectEquipClothes() != before) start_safe_clothes_change(link);
        return true;
    }

    const u8 item = native_tunic_item(tunic);
    if (item == dItemNo_NONE_e) return false;
    if (seamless && start_seamless_clothes_change(link, item)) {
        tunic_equip_feedback(silent);
        return true;
    }
    if (anyPose) return false;
    if (custom_equip_active(CE_TUNIC)) collectionlib_clear(CE_TUNIC);
    dMeter2Info_setCloth(item, false);
    start_safe_clothes_change(link);
    tunic_equip_feedback(silent);
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

    collectionlib_set_unequip_policy([](CustomEquipKind kind) {
        return kind == CE_TUNIC || !boss_rush_darklink_gear_locked();
    });
    collectionlib_set_keep_ordon_shield_policy([]() { return g_configCollectionKeepOrdonShield; });
    collectionlib_set_hd_layout_policy(&twilight_hd_collection);

    if (hook_svc != nullptr) {
        mods::hook::add_pre<CollectionTunicChangeExecuteHook>(hook_svc, on_tunic_change_execute_pre);
        mods::hook::add_pre<CollectionFootSpeedHook>(hook_svc, on_foot_speed_pre);
        mods::hook::add_post<CollectionFootSpeedHook>(hook_svc, on_foot_speed_post);
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
    if (s_seamless.pending) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link == nullptr || fopAcM_GetID(link) != s_seamless.linkId) seamless_begin_drain();
    }
    seamless_drain();
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
        (g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusB().mDarkClearLevelFlag & 1) &&
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
