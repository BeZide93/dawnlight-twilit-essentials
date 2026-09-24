#include "collection_internal.hpp"

#include "d/d_msg_flow.h"

// Equipping from the Collection screen.
//
// Native cells still go through the game's changeSword/changeShield/changeClothe(); the
// library only steps in when a custom item of that kind is worn (the native code would see
// the vanilla item underneath and do nothing). Custom cells are handled completely here.

DEFINE_HOOK(&dMenu_Collect2D_c::wait_proc, WaitProcHook);
DEFINE_HOOK(&dMenu_Collect2D_c::pointerActivateCurrent, PointerActivateCurrentHook);
DEFINE_HOOK(&dMenu_Collect2D_c::changeSword, ChangeSwordHook);
DEFINE_HOOK(&dMenu_Collect2D_c::changeShield, ChangeShieldHook);
DEFINE_HOOK(&dMenu_Collect2D_c::changeClothe, ChangeClotheHook);

DEFINE_HOOK(&daAlink_c::create, DaAlinkCreateHook);
DEFINE_HOOK(&daAlink_c::changeLink, DaAlinkChangeLinkHook);
DEFINE_HOOK(&daAlink_c::changeWolf, DaAlinkChangeWolfHook);
DEFINE_HOOK(&dComIfGs_setSelectEquipClothes, SetSelectEquipClothesHook);
DEFINE_HOOK(&dMsgFlow_c::query022, MsgFlowGetCheckHook);

namespace {

bool change_timer_free(CustomEquipKind kind) {
    daPy_py_c* pl = daPy_getPlayerActorClass();
    if (pl == nullptr) return false;
    switch (kind) {
    case CE_SWORD:  return pl->getSwordChangeWaitTimer() == 0;
    case CE_SHIELD: return pl->getShieldChangeWaitTimer() == 0;
    default:        return pl->getClothesChangeWaitTimer() == 0;
    }
}

u8 current_equip(int r) {
    if (r == 0) return dComIfGs_getSelectEquipSword();
    if (r == 1) return dComIfGs_getSelectEquipShield();
    return dComIfGs_getSelectEquipClothes();
}

// What the native change*() equips for a native cell (d_menu_collect.cpp).
u8 native_target(int r, u8 x) {
    if (r == 0) {
        if (x == 3) return dComIfGs_isItemFirstBit(dItemNo_SWORD_e) ? dItemNo_SWORD_e : dItemNo_WOOD_STICK_e;
        if (x == 4) return dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? dItemNo_LIGHT_SWORD_e : dItemNo_MASTER_SWORD_e;
    } else if (r == 1) {
        if (x == 3) {
            if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) return dItemNo_SHIELD_e;
            if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) return dItemNo_WOOD_SHIELD_e;
        }
        if (x == 4) return dItemNo_HYLIA_SHIELD_e;
    } else {
        if (x == 3) return dItemNo_WEAR_KOKIRI_e;
        if (x == 4) return dItemNo_WEAR_ZORA_e;
        if (x == 5) return dItemNo_ARMOR_e;
    }
    return dItemNo_NONE_e;
}

