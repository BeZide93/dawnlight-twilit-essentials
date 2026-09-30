#pragma once

#include "z_common.hpp"

struct CheckItemChangeFromButtonHook;
struct CheckItemButtonChangeHook;
struct CheckItemSetButtonHook;
struct CheckSetItemTriggerHook;
struct SetHeavyBootsHook;
struct OrderTalkHook;
struct ZEquipBottleItemInHook;
struct ZEquipBottleItemEmptyHook;

HookAction on_check_item_change_from_button_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_check_item_button_change_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_check_item_set_button_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_check_set_item_trigger_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_set_heavy_boots_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
void on_order_talk_post(ModContext* mod_ctx, void* args, void* ret, void* user_data);
void on_set_stick_data_track_item_button_post(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_equip_bottle_item_in_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
HookAction on_equip_bottle_item_empty_pre(ModContext* mod_ctx, void* args, void* ret, void* user_data);
void check_iron_boots_unequip_on_overwrite();
