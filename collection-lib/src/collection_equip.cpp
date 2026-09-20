#include "collection_equip.hpp"
#include "collection_lib/collection_common.hpp"
#include "collection_layout.hpp"
#include "collection_page.hpp"
#include "collection_lib/custom_equip.hpp"
#include <cmath>

static bool s_inAlinkCreate = false;

HookAction on_wait_proc_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D || !collect2D->mpScreen) return HOOK_CONTINUE;

    // Make sure a kept-but-burned Ordon shield is back in the inventory before
    // the player can select its slot, so the equip takes the normal "already
    // owned" path.

    apply_collect_shifts(collect2D);

    // Second-page R/L toggle. While the grid isn't fully on the main page, freeze
    // all normal collection input (cursor, equip, mouse) - only R/L page through.
    collection_page_handle_input(collect2D);
    if (collection_page_p2_focused()) {
        return HOOK_SKIP_ORIGINAL;
    }

    if (dMw_A_TRIGGER()) {
        u8 curX = collect2D->mCursorX;
        u8 curY = collect2D->mCursorY;

        if (const SlotSpec* slot = slot_at(curX, curY)) {
            if (slot->onEquip) {
                slot->onEquip(collect2D);
                return HOOK_SKIP_ORIGINAL;
            }
        }

        if (curY == 0 && curX >= 3 && curX <= 5) {
            if (daPy_getPlayerActorClass()->getSwordChangeWaitTimer() == 0) {
                collect2D->changeSword();
                return HOOK_SKIP_ORIGINAL;
            }
        } else if (curY == 1 && curX >= 3 && curX <= 5) {
            if (daPy_getPlayerActorClass()->getShieldChangeWaitTimer() == 0) {
                collect2D->changeShield();
                return HOOK_SKIP_ORIGINAL;
            }
        } else if (curY == 2 && curX >= 3 && curX <= 6) {
            if (daPy_getPlayerActorClass()->getClothesChangeWaitTimer() == 0) {
                collect2D->changeClothe();
                return HOOK_SKIP_ORIGINAL;
            }
        }
    }
    return HOOK_CONTINUE;
}



void on_wait_proc_post(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    if (const SlotSpec* slot = slot_at(curX, curY)) {
        if (slot->onEquip) {
            if (collect2D->mIsWolf) {
                collect2D->setAButtonString(0);
            } else if (is_collect_item_equipped(curX, curY)) {
                collect2D->setAButtonString((true && curY != 2) ? 0x437 : 0);  // "Unequip"
            } else {
                collect2D->setAButtonString(0x436);  // "Equip"
            }
            return;
        }
    }

    if (curX >= 3 && curX <= 6 && curY <= 2) {
        if (collect2D->mIsWolf || !is_collect_item_unlocked(curX, curY)) {
            collect2D->setAButtonString(0);
        } else if (is_collect_item_equipped(curX, curY)) {
            if (true && (curY == 0 || curY == 1)) {
                collect2D->setAButtonString(0x437); // "Unequip"
            } else {
                collect2D->setAButtonString(0);
            }
        } else {
            collect2D->setAButtonString(0x436); // "Equip"
        }
    }

    // The game's own waitProc re-shows the vanilla cell panes it considers
    // unlocked - re-apply the blank layout AFTER it, so the hidden state is what
    // actually gets rendered this frame.
    cl_apply_blank_layout(collect2D);
}

