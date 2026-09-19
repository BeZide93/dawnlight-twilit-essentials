#include "boss_rush_equipment.hpp"

#include "global.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "d/d_meter2_info.h"

#include <cstring>

bool g_configBossRushVanillaGear = false;

namespace {

enum {
    IT_BOOM  = 1 << 0,
    IT_BOOTS = 1 << 1,
    IT_BOW   = 1 << 2,
    IT_CLAW  = 1 << 3,
    IT_SPIN  = 1 << 4,
    IT_BALL  = 1 << 5,
    IT_ROD   = 1 << 6,
};

constexpr u32 IT_ZANT_ALL = IT_BOOM | IT_BOOTS | IT_BOW | IT_CLAW | IT_SPIN | IT_BALL | IT_ROD;

struct GearRule {
    const char* name;
    u8   hearts;
    u32  items;
    u8   skills;
    bool masterSword;
    u8   tunicTier;
};

constexpr GearRule kRules[] = {
    {"Ook",             3, 0,                1, false, 0},
    {"Diababa",         3, IT_BOOM,          1, false, 0},
    {"Dangoro",         4, IT_BOOTS,         2, false, 0},
    {"Fyrus",           4, IT_BOW | IT_BOOTS,               2, false, 0},
    {"Deku Toad",       5, 0,                3, false, 0},
    {"Morpheel",        5, IT_CLAW | IT_BOOTS,              3, false, 1},
    {"Death Sword",     6, IT_BOW | IT_CLAW, 4, true,  0},
    {"Stallord",        6, IT_SPIN,          4, true,  0},
    {"Darkhammer",      7, IT_CLAW,          5, true,  0},
    {"Blizzeta",        7, IT_BALL,          5, true,  0},
    {"Darknut",         8, 0,                6, true,  0},
    {"Armogohma",       8, IT_BOW | IT_ROD,  6, true,  0},
    {"Aeralfos",        9, IT_CLAW,          7, true,  0},
    {"Argorok",         9, IT_CLAW | IT_BOOTS,              7, true,  0},
    {"Zant",           10, IT_ZANT_ALL,      7, true,  2},
    {"Puppet Zelda",   11, IT_BOW,           7, true,  0},
    {"Beast Ganon",    11, IT_BOW,           7, true,  0},
    {"Horseback Ganon",11, IT_BOW,           7, true,  0},
    {"Ganondorf",      11, IT_BOW,           7, true,  0},
};

const GearRule* find_rule(const char* name) {
    if (name == nullptr) return nullptr;
    for (const GearRule& r : kRules) {
        if (std::strcmp(r.name, name) == 0) return &r;
    }
    return nullptr;
}

void strip_slot(int slot) {
    dComIfGs_setItem(slot, dItemNo_NONE_e);
    dComIfGp_setItem(slot, dItemNo_NONE_e);
}

void keep_or_strip(int slot, u32 have, u32 need) {
    if ((have & need) == 0) strip_slot(slot);
}

}

void apply_boss_rush_equipment_restriction(const BossGalleryEntry& boss) {
    const GearRule* rule = find_rule(boss.displayName);
    if (rule == nullptr) {
        return;
    }

    const u8 maxLife = static_cast<u8>(rule->hearts * 5);
    dComIfGs_setMaxLife(maxLife);
    dComIfGs_setLife(maxLife);

    const u32 have = rule->items;
    keep_or_strip(SLOT_0,  have, IT_BOOM);
    keep_or_strip(SLOT_2,  have, IT_SPIN);
    keep_or_strip(SLOT_3,  have, IT_BOOTS);
    keep_or_strip(SLOT_4,  have, IT_BOW);
    keep_or_strip(SLOT_5,  have, IT_BOW);
    keep_or_strip(SLOT_6,  have, IT_BALL);
    keep_or_strip(SLOT_8,  have, IT_ROD);
    keep_or_strip(SLOT_10, have, IT_CLAW);
    strip_slot(SLOT_15);
    strip_slot(SLOT_16);
    strip_slot(SLOT_17);

    static const u16 kSkillFlags[7] = {
        dSv_event_flag_c::F_0338, dSv_event_flag_c::F_0339, dSv_event_flag_c::F_0340,
        dSv_event_flag_c::F_0341, dSv_event_flag_c::F_0342, dSv_event_flag_c::F_0343,
        dSv_event_flag_c::F_0344,
    };
    for (u8 i = 0; i < 7; ++i) {
        if (i < rule->skills) dComIfGs_onEventBit(kSkillFlags[i]);
        else                  dComIfGs_offEventBit(kSkillFlags[i]);
    }

    constexpr u8 kOrdonShield = dItemNo_WOOD_SHIELD_e;
    constexpr u8 kWoodShield  = dItemNo_SHIELD_e;
    constexpr u8 kHyliaShield = dItemNo_HYLIA_SHIELD_e;

    const u8 sword  = rule->masterSword ? dItemNo_MASTER_SWORD_e : dItemNo_SWORD_e;
    const u8 shield = rule->masterSword ? kHyliaShield : kOrdonShield;

    auto& getItem = g_dComIfG_gameInfo.info.getPlayer().getGetItem();

    getItem.offFirstBit(kWoodShield);

    getItem.offFirstBit(dItemNo_WOOD_STICK_e);
    getItem.offFirstBit(dItemNo_LIGHT_SWORD_e);

    if (rule->masterSword) {
        getItem.offFirstBit(dItemNo_SWORD_e);
    } else {
        getItem.offFirstBit(kHyliaShield);
        getItem.offFirstBit(dItemNo_MASTER_SWORD_e);
    }

    dComIfGs_onItemFirstBit(kOrdonShield);

    dComIfGs_setCollectSword(rule->masterSword ? COLLECT_MASTER_SWORD : COLLECT_ORDON_SWORD);
    dComIfGs_setCollectShield(rule->masterSword ? COLLECT_HYLIAN_SHIELD : COLLECT_ORDON_SHIELD);
    dComIfGs_onItemFirstBit(sword);
    dComIfGs_onItemFirstBit(shield);

    dMeter2Info_setSword(sword, false);
    dMeter2Info_setShield(shield, false);
    dComIfGs_setSelectEquipSword(sword);
    dComIfGp_setSelectEquipSword(sword);
    dComIfGs_setSelectEquipShield(shield);
    dComIfGp_setSelectEquipShield(shield);

    if (rule->tunicTier < 1) getItem.offFirstBit(dItemNo_WEAR_ZORA_e);
    if (rule->tunicTier < 2) getItem.offFirstBit(dItemNo_ARMOR_e);

    const u8 curClothes = dComIfGs_getSelectEquipClothes();
    bool forceKokiri = false;
    if (rule->tunicTier == 0) {
        forceKokiri = (curClothes != dItemNo_WEAR_KOKIRI_e);
    } else if (rule->tunicTier == 1) {
        forceKokiri = (curClothes == dItemNo_ARMOR_e);
    }
    if (forceKokiri) {
        dMeter2Info_setCloth(dItemNo_WEAR_KOKIRI_e, false);
        dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
        dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr) {
        link->setClothesChange(0);
        link->setSelectEquipItem(FALSE);
    }
}
