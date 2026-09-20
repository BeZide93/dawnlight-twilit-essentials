#pragma once

#include "collection_lib/collection_common.hpp"
#include "d/d_msg_flow.h"

// Equipment hooks
DEFINE_HOOK(&dMenu_Collect2D_c::wait_proc, WaitProcHook);
HookAction on_wait_proc_pre(ModContext*, void* args, void*, void*);
void on_wait_proc_post(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::pointerActivateCurrent, PointerActivateCurrentHook);
HookAction on_pointer_activate_current_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::changeSword, ChangeSwordHook);
HookAction on_change_sword_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::changeShield, ChangeShieldHook);
HookAction on_change_shield_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::changeClothe, ChangeClotheHook);
HookAction on_change_clothes_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorSword, SetEquipFrameColorSwordHook);
HookAction on_set_equip_frame_sword_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorShield, SetEquipFrameColorShieldHook);
HookAction on_set_equip_frame_shield_pre(ModContext*, void* args, void*, void*);

DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorClothes, SetEquipFrameColorClothesHook);
HookAction on_set_equip_frame_clothes_pre(ModContext*, void* args, void*, void*);

// Area transition & spawn clothes preservation hooks
DEFINE_HOOK(&daAlink_c::create, DaAlinkCreateHook);
HookAction on_da_alink_create_pre(ModContext*, void*, void*, void*);
void on_da_alink_create_post(ModContext*, void*, void*, void*);

// Link human-model rebuild (create / clothes change / Wolf->Human) - re-applies
// the custom tunic swap in the same call the vanilla models are built.
DEFINE_HOOK(&daAlink_c::changeLink, DaAlinkChangeLinkHook);
HookAction on_da_alink_change_link_pre(ModContext*, void*, void*, void*);
void on_da_alink_change_link_post(ModContext*, void*, void*, void*);

// mpLinkModel becomes the wolf model here - the custom shield/sword must stop
// drawing the instant this runs (not linger on the wolf's back).
DEFINE_HOOK(&daAlink_c::changeWolf, DaAlinkChangeWolfHook);
void on_da_alink_change_wolf_post(ModContext*, void*, void*, void*);

DEFINE_HOOK(&dComIfGs_setSelectEquipClothes, SetSelectEquipClothesHook);
HookAction on_set_select_equip_clothes_pre(ModContext*, void* args, void*, void*);

// Shield preservation hook for keep Ordon Shield
DEFINE_HOOK(&dMeter2Info_setShield, Meter2InfoSetShieldHook);
HookAction on_meter2_info_set_shield_pre(ModContext*, void* args, void*, void*);

// Lets shops sell the (distinct) Wooden Shield even though "keep Ordon Shield"
// makes the game still see you owning the Ordon Shield.
DEFINE_HOOK(&dMsgFlow_c::query022, MsgFlowGetCheckHook);
HookAction on_msg_flow_get_check_pre(ModContext*, void* args, void* retval, void*);