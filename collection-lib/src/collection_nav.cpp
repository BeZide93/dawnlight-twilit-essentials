#include "collection_nav.hpp"
#include "collection_page.hpp"

#include "f_pc/f_pc_profile_lst.h"

HookAction on_get_item_tag_pre(ModContext*, void* args, void* ret, void*) {
    if (!is_collection_menu_enabled() || !args || !ret) return HOOK_CONTINUE;
    int i_tag1 = mods::arg<int>(args, 1);
    int i_tag2 = mods::arg<int>(args, 2);
    bool param_3 = mods::arg<bool>(args, 3);

    if (i_tag2 == 5 && !param_3) {
        *(u64*)ret = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (const SlotSpec* slot = slot_at(i_tag1, i_tag2)) {
        if (!is_collect_item_unlocked(i_tag1, i_tag2) || collection_page_active()) {
            *(u64*)ret = 0;
            return HOOK_SKIP_ORIGINAL;
        }
        *(u64*)ret = slot->iconTag;
        return HOOK_SKIP_ORIGINAL;
    }

    if (i_tag1 == 3) {
        if ((i_tag2 == 0 || i_tag2 == 2) && !(cl_column_claimed(1, 1) || cl_column_claimed(3, 1))) {
            *(u64*)ret = 0;
            return HOOK_SKIP_ORIGINAL;
        }
        if (i_tag2 == 1 && !ordon_shield_slot_present()) {
            *(u64*)ret = 0;
            return HOOK_SKIP_ORIGINAL;
        }
    }

    if (i_tag2 < 3 && i_tag1 < 3) {
        *(u64*)ret = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    if (i_tag2 == 0) {
        if (i_tag1 == 3) { *(u64*)ret = MULTI_CHAR('ken_n0'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 5) { *(u64*)ret = MULTI_CHAR('ken_n1'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 6) { *(u64*)ret = MULTI_CHAR('heart_n'); return HOOK_SKIP_ORIGINAL; }
    } else if (i_tag2 == 1) {
        if (i_tag1 == 3) { *(u64*)ret = MULTI_CHAR('tate_n0'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 5) { *(u64*)ret = MULTI_CHAR('tate_n1'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 6) { *(u64*)ret = 0; return HOOK_SKIP_ORIGINAL; }
    } else if (i_tag2 == 2) {
        if (i_tag1 == 4) { *(u64*)ret = MULTI_CHAR('fuku_n0'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 5) { *(u64*)ret = MULTI_CHAR('fuku_n1'); return HOOK_SKIP_ORIGINAL; }
        if (i_tag1 == 6) { *(u64*)ret = MULTI_CHAR('fuku_n2'); return HOOK_SKIP_ORIGINAL; }
    }
    return HOOK_CONTINUE;
}

J2DPane* get_target_pane(dMenu_Collect2D_c* collect2D, u8 x, u8 y) {
    if (!collect2D || !collect2D->mpScreen) return nullptr;
    if (const SlotSpec* s = slot_at(x, y)) {
        if (s->autoLayout.on && s->icon) return s->icon;
    }
    if (y == 0) {
        if (x == 3) return collect2D->mpScreen->search(MULTI_CHAR('ken_n0'));
        if (x == 4) {
            if (cl_item23_swapped(1)) return collect2D->mpScreen->search(MULTI_CHAR('ken_n1'));
            return slot_icon(4, 0);
        }
        if (x == 5) return collect2D->mpScreen->search(MULTI_CHAR('ken_n1'));
        if (x == 6) return collect2D->mpScreen->search(MULTI_CHAR('heart_n'));
    } else if (y == 1) {
        if (x == 3) return collect2D->mpScreen->search(MULTI_CHAR('tate_n0'));
        if (x == 4) return slot_icon(4, 1);
        if (x == 5) return collect2D->mpScreen->search(MULTI_CHAR('tate_n1'));
    } else if (y == 2) {
        if (x == 3) {
            J2DPane* p = slot_icon(3, 2);
            if (p) return p;
            return collect2D->mpScreen->search(MULTI_CHAR('fuku_ord'));
        }
        if (x == 4) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n0'));
        if (x == 5) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n1'));
        if (x == 6) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n2'));
    } else if (y == 3) {
        if (x == 0) return collect2D->mpScreen->search(MULTI_CHAR('item_1_n'));
        if (x == 1) return collect2D->mpScreen->search(MULTI_CHAR('item_0_n'));
        if (x == 2) return collect2D->mpScreen->search(MULTI_CHAR('kabu_6n'));
        if (x == 3) return collect2D->mpScreen->search(MULTI_CHAR('maki_5_n'));
    } else if (y == 4) {
        if (x == 0) return collect2D->mpScreen->search(MULTI_CHAR('wolf_n'));
        if (x == 1) return collect2D->mpScreen->search(MULTI_CHAR('item_2_n'));
        if (x == 2) return collect2D->mpScreen->search(MULTI_CHAR('fish_3_n'));
        if (x == 3) return collect2D->mpScreen->search(MULTI_CHAR('lett_4_n'));
    } else if (y == 5) {
        if (x == 0) return collect2D->mpScreen->search(MULTI_CHAR('save_n'));
        if (x == 1) return collect2D->mpScreen->search(MULTI_CHAR('option_n'));
    }
    if (x < 7 && y < 6 && collect2D->mpSelPm[x][y]) {
        return collect2D->mpSelPm[x][y]->getPanePtr();
    }
    return nullptr;
}

HookAction on_cursor_pos_set_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D || !collect2D->mpScreen || !collect2D->mpDrawCursor) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    for (u8 y = 0; y < 6; y++) {
        for (u8 x = 0; x < 7; x++) {
            J2DPane* pane = get_target_pane(collect2D, x, y);
            if (pane) {

                bool skipScale = (x == 0 && y == 0) ||
                                 (x == 6 && y == 0 && !slot_at(6, 0) && collection_page_claims_cell(6, 0));
                if (!skipScale) {
                    if (y == 5) {
                        if (x == curX && y == curY) {
                            pane->scale(g_drawHIO.mCollectScreen.mSelectSaveOptionScale,
                                       g_drawHIO.mCollectScreen.mSelectSaveOptionScale);
                        } else {
                            pane->scale(g_drawHIO.mCollectScreen.mUnselectSaveOptionScale,
                                       g_drawHIO.mCollectScreen.mUnselectSaveOptionScale);
                        }
                    } else if (x == curX && y == curY) {
                        pane->scale(g_drawHIO.mCollectScreen.mSelectItemScale,
                                   g_drawHIO.mCollectScreen.mSelectItemScale);
                    } else {
                        pane->scale(g_drawHIO.mCollectScreen.mUnselectItemScale,
                                   g_drawHIO.mCollectScreen.mUnselectItemScale);
                    }
                }
            }
        }
    }

    collect2D->mpDrawCursor->setAlphaRate(1.0f);

    J2DPane* curPane = get_target_pane(collect2D, curX, curY);
    if (curPane) {
        Vec pos;
        if (curX < 7 && curY < 6 && collect2D->mpSelPm[curX][curY]) {
            pos = collect2D->mpSelPm[curX][curY]->getGlobalVtxCenter(false, 0);
        } else {
            CPaneMgr tempPm;
            tempPm.initiate(curPane, nullptr);
            pos = tempPm.getGlobalVtxCenter(false, 0);
        }
        collect2D->mpDrawCursor->setPos(pos.x, pos.y, curPane, false);
    }

    if (curY == 5) {
        collect2D->mpDrawCursor->setParam(1.1f, 0.85f, 0.05f, 0.5f, 0.5f);
    } else if (curX == 6 && curY == 0 && !slot_at(6, 0) && collection_page_claims_cell(6, 0)) {
        collect2D->mpDrawCursor->setParam(0.6f, 0.85f, 0.03f, 0.6f, 0.6f);
    } else {
        collect2D->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.7f, 0.7f);
    }

    return HOOK_SKIP_ORIGINAL;
}

HookAction on_cursor_move_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D || !collect2D->mpScreen || !collect2D->mpStick) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;

    const SlotSpec* curSlot = slot_at(curX, curY);

    bool hijackNav = cl_vanilla_layout_hidden() || curSlot != nullptr || slot_in_row(curY) != nullptr;
    bool inEquipGrid = !collect2D->mIsWolf && hijackNav &&
                       ((curX >= 3 && curX <= 6 && curY <= 2) || (curX == 6 && curY == 0) ||
                        (curSlot != nullptr && curSlot->autoLayout.on));
    if (inEquipGrid) {
        collect2D->mpStick->checkTrigger();

        u8 targetX = curX;
        u8 targetY = curY;
        bool moved = false;

        int dir = -1;
        if (collect2D->mpStick->checkRightTrigger()) {
            dir = 1;
        } else if (collect2D->mpStick->checkLeftTrigger()) {
            dir = 0;
        } else {
            int v = collect2D->mpStick->checkDownTrigger()
                        ? 1
                        : (collect2D->mpStick->checkUpTrigger() ? -1 : 0);
            if (v == 1) dir = 3;
            else if (v == -1) dir = 2;
        }

        SlotCell nav = (dir >= 0) ? slot_nav_target(curX, curY, dir) : SlotCell{};

        if (dir < 0) {

        } else if (slot_cell_set(nav)) {
            targetX = nav.x;
            targetY = nav.y;
            moved = true;
        } else if (dir == 1) {
            for (int tx = curX + 1; tx <= 6; tx++) {
                if (collect2D->field_0x22d[tx][curY] != 0) {
                    targetX = tx;
                    moved = true;
                    break;
                }
            }
        } else if (dir == 0) {
            for (int tx = curX - 1; tx >= 3; tx--) {
                if (collect2D->field_0x22d[tx][curY] != 0) {
                    targetX = tx;
                    moved = true;
                    break;
                }
            }
            if (!moved && !cl_vanilla_layout_hidden()) {

                for (int ty = (curY == 0) ? 3 : 4; ty <= 4 && !moved; ty++) {
                    if (collect2D->getItemTag(2, ty, true)) {
                        targetX = 2;
                        targetY = (u8)ty;
                        moved = true;
                    }
                }
            }
        } else {

            const bool goingDown = (dir == 3);
            J2DPane* fromPane = get_target_pane(collect2D, curX, curY);
            f32 fromX = fromPane ? fromPane->getTranslateX() : 0.0f;

            int tyStart = goingDown ? (int)curY + 1 : (int)curY - 1;
            int tyEnd   = goingDown ? 2 : 0;
            int tyStep  = goingDown ? 1 : -1;

            for (int ty = tyStart; goingDown ? (ty <= tyEnd) : (ty >= tyEnd); ty += tyStep) {
                int bestX = -1;
                f32 bestDist = 1.0e9f;
                for (int tx = 3; tx <= 6; tx++) {
                    if (collect2D->field_0x22d[tx][ty] != 0) {
                        J2DPane* p = get_target_pane(collect2D, tx, ty);
                        f32 px = p ? p->getTranslateX() : (f32)tx;
                        f32 d = px - fromX;
                        if (d < 0.0f) d = -d;
                        if (d < bestDist) {
                            bestDist = d;
                            bestX = tx;
                        }
                    }
                }
                if (bestX != -1) {
                    targetX = (u8)bestX;
                    targetY = (u8)ty;
                    moved = true;
                    break;
                }
            }
            if (!moved && goingDown && !cl_vanilla_layout_hidden()) {

                static const u8 kDownX[8] = {3, 2, 3, 1, 2, 0, 1, 0};
                static const u8 kDownY[8] = {3, 3, 4, 3, 4, 3, 4, 4};
                for (int i = 0; i < 8 && !moved; i++) {
                    if (collect2D->getItemTag(kDownX[i], kDownY[i], true)) {
                        targetX = kDownX[i];
                        targetY = kDownY[i];
                        moved = true;
                    }
                }
                if (!moved) {
                    targetY = 5;
                    targetX = (curX <= 2) ? 0 : 1;
                    moved = true;
                }
            }
        }

        if (moved) {
            collect2D->field_0x259 = curX;
            collect2D->field_0x25a = curY;
            collect2D->mCursorX = targetX;
            collect2D->mCursorY = targetY;
            if (targetY == 5) {
                Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OPTION, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            } else {
                Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CURSOR_COMMON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
            collect2D->cursorPosSet();
            collect2D->setItemNameString(collect2D->mCursorX, collect2D->mCursorY);
            return HOOK_SKIP_ORIGINAL;
        }
        if (dir >= 0) {

            return HOOK_SKIP_ORIGINAL;
        }
    } else if (curY == 3 && collect2D->mpStick->checkUpTrigger()) {

        int bestX = -1, bestY = -1;
        J2DPane* fromPane = get_target_pane(collect2D, curX, curY);
        f32 fromX = fromPane ? fromPane->getTranslateX() : 0.0f;
        for (int ty = 2; ty >= 0 && bestX == -1; ty--) {
            f32 bestDist = 1.0e9f;
            for (int tx = 3; tx <= 6; tx++) {
                if (collect2D->field_0x22d[tx][ty] != 0 && is_collect_item_unlocked(tx, ty)) {
                    J2DPane* p = get_target_pane(collect2D, tx, ty);
                    f32 px = p ? p->getTranslateX() : (f32)tx;
                    f32 d = px - fromX;
                    if (d < 0.0f) d = -d;
                    if (d < bestDist) {
                        bestDist = d;
                        bestX = tx;
                        bestY = ty;
                    }
                }
            }
        }
        if (bestX != -1) {
            collect2D->mCursorX = (u8)bestX;
            collect2D->mCursorY = (u8)bestY;
            Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CURSOR_COMMON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            collect2D->cursorPosSet();
            collect2D->setItemNameString(collect2D->mCursorX, collect2D->mCursorY);
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static JGeometry::TBox2<f32> s_heartBoundsSave;
static bool s_heartBoundsSaved = false;
static JGeometry::TBox2<f32> s_kamenBoundsSave;
static bool s_kamenBoundsSaved = false;

struct SlotBoundsSave {
    J2DPane* pane;
    JGeometry::TBox2<f32> bounds;
};
static SlotBoundsSave s_modSlotBoundsSave[12];
static int s_modSlotBoundsSaveCount = 0;

static J2DPane* pw_heart(void* args) {
    dMenu_Collect2D_c* c = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    return (c && c->mpScreen) ? c->mpScreen->search(MULTI_CHAR('heart_n')) : nullptr;
}

static J2DPane* pw_kamen(void* args) {
    dMenu_Collect2D_c* c = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    return (c && c->mpScreen) ? c->mpScreen->search(MULTI_CHAR('kamen_n')) : nullptr;
}

static J2DPane* pw_modelbgn(void* args) {
    dMenu_Collect2D_c* c = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    return (c && c->mpScreen) ? c->mpScreen->search(MULTI_CHAR('modelbgn')) : nullptr;
}

static JGeometry::TBox2<f32> s_modelbgnBoundsSave;
static bool s_modelbgnBoundsSaved = false;

HookAction on_pointer_wait_pre(ModContext*, void* args, void*, void*) {
    s_heartBoundsSaved = false;
    s_kamenBoundsSaved = false;
    s_modelbgnBoundsSaved = false;
    s_modSlotBoundsSaveCount = 0;
    if (!is_collection_menu_enabled()) return HOOK_CONTINUE;

    dMenu_Collect2D_c* collect2D = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    if (collect2D) {
        bool isP2 = collection_page_active();

        for (int i = 0; i < slot_count(); i++) {
            const SlotSpec* s = slot_get(i);
            if (!s || !s->autoLayout.on || !s->icon) continue;

            if (s_modSlotBoundsSaveCount < 12) {
                s_modSlotBoundsSave[s_modSlotBoundsSaveCount++] = { s->icon, s->icon->mBounds };
            }

            if (!isP2 && is_collect_item_unlocked(s->x, s->y)) {
                s->icon->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
            } else {
                s->icon->mBounds.set(-99999.0f, -99999.0f, -99990.0f, -99990.0f);
            }
        }
    }

    if (J2DPane* heart = pw_heart(args)) {
        s_heartBoundsSave = heart->mBounds;
        s_heartBoundsSaved = true;

        if (collection_page_claims_cell(6, 0)) {
            if (!collection_page_active()) {
                heart->mBounds.set(-99999.0f, -99999.0f, -99990.0f, -99990.0f);
            } else {
                heart->mBounds.set(-24.0f, -28.0f, 24.0f, 28.0f);
            }
        }
    }

    if (J2DPane* kamen = pw_kamen(args)) {
        s_kamenBoundsSave = kamen->mBounds;
        s_kamenBoundsSaved = true;
        if (!collection_page_active()) {
            kamen->mBounds.set(-99999.0f, -99999.0f, -99990.0f, -99990.0f);
        }
    }

    if (J2DPane* modelbgn = pw_modelbgn(args)) {
        s_modelbgnBoundsSave = modelbgn->mBounds;
        s_modelbgnBoundsSaved = true;
        if (!collection_page_active()) {
            modelbgn->mBounds.set(-99999.0f, -99999.0f, -99990.0f, -99990.0f);
        }
    }

    return HOOK_CONTINUE;
}

void on_pointer_wait_post(ModContext*, void* args, void*, void*) {
    for (int i = 0; i < s_modSlotBoundsSaveCount; i++) {
        if (s_modSlotBoundsSave[i].pane) {
            s_modSlotBoundsSave[i].pane->mBounds = s_modSlotBoundsSave[i].bounds;
        }
    }
    s_modSlotBoundsSaveCount = 0;

    if (s_heartBoundsSaved) {
        if (J2DPane* heart = pw_heart(args)) {
            heart->mBounds = s_heartBoundsSave;
        }
        s_heartBoundsSaved = false;
    }

    if (s_kamenBoundsSaved) {
        if (J2DPane* kamen = pw_kamen(args)) {
            kamen->mBounds = s_kamenBoundsSave;
        }
        s_kamenBoundsSaved = false;
    }

    if (s_modelbgnBoundsSaved) {
        if (J2DPane* modelbgn = pw_modelbgn(args)) {
            modelbgn->mBounds = s_modelbgnBoundsSave;
        }
        s_modelbgnBoundsSaved = false;
    }
}

bool (*g_hitPaneFn)(CPaneMgr*, f32) = nullptr;
const char* const kHitPaneMangledName = "?hit_pane@menu_pointer@dusk@@YA_NPEAVCPaneMgr@@M@Z";

void (*g_setHoverTargetFn)(u16) = nullptr;
const char* const kSetHoverTargetMangledName = "?set_hover_target@menu_pointer@dusk@@YAXG@Z";

bool (*g_peekClickFn)() = nullptr;
const char* const kPeekClickMangledName = "?peek_click@menu_pointer@dusk@@YA_NXZ";

void on_pointer_wait_replace(ModContext*, void* args, void* retval, void*) {
    dMenu_Collect2D_c* self = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    bool result = false;
    if (self) {
        if (PointerWaitHook::g_orig) {
            result = PointerWaitHook::g_orig(self);
        }

        if (!result && g_hitPaneFn) {
            auto activate = [&](u8 x, u8 y) {

                if (g_setHoverTargetFn) {
                    g_setHoverTargetFn(static_cast<u16>(x + y * 7));
                }
                if (self->mCursorX != x || self->mCursorY != y) {
                    Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CURSOR_COMMON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    self->mCursorX = x;
                    self->mCursorY = y;
                    self->cursorPosSet();
                    self->setItemNameString(self->mCursorX, self->mCursorY);
                }

                static bool s_wasClicked = false;
                bool isClicked = g_peekClickFn && g_peekClickFn();
                if (isClicked && !s_wasClicked) {
                    self->pointerActivateCurrent();
                    result = true;
                }
                s_wasClicked = isClicked;
            };

            bool handled = false;
            for (int i = 0; i < slot_count() && !handled; i++) {
                const SlotSpec* s = slot_get(i);
                if (!s || !s->autoLayout.on || s->x >= 7 || s->y >= 6) continue;
                if (!is_collect_item_unlocked(s->x, s->y)) continue;
                CPaneMgr* pm = self->mpSelPm[s->x][s->y];
                if (!pm || !g_hitPaneFn(pm, 8.0f)) continue;
                activate(s->x, s->y);
                handled = true;
            }

            if (!handled) {
                static const struct { u8 x, y; } kVanillaFallback[] = { {5, 1}, {6, 2} };
                for (const auto& c : kVanillaFallback) {
                    CPaneMgr* pm = self->mpSelPm[c.x][c.y];
                    if (!pm || !g_hitPaneFn(pm, 8.0f)) continue;
                    activate(c.x, c.y);
                    break;
                }
            }
        }
    }
    if (retval) *(bool*)retval = result;
}

HookAction on_set_item_name_string_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    u8 x = mods::arg<u8>(args, 1);
    u8 y = mods::arg<u8>(args, 2);
    if (!collect2D || !collect2D->mpScreen) return HOOK_CONTINUE;

    if (collection_page_on_page()) {
        collect2D->setItemNameStringNull();
        return HOOK_SKIP_ORIGINAL;
    }

    const SlotSpec* slot = slot_at(x, y);
    if ((x >= 3 && x <= 6 && y <= 2) || (slot != nullptr && slot->autoLayout.on)) {
        if (!is_collect_item_unlocked(x, y)) {
            collect2D->setItemNameStringNull();
            return HOOK_SKIP_ORIGINAL;
        }

        if (slot) {
            collect2D->field_0x180 = slot_name_id(slot);
            collect2D->mItemNameString = slot_desc_id(slot);
        } else if (y == 0) {
            if (x == 3) {
                collect2D->field_0x180 = 0x1a4;
                collect2D->mItemNameString = 0x2a4;
            } else if (x == 4 && cl_item23_swapped(1)) {

                collect2D->field_0x180 = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? 0x1ae : 0x18e;
                collect2D->mItemNameString = collect2D->field_0x180 + 0x100;
            } else if (x == 5) {
                collect2D->field_0x180 = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? 0x1ae : 0x18e;
                collect2D->mItemNameString = collect2D->field_0x180 + 0x100;
            } else if (x == 6) {
                collect2D->field_0x180 = 0x186;
                collect2D->mItemNameString = 0x286;
            }
        } else if (y == 1) {
            if (x == 3) {
                collect2D->field_0x180 = 0x18f;
                collect2D->mItemNameString = 0x28f;
            } else if (x == 5) {
                collect2D->field_0x180 = 0x191;
                collect2D->mItemNameString = 0x291;
            }
        } else if (y == 2) {
            if (x == 4) {
                collect2D->field_0x180 = 0x194;
                collect2D->mItemNameString = 0x294;
            } else if (x == 5) {
                collect2D->field_0x180 = 0x196;
                collect2D->mItemNameString = 0x296;
            } else if (x == 6) {
                collect2D->field_0x180 = 0x195;
                collect2D->mItemNameString = 0x295;
            }
        }

        if (x < 7 && y < 6) {
            collect2D->field_0x184[x][y] = collect2D->field_0x180;
            collect2D->field_0x1d8[x][y] = collect2D->mItemNameString;
        }
        return HOOK_CONTINUE;
    }
    return HOOK_CONTINUE;
}

HookAction on_get_string_kanji_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    u32 msgID = mods::arg<u32>(args, 1);
    if (const SlotSpec* slot = slot_by_msgid(msgID)) {
        if (msgID == slot_name_id(slot) && slot->name.str) {
            TEXT_SPAN o_str = mods::arg<TEXT_SPAN>(args, 2);
            if (o_str) {
                SAFE_STRCPY(o_str, slot->name.str);
            }
            return HOOK_SKIP_ORIGINAL;
        }
    }
    if (msgID == 0x437) {
        TEXT_SPAN o_str = mods::arg<TEXT_SPAN>(args, 2);
        if (o_str) {
            SAFE_STRCPY(o_str, "Unequip");
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

static HookAction write_slot_description(void* args, void* ret, const char* text) {
    dMsgStringBase_c* msgStr = mods::arg<dMsgStringBase_c*>(args, 0);
    J2DTextBox* boxes[2] = { mods::arg<J2DTextBox*>(args, 2), mods::arg<J2DTextBox*>(args, 3) };
    COutFont_c* outFont = mods::arg<COutFont_c*>(args, 5);

    for (J2DTextBox* tb : boxes) {
        if (!tb) continue;
        if (msgStr) msgStr->resetStringLocal(tb);
        if (outFont) outFont->reset(tb);
        if (tb->getStringPtr()) {
            SAFE_STRCPY(tb->getStringPtr(), text);
        }
    }
    if (ret) *(f32*)ret = 0.0f;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_get_string_local_pre(ModContext*, void* args, void* ret, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    u32 msgID = mods::arg<u32>(args, 1);
    if (const SlotSpec* slot = slot_by_msgid(msgID)) {
        if (msgID == slot_desc_id(slot) && slot->description.str) {
            return write_slot_description(args, ret, slot->description.str);
        }
    }
    return HOOK_CONTINUE;
}