HookAction on_pointer_activate_current_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    if (const SlotSpec* slot = slot_at(curX, curY)) {
        if (slot->onEquip) {
            slot->onEquip(collect2D);
            return HOOK_SKIP_ORIGINAL;
        }
    }

    if (curY == 0 && curX >= 3 && curX <= 5) {
        if (daPy_getPlayerActorClass()->getSwordChangeWaitTimer() == 0) {
            collect2D->changeSword();
            return HOOK_SKIP_ORIGINAL;
        }
    } else if (curY == 1 && curX >= 3 && curX <= 5) {
        if (daPy_getPlayerActorClass()->getShieldChangeWaitTimer() == 0) {
            collect2D->changeShield();
            return HOOK_SKIP_ORIGINAL;
        }
    } else if (curY == 2 && curX >= 3 && curX <= 6) {
        if (daPy_getPlayerActorClass()->getClothesChangeWaitTimer() == 0) {
            collect2D->changeClothe();
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

HookAction on_change_sword_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    // Picking a real sword slot drops any custom sword skin. Only when one is
    // actually active though - custom_equip_clear() rebuilds the sword model,
    // and doing that for a plain vanilla pick used to leave Link holding a
    // sword that was sheathed when the Collection screen opened.
    bool wasCustomSword = custom_equip_active(CE_SWORD);
    if (wasCustomSword && curX >= 3 && curX <= 5 && curY == 0) custom_equip_clear(CE_SWORD);

    if (curX == 3) {
        if (is_collect_item_unlocked(3, 0)) {
            if (!wasCustomSword && dComIfGs_getSelectEquipSword() == dItemNo_WOOD_STICK_e) {
                if (true) {
                    dMeter2Info_setSword(dItemNo_NONE_e, false);
                    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    dMeter2Info_set2DVibration();
                    update_frame_highlights(collect2D);
                }
            } else {
                dMeter2Info_setSword(dItemNo_WOOD_STICK_e, false);
                dComIfGs_onItemFirstBit(dItemNo_WOOD_STICK_e);
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            }
        }
    } else if (curX == 4) {
        if (is_collect_item_unlocked(4, 0)) {
            // Relocated native Master Sword pane when the row is item2/3-
            // swapped (cl_item23_swapped) - equip Master/Light Sword instead
            // of Ordon Sword, matching what's actually drawn at this column.
            if (cl_item23_swapped(1)) {
                u8 targetSword = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? dItemNo_LIGHT_SWORD_e : dItemNo_MASTER_SWORD_e;
                bool isMasterEquipped = (dComIfGs_getSelectEquipSword() == targetSword || dComIfGs_getSelectEquipSword() == dItemNo_MASTER_SWORD_e || dComIfGs_getSelectEquipSword() == dItemNo_LIGHT_SWORD_e);
                if (!wasCustomSword && isMasterEquipped) {
                    dMeter2Info_setSword(dItemNo_NONE_e, false);
                    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    dMeter2Info_set2DVibration();
                    update_frame_highlights(collect2D);
                } else {
                    dMeter2Info_setSword(targetSword, false);
                    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    dMeter2Info_set2DVibration();
                    update_frame_highlights(collect2D);
                }
            } else if (!wasCustomSword && dComIfGs_getSelectEquipSword() == dItemNo_SWORD_e) {
                dMeter2Info_setSword(dItemNo_NONE_e, false);
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            } else {
                dMeter2Info_setSword(dItemNo_SWORD_e, false);
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            }
        }
    } else if (curX == 5) {
        if (is_collect_item_unlocked(5, 0)) {
            u8 targetSword = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? dItemNo_LIGHT_SWORD_e : dItemNo_MASTER_SWORD_e;
            bool isMasterEquipped = (dComIfGs_getSelectEquipSword() == targetSword || dComIfGs_getSelectEquipSword() == dItemNo_MASTER_SWORD_e || dComIfGs_getSelectEquipSword() == dItemNo_LIGHT_SWORD_e);
            if (!wasCustomSword && isMasterEquipped) {
                if (true) {
                    dMeter2Info_setSword(dItemNo_NONE_e, false);
                    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    dMeter2Info_set2DVibration();
                    update_frame_highlights(collect2D);
                }
            } else {
                dMeter2Info_setSword(targetSword, false);
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            }
        }
    }

    return HOOK_SKIP_ORIGINAL;
}

HookAction on_change_shield_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    // Picking one of the real shield slots drops any custom shield skin (those
    // slots equip through custom_equip_on_equip, never here). Gated on the shield
    // columns so a spurious changeShield() call can't clear it.
    bool wasCustomShield = custom_equip_active(CE_SHIELD);
    if (curX >= 3 && curX <= 5 && curY == 1) custom_equip_clear(CE_SHIELD);

    static const struct { u8 x; u8 itemNo; } kVanillaShields[] = {
        { 3, dItemNo_WOOD_SHIELD_e },
        { 4, dItemNo_SHIELD_e },
        { 5, dItemNo_HYLIA_SHIELD_e },
    };

    for (const auto& vs : kVanillaShields) {
        if (vs.x == curX && is_collect_item_unlocked(vs.x, 1)) {
            if (!wasCustomShield && dComIfGs_getSelectEquipShield() == vs.itemNo) {
                if (true) {
                    dMeter2Info_setShield(dItemNo_NONE_e, false);
                    daAlink_getAlinkActorClass()->setShieldChange();
                    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    dMeter2Info_set2DVibration();
                    update_frame_highlights(collect2D);
                }
            } else {
                dMeter2Info_setShield(vs.itemNo, false);
                dComIfGs_onItemFirstBit(vs.itemNo);
                daAlink_getAlinkActorClass()->setShieldChange();
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            }
            break;
        }
    }

    return HOOK_SKIP_ORIGINAL;
}