void equip_feedback(bool equipped) {
    Z2GetAudioMgr()->seStart(equipped ? Z2SE_SY_ITEM_SET_X : Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0,
                             1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

void set_vanilla(int r, u8 item) {
    if (r == 0) {
        dMeter2Info_setSword(item, false);
    } else if (r == 1) {
        dMeter2Info_setShield(item, false);
        if (daAlink_c* link = daAlink_getAlinkActorClass()) link->setShieldChange();
    } else {
        dMeter2Info_setCloth(item, false);
        if (daPy_py_c* pl = daPy_getPlayerActorClass()) pl->setClothesChange(0);
    }
}

int custom_under_cursor(dMenu_Collect2D_c* c) {
    if (c->mCursorY >= kClRows) return -1;
    return layout_custom_at_cell(c->mCursorY, c->mCursorX);
}

HookAction handle_change(void* args, int r) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || c->mCursorY != r) return HOOK_CONTINUE;

    const int id = custom_under_cursor(c);
    if (id >= 0) {
        equip_activate_custom(c, id);
        return HOOK_SKIP_ORIGINAL;
    }

    const u8 x = c->mCursorX;
    const CustomEquipKind kind = row_kind(r);
    const u8 target = native_target(r, x);

    if (!custom_equip_active(kind)) {
        if (r != 2 && target != dItemNo_NONE_e && current_equip(r) == target && cl_unequip_enabled()) {
            set_vanilla(r, dItemNo_NONE_e);
            equip_feedback(false);
            screen_refresh_frames(c);
            return HOOK_SKIP_ORIGINAL;
        }
        return HOOK_CONTINUE;
    }

    custom_equip_clear(kind);
    if (target != dItemNo_NONE_e && current_equip(r) != target) {
        return HOOK_CONTINUE;   // the native code equips it (sound, frame) as usual
    }
    // The vanilla item was already worn underneath: the native code would do nothing.
    if (target != dItemNo_NONE_e) {
        set_vanilla(r, target);
        equip_feedback(true);
    }
    screen_refresh_frames(c);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_change_sword_pre(ModContext*, void* args, void*, void*) { return handle_change(args, 0); }
HookAction on_change_shield_pre(ModContext*, void* args, void*, void*) { return handle_change(args, 1); }
HookAction on_change_clothes_pre(ModContext*, void* args, void*, void*) { return handle_change(args, 2); }

HookAction on_wait_proc_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c)) return HOOK_CONTINUE;

    collection_page_handle_input(c);
    if (collection_page_p2_focused()) {
        c->setAButtonString(0);
        return HOOK_SKIP_ORIGINAL;
    }

    // The native A handling only knows the native cells.
    if (dMw_A_TRIGGER()) {
        const int id = custom_under_cursor(c);
        if (id >= 0) {
            equip_activate_custom(c, id);
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

void on_wait_proc_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || collection_page_p2_focused() || c->mCursorY >= kClRows) return;

    const int r = c->mCursorY;
    const u8 x = c->mCursorX;
    const int id = custom_under_cursor(c);
    if (id >= 0) {
        u16 str = 0;
        if (!c->mIsWolf && custom_equip_unlocked(id)) {
            str = (r != 2 && cl_unequip_enabled() && custom_equip_equipped(id)) ? kClUnequipMsg : kClEquipMsg;
        }
        c->setAButtonString(str);
        return;
    }
    if (r != 2 && cl_unequip_enabled() && !c->mIsWolf && layout_col_of_cell(r, x) != 0 &&
        c->field_0x22d[x][r] != 0 && native_cell_equipped(r, x)) {
        c->setAButtonString(kClUnequipMsg);
    }
}

HookAction on_pointer_activate_current_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c)) return HOOK_CONTINUE;
    const int id = custom_under_cursor(c);
    if (id < 0) return HOOK_CONTINUE;
    equip_activate_custom(c, id);
    return HOOK_SKIP_ORIGINAL;
}

// ---------------------------------------------------------------------------
// Link
// ---------------------------------------------------------------------------

bool s_inAlinkCreate = false;
cXyz s_savedLinkPos;
s16 s_savedLinkAngleY = 0;
constexpr f32 kMaxSaneRestoreDistSq = 300.0f * 300.0f;

HookAction on_da_alink_create_pre(ModContext*, void*, void*, void*) {
    s_inAlinkCreate = true;
    return HOOK_CONTINUE;
}

void on_da_alink_create_post(ModContext*, void*, void*, void*) {
    s_inAlinkCreate = false;
}

HookAction on_da_alink_change_link_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* alink = args != nullptr ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    if (alink != nullptr) {
        s_savedLinkPos = alink->current.pos;
        s_savedLinkAngleY = alink->current.angle.y;
    }
    if (custom_equip_active(CE_TUNIC) && !s_inAlinkCreate) {
        const CustomEquipDef* td = custom_equip_get(custom_equip_active_id(CE_TUNIC));
        const u8 base = td != nullptr ? custom_equip_resolved_base(*td) : dItemNo_NONE_e;
        if (base != dItemNo_NONE_e && dComIfGs_getSelectEquipClothes() != base) {
            dComIfGs_setSelectEquipClothes(base);
        }
    }
    custom_equip_before_link_rebuild();
    return HOOK_CONTINUE;
}

void on_da_alink_change_link_post(ModContext*, void* args, void*, void*) {
    custom_equip_set_link_model_wolf(false);

    daAlink_c* alink = args != nullptr ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    if (alink == nullptr) return;
    if (s_savedLinkPos.x != 0.0f || s_savedLinkPos.z != 0.0f) {
        const f32 dx = alink->current.pos.x - s_savedLinkPos.x;
        const f32 dy = alink->current.pos.y - s_savedLinkPos.y;
        const f32 dz = alink->current.pos.z - s_savedLinkPos.z;
        if (dx * dx + dy * dy + dz * dz <= kMaxSaneRestoreDistSq) {
            alink->current.pos = s_savedLinkPos;
            alink->current.angle.y = s_savedLinkAngleY;
        }
    }
    custom_equip_on_alink_created(alink);
}

