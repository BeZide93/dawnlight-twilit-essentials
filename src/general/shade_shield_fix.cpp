#include "shade_shield_fix.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_flow.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

DEFINE_HOOK(&daAlink_c::execute, ShadeShieldAlinkExecuteHook);
DEFINE_HOOK(&dMsgFlow_c::query021, ShadeShieldQueryEquip);
DEFINE_HOOK(&dMsgFlow_c::query022, ShadeShieldQueryOwn);

static u8 best_owned_shield() {
    if (dComIfGs_isItemFirstBit(dItemNo_HYLIA_SHIELD_e)) return dItemNo_HYLIA_SHIELD_e;
    if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) return dItemNo_SHIELD_e;
    if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) return dItemNo_WOOD_SHIELD_e;
    return dItemNo_NONE_e;
}

static bool owns_any_shield() {
    return best_owned_shield() != dItemNo_NONE_e;
}

static bool is_shield_item(u16 item) {
    return item == dItemNo_WOOD_SHIELD_e || item == dItemNo_SHIELD_e || item == dItemNo_HYLIA_SHIELD_e;
}

static bool s_knPresent = false;

static void on_shade_shield_alink_execute_post(ModContext*, void*, void*, void*) {
    s_knPresent = fopAcM_SearchByName(fpcNm_NPC_KN_e) != nullptr();
    if (!s_knPresent) return;

    if (dComIfGs_getSelectEquipShield() != dItemNo_NONE_e) return;
    if (daPy_py_c::checkNowWolf()) return;

    const u8 shield = best_owned_shield();
    if (shield == dItemNo_NONE_e) return;

    dMeter2Info_setShield(shield, false);
    if (daAlink_c* link = daAlink_getAlinkActorClass()) {
        link->setShieldChange();
    }
}

// "is this item equipped?" returns 1 when equipped
static void on_shade_shield_query_equip_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || args == nullptr || !s_knPresent) return;
    const mesg_flow_node_branch* node = mods::arg<mesg_flow_node_branch*>(args, 1);
    if (node == nullptr) return;
    const u16 prm = node->param;
    if (!is_shield_item(prm)) return;

    if (owns_any_shield() && *static_cast<u16*>(retval) != 1) {
        *static_cast<u16*>(retval) = 1;
    }
}

// "does the player own this item?" returns 0 when owned
static void on_shade_shield_query_own_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || args == nullptr || !s_knPresent) return;
    const mesg_flow_node_branch* node = mods::arg<mesg_flow_node_branch*>(args, 1);
    if (node == nullptr) return;
    const u16 prm = node->param;
    if (!is_shield_item(prm)) return;

    if (owns_any_shield() && *static_cast<u16*>(retval) != 0) {
        *static_cast<u16*>(retval) = 0;
    }
}

ModResult init_shade_shield_fix(const HookService* hook_svc, ModError*) {
    if (hook_svc == nullptr) return MOD_ERROR;

    const ModResult alink =
        mods::hook::add_post<ShadeShieldAlinkExecuteHook>(hook_svc, on_shade_shield_alink_execute_post);
    const ModResult queryEquip =
        mods::hook::add_post<ShadeShieldQueryEquip>(hook_svc, on_shade_shield_query_equip_post);
    const ModResult queryOwn =
        mods::hook::add_post<ShadeShieldQueryOwn>(hook_svc, on_shade_shield_query_own_post);

    return (alink == MOD_OK && queryEquip == MOD_OK && queryOwn == MOD_OK) ? MOD_OK : MOD_ERROR;
}

void shutdown_shade_shield_fix() {}