HookAction on_meter2_info_set_shield_pre(ModContext*, void* args, void*, void*) {
    if (!true || !args) return HOOK_CONTINUE;
    u8 itemId = mods::arg<u8>(args, 0);
    bool offItemBit = mods::arg<bool>(args, 1);
    // The wood/Ordon shield only ever burns while it is the equipped shield, so
    // this catches the burn: swallow the "clear the owned bit" part, keep the
    // item, just let it get unequipped like vanilla.
    if (false) {
        dMeter2Info_setShield(itemId, false);
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// Message-flow "Get Check" (query022) = "does the player own item <param>?".
// The Ordon shop gates selling the Wooden Shield (dItemNo_SHIELD_e) on NOT
// owning the Ordon Shield (dItemNo_WOOD_SHIELD_e) - they're treated as the same
// wooden shield. With "keep Ordon Shield" on, the Ordon Shield never leaves the
// player's inventory, so that gate would block the sale forever. Report "not
// owned" for the Ordon Shield specifically, and only while the player does own
// the Ordon Shield but is missing the shop's Wooden Shield - the exact case the
// option is meant to allow.
HookAction on_msg_flow_get_check_pre(ModContext*, void* args, void* retval, void*) {
    if (!true || !args) return HOOK_CONTINUE;
    mesg_flow_node_branch* node = mods::arg<mesg_flow_node_branch*>(args, 1);
    if (!node) return HOOK_CONTINUE;
    u8 prm0 = static_cast<u8>(node->param);
    if (prm0 == dItemNo_WOOD_SHIELD_e &&
        dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e) &&
        !dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) {
        if (retval) *static_cast<u16*>(retval) = 1;   // 1 == "not owned" -> sale proceeds
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

// "Keep Ordon Shield" also means: if the shield was already burned away before
// the option was turned on (or lost through any path the hook above missed),
// hand it back. Safe to call every frame - it only re-grants the owned bit when
// the permanent "once collected" record says the player earned it.


struct VanillaClothEntry {
    u8 x;
    u8 itemNo;
};

static const VanillaClothEntry kVanillaClothes[] = {
    { 3, dItemNo_WEAR_CASUAL_e },
    { 4, dItemNo_WEAR_KOKIRI_e },
    { 5, dItemNo_WEAR_ZORA_e },
    { 6, dItemNo_ARMOR_e },
};

HookAction on_change_clothes_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;
    if (curY != 2) return HOOK_SKIP_ORIGINAL;

    bool wasCustomTunic = custom_equip_active(CE_TUNIC);
    custom_equip_clear(CE_TUNIC);

    for (const auto& vc : kVanillaClothes) {
        if (vc.x == curX && is_collect_item_unlocked(vc.x, 2)) {
            if (wasCustomTunic || dComIfGs_getSelectEquipClothes() != vc.itemNo) {
                dMeter2Info_setCloth(vc.itemNo, false);
                daPy_getPlayerActorClass()->setClothesChange(0);
                Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                dMeter2Info_set2DVibration();
                update_frame_highlights(collect2D);
            }
            break;
        }
    }

    return HOOK_SKIP_ORIGINAL;
}

HookAction on_set_equip_frame_sword_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    update_frame_highlights(collect2D);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_set_equip_frame_shield_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    update_frame_highlights(collect2D);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_set_equip_frame_clothes_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    update_frame_highlights(collect2D);
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_da_alink_create_pre(ModContext*, void*, void*, void*) {
    s_inAlinkCreate = true;
    return HOOK_CONTINUE;
}

void on_da_alink_create_post(ModContext*, void*, void*, void*) {
    s_inAlinkCreate = false;
}

// daAlink_c::changeLink() rebuilds Link's HUMAN body/hat/face/hand models. It runs
// on initial create, on a clothes change, and on the Wolf->Human transform - i.e.
// every moment the custom tunic swap needs to be (re-)applied. Doing it here, in
// the same call that builds the vanilla models, means no plain base model is ever
// drawn (fade-in, un-transform, clothes swap).
// Position save/restore: the changeLink rebuild can reset Link's position
// offset during area transitions. Save before, restore after.
static cXyz s_savedLinkPos;
static s16 s_savedLinkAngleY = 0;

HookAction on_da_alink_change_link_pre(ModContext*, void* args, void*, void*) {
    // Save Link's position BEFORE the changeLink rebuild.
    daAlink_c* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink != nullptr) {
        s_savedLinkPos = alink->current.pos;
        s_savedLinkAngleY = alink->current.angle.y;
    }
    // The custom tunic is grafted onto a specific vanilla clothes model - force
    // that base so changeLink() builds the right skeleton + sub-models.
    if (custom_equip_active(CE_TUNIC) && !s_inAlinkCreate) {
        const CustomEquipDef* td = custom_equip_get(custom_equip_active_id(CE_TUNIC));
        if (td != nullptr && dComIfGs_getSelectEquipClothes() != td->baseClothes) {
            dComIfGs_setSelectEquipClothes(td->baseClothes);
        }
        // changeLink() builds mpLinkModel/Hat/Hand straight off the live
        // checkCasualWearFlg()/checkZoraWearFlg()/... state (always correct,
        // whatever dComIfGs_getSelectEquipClothes() says right now) - but it
        // builds mpLinkFaceModel off the ACTOR'S OWN mArcName field instead
        // (d_a_alink_wolf.inc, changeLink: "al_face.bmd" is fetched from
        // mArcName, not from the same live checks). Vanilla only ever updates
        // mArcName via setArcName(), called from the timer-driven
        // loadModelDVD() clothes-change flow - forcing
        // dComIfGs_setSelectEquipClothes() directly, as this hook and
        // custom_equip_apply's per-frame pin both do, never goes through that
        // flow, so mArcName is left holding whatever the LAST real (timer-
        // driven) clothes change set it to - Casual/Zora/Magic Armor/Kokiri
        // from however the player's save left them, not necessarily this
        // tunic's baseClothes. A changeLink() call here then builds a face
        // model from the WRONG archive - if that archive is a different vanilla
        // one it's merely visually wrong, but if it was never actually mounted
        // (this state machine skipped) dComIfG_getObjectRes() can return
        // stale/malformed data, which is exactly what a "custom tunic +
        // straight into the collection menu on the very first changeLink of
        // the session" repro hits: calcWeightEnvelopeMtx garbage-reads it.
        // Always resync mArcName to match the value just pinned above,
        // regardless of whether this call needed to change it.
        if (td != nullptr) {
            alink->setArcName(false);
        }
    }
    // The old captured base models are about to be freed - forget them so the
    // POST re-captures the fresh ones (otherwise the "unequip -> restore" path
    // could later write a dangling pointer into mpLinkModel).
    custom_equip_before_link_rebuild();
    return HOOK_CONTINUE;
}

// Above this, a PRE->POST jump in current.pos is changeLink()'s own internal
// skeleton/offset reset (what the restore exists to undo) - a few units at
// most. On some stage transitions that DON'T go through a full daAlink_c::create()
// (s_inAlinkCreate stays false), our forced base-clothes fix-up below can trigger
// this same changeLink() call WHILE the room-control code is still mid-placement,
// i.e. before it has written Link's real spawn-point position for the new area.
// Restoring unconditionally in that case overwrites the real spawn position with
// the stale pre-transition one, landing Link "somewhere else" after certain
// transitions with a custom tunic equipped (PRE captured the wrong value to begin
// with - it was already wrong before our restore ever ran). A same-room skeleton
// offset and a cross-room teleport differ by orders of magnitude, so treat any
// large jump as an intentional reposition and leave it alone.
constexpr f32 kMaxSaneRestoreDistSq = 300.0f * 300.0f;

void on_da_alink_change_link_post(ModContext*, void* args, void*, void*) {
    custom_equip_set_link_model_wolf(false);   // mpLinkModel is now a human model

    // Restore Link's position AFTER the model rebuild.
    daAlink_c* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink != nullptr) {
        if (s_savedLinkPos.x != 0.0f || s_savedLinkPos.z != 0.0f) {
            const f32 dx = alink->current.pos.x - s_savedLinkPos.x;
            const f32 dy = alink->current.pos.y - s_savedLinkPos.y;
            const f32 dz = alink->current.pos.z - s_savedLinkPos.z;
            const f32 distSq = dx * dx + dy * dy + dz * dz;
            if (distSq <= kMaxSaneRestoreDistSq) {
                alink->current.pos = s_savedLinkPos;
                alink->current.angle.y = s_savedLinkAngleY;
            }
        }
    }

    if (args) custom_equip_on_alink_created(mods::arg<daAlink_c*>(args, 0));
}

void on_da_alink_change_wolf_post(ModContext*, void*, void*, void*) {
    custom_equip_set_link_model_wolf(true);    // mpLinkModel is now the wolf model
}

HookAction on_set_select_equip_clothes_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    u8 newCloth = mods::arg<u8>(args, 0);
    if (s_inAlinkCreate && newCloth == dItemNo_WEAR_KOKIRI_e && dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}