void on_da_alink_change_wolf_post(ModContext*, void*, void*, void*) {
    custom_equip_set_link_model_wolf(true);
}

// Link's create switches Ordon Clothes to the Hero's Clothes once they are owned. When a
// slot lets the player wear the Ordon Clothes again, keep them.
HookAction on_set_select_equip_clothes_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr || !s_inAlinkCreate) return HOOK_CONTINUE;
    const u8 newCloth = mods::arg<u8>(args, 0);
    if (newCloth == dItemNo_WEAR_KOKIRI_e && dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e &&
        layout_uses_base_item(dItemNo_WEAR_CASUAL_e)) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction on_msg_flow_get_check_pre(ModContext*, void* args, void* retval, void*) {
    if (args == nullptr || !cl_keep_ordon_shield_enabled()) return HOOK_CONTINUE;
    mesg_flow_node_branch* node = mods::arg<mesg_flow_node_branch*>(args, 1);
    if (node == nullptr) return HOOK_CONTINUE;
    const u8 item = static_cast<u8>(node->param);
    if (item == dItemNo_WOOD_SHIELD_e && dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) &&
        !dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) {
        if (retval != nullptr) *static_cast<u16*>(retval) = 1;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

}  // namespace

// Native "equipped" rule of a native cell (setEquipItemFrameColor*). A Wooden Sword or Ordon
// Shield with a column of its own is no longer part of the cell that shows it natively.
bool native_cell_equipped(int r, u8 x) {
    if (custom_equip_active(row_kind(r))) return false;
    const u8 cur = current_equip(r);
    if (r == 0) {
        if (x == 3) {
            return cur == dItemNo_SWORD_e ||
                   (cur == dItemNo_WOOD_STICK_e && !layout_has_stand_in(dItemNo_WOOD_STICK_e));
        }
        if (x == 4) return cur == dItemNo_MASTER_SWORD_e || cur == dItemNo_LIGHT_SWORD_e;
    } else if (r == 1) {
        if (x == 3) {
            return cur == dItemNo_SHIELD_e ||
                   (cur == dItemNo_WOOD_SHIELD_e && !layout_has_stand_in(dItemNo_WOOD_SHIELD_e));
        }
        if (x == 4) return cur == dItemNo_HYLIA_SHIELD_e;
    } else {
        return cur == native_target(r, x);
    }
    return false;
}

void equip_activate_custom(dMenu_Collect2D_c* c, int id) {
    if (c == nullptr || c->mIsWolf) return;
    const CustomEquipDef* d = custom_equip_get(id);
    if (d == nullptr || !custom_equip_unlocked(id) || !change_timer_free(d->kind)) return;
    if (custom_equip_toggle(id)) screen_refresh_frames(c);
}

void equip_install_hooks(const HookService* hook_svc) {
    CL_HOOK_PRE(WaitProcHook, on_wait_proc_pre);
    CL_HOOK_POST(WaitProcHook, on_wait_proc_post);
    CL_HOOK_PRE(PointerActivateCurrentHook, on_pointer_activate_current_pre);
    CL_HOOK_PRE(ChangeSwordHook, on_change_sword_pre);
    CL_HOOK_PRE(ChangeShieldHook, on_change_shield_pre);
    CL_HOOK_PRE(ChangeClotheHook, on_change_clothes_pre);

    CL_HOOK_PRE(DaAlinkCreateHook, on_da_alink_create_pre);
    CL_HOOK_POST(DaAlinkCreateHook, on_da_alink_create_post);
    CL_HOOK_PRE(DaAlinkChangeLinkHook, on_da_alink_change_link_pre);
    CL_HOOK_POST(DaAlinkChangeLinkHook, on_da_alink_change_link_post);
    CL_HOOK_POST(DaAlinkChangeWolfHook, on_da_alink_change_wolf_post);
    CL_HOOK_PRE(SetSelectEquipClothesHook, on_set_select_equip_clothes_pre);
    CL_HOOK_PRE(MsgFlowGetCheckHook, on_msg_flow_get_check_pre);
}
