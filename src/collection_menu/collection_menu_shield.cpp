#include "collection_menu_shield.hpp"
#include "collection_menu.hpp"
#include "../quick_access/quick_access.hpp"

#include "mods/svc/stage.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/item.h"

#include "d/d_stage.h"
#include "d/d_item_data.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_tbox.h"
#include "d/actor/d_a_obj_shield.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstring>

extern const StageService* svc_stage;
extern const ItemService* svc_item;
extern ModContext* mod_ctx;

namespace {

constexpr const char* kChestStage = "R_SP01";
constexpr u8 kChestRoom = 4;
constexpr u8 kShieldSwBit = 0x2A;
constexpr u8 kShieldSwBit2 = 0x29;
constexpr f32 kShieldItemPosX = 400.0f;
constexpr f32 kShieldItemPosY = 90.0f;
constexpr f32 kShieldItemPosZ = -75.0f;
constexpr s16 kShieldItemAngleY = -0x38E4;
StageActorHandle s_chestHandle = 0;
bool s_chestApplied = false;
StageActorHandle s_shieldHandle = 0;
bool s_shieldApplied = false;

bool apply_shield_item() {
    if (g_configCollectionKeepOrdonShield) {
        return false;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) {
        return false;
    }
    if (s_shieldApplied) return true;
    if (svc_stage == nullptr) {
        return false;
    }

    const stage_actor_data_class item{
        .name = "wshield",
        .base =
            {
                .parameters = (0xFFu << 24) | (kShieldSwBit << 16) |
                              (kShieldSwBit2 << 8),
                .position = {kShieldItemPosX, kShieldItemPosY, kShieldItemPosZ},
                .angle = {0, kShieldItemAngleY, 0},
                .setID = 0xFFFF,
            },
    };

    if (svc_stage->add_actor(mod_ctx, kChestStage, kChestRoom, -1, &item, sizeof(item),
                             &s_shieldHandle) != MOD_OK) {
        return false;
    }
    s_shieldApplied = true;
    return true;
}

void remove_shield_item() {
    if (svc_stage == nullptr || !s_shieldApplied) return;
    svc_stage->remove_actor_edit(mod_ctx, s_shieldHandle);
    s_shieldHandle = 0;
    s_shieldApplied = false;
}

DEFINE_HOOK(&daItemShield_c::create, ShieldCreateHook);

HookAction on_shield_create_pre(ModContext*, void* args, void*, void*) {
    if (g_configCollectionKeepOrdonShield || !args) return HOOK_CONTINUE;
    daItemShield_c* shield = mods::arg<daItemShield_c*>(args, 0);
    if (shield == nullptr) return HOOK_CONTINUE;
    if (shield->getSwBit() != kShieldSwBit) return HOOK_CONTINUE;
    if (fopAcM_GetRoomNo(shield) != kChestRoom) return HOOK_CONTINUE;
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, kChestStage) != 0) return HOOK_CONTINUE;

    if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) {
        fopAcM_onSwitch(shield, kShieldSwBit2);
        return HOOK_CONTINUE;
    }
    fopAcM_onSwitch(shield, kShieldSwBit);
    fopAcM_offSwitch(shield, kShieldSwBit2);
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&daItemShield_c::Create, ShieldCreatePostHook);

void on_shield_create_post(ModContext*, void* args, void*, void*) {
    if (g_configCollectionKeepOrdonShield || !args) return;
    daItemShield_c* shield = mods::arg<daItemShield_c*>(args, 0);
    if (shield == nullptr) return;
    if (shield->getSwBit() != kShieldSwBit) return;
    if (fopAcM_GetRoomNo(shield) != kChestRoom) return;
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || std::strcmp(stage, kChestStage) != 0) return;

    shield->current.pos.set(kShieldItemPosX, kShieldItemPosY, kShieldItemPosZ);
    shield->home.pos = shield->current.pos;
    fopAcM_SetGravity(shield, 0.0f);
    shield->speed.set(0.0f, 0.0f, 0.0f);
    fopAcM_SetSpeedF(shield, 0.0f);
    shield->field_0x936 = 1;
}

u8 s_prevEquip = 0xFF;
int s_revertWatch = 0;
bool s_ownedPrev = false;

void shield_prompt_and_pickup_update() {
    const char* stage = dComIfGp_getStartStageName();
    const bool inRoom = !g_configCollectionKeepOrdonShield &&
                        stage != nullptr && std::strcmp(stage, kChestStage) == 0 &&
                        dComIfGp_roomControl_getStayNo() == kChestRoom;
    if (!inRoom) {
        s_prevEquip = 0xFF;
        s_revertWatch = 0;
        s_ownedPrev = false;
        return;
    }

    const u8 curEquip = dComIfGs_getSelectEquipShield();
    const bool owned = dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e);
    if (owned && !s_ownedPrev && curEquip != dItemNo_WOOD_SHIELD_e) {
        s_revertWatch = 900;
    }
    s_ownedPrev = owned;
    if (s_revertWatch > 0) {
        if (curEquip != dItemNo_WOOD_SHIELD_e) {
            s_prevEquip = curEquip;
        } else {
            dComIfGs_setSelectEquipShield(s_prevEquip);
            dComIfGp_setSelectEquipShield(s_prevEquip);
            s_revertWatch = 0;
        }
        --s_revertWatch;
    } else {
        if (curEquip != dItemNo_WOOD_SHIELD_e) s_prevEquip = curEquip;
    }

    if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) {
        struct shield_hider {
            static void* collect(void* i_proc, void*) {
                if (((base_process_class*)i_proc)->name == fpcNm_Obj_Shield_e) {
                    fopAcM_delete(static_cast<fopAc_ac_c*>(i_proc));
                }
                return nullptr;
            }
        };
        fopAcM_Search(shield_hider::collect, nullptr);
        return;
    }
}

DEFINE_HOOK(&daAlink_c::execute, ShieldAlinkExecuteHook);

void on_shield_alink_execute_post(ModContext*, void*, void*, void*) {
    shield_prompt_and_pickup_update();
}

}

ModResult init_collection_menu_chest(const HookService* hook_svc, const LogService* log_svc,
                                     ModContext* mod_ctx, ModError*) {
    if (hook_svc != nullptr) {
        mods::hook::add_pre<ShieldCreateHook>(hook_svc, on_shield_create_pre);
        mods::hook::add_post<ShieldCreatePostHook>(hook_svc, on_shield_create_post);
        mods::hook::add_post<ShieldAlinkExecuteHook>(hook_svc, on_shield_alink_execute_post);
    }
    if (g_configCollectionStarterEquip) {
        apply_shield_item();
    }
    return MOD_OK;
}

void update_collection_menu_chest(const LogService*, ModContext*) {
    if (g_configCollectionStarterEquip && !g_configCollectionKeepOrdonShield &&
        apply_shield_item()) {
        return;
    }
    remove_shield_item();
}

void shutdown_collection_menu_chest() {
    remove_shield_item();
}
