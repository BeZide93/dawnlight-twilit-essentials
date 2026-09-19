#include "collection_nav.hpp"
#include "collection_page.hpp"

#include "f_pc/f_pc_profile_lst.h"


// NOTE: this hook installs with MOD_ERROR on this build (getItemTag() is too
// small a function for the detour mechanism - confirmed via diagnostic logging
// during investigation) - the logic below never actually runs. Left in place,
// unmodified, in case a future engine build makes it hookable again; the real
// fix path is on_pointer_wait_replace below, which no longer depends on this.
HookAction on_get_item_tag_pre(ModContext*, void* args, void* ret, void*) {
    if (!is_collection_menu_enabled() || !args || !ret) return HOOK_CONTINUE;
    int i_tag1 = mods::arg<int>(args, 1);
    int i_tag2 = mods::arg<int>(args, 2);
    bool param_3 = mods::arg<bool>(args, 3);

    if (i_tag2 == 5 && !param_3) {
        *(u64*)ret = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    // Mod-added slots resolve to the pane tag recorded in their SlotSpec - a
    // custom slot owns its cell COMPLETELY, including the vanilla starter
    // cells: when the starter gear is off, auto-fill places a custom sword at
    // (3,0), and its unlocked() gate (not the starter gate below) decides
    // whether the cell - and therefore its mouse hover - exists.
    if (const SlotSpec* slot = slot_at(i_tag1, i_tag2)) {
        if (!is_collect_item_unlocked(i_tag1, i_tag2) || collection_page_active()) {
            *(u64*)ret = 0;
            return HOOK_SKIP_ORIGINAL;
        }
        *(u64*)ret = slot->iconTag;
        return HOOK_SKIP_ORIGINAL;
    }

    // Wooden-sword (3,0) and ordon-clothes (3,2) slots don't exist without the
    // starter-equip claim; the Ordon Shield (3,1) also goes unless "keep ordon
    // shield" holds it (greyed - still has a pane). Only reached for cells no
    // custom slot has claimed.
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

    // Any cell outside the visible 4x3 equip grid is a leftover vanilla hidden
    // pane (the game parks its own unused cells at x0..2 / y3..5). It has a
    // pane and a tag, so the mouse hover scan stops on it and shadows every
    // custom slot drawn further right. Report "no item" for them.
    if (i_tag2 >= 3 || i_tag1 < 3) {
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
        if (x == 3) return slot_icon(3, 2);
        if (x == 4) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n0'));
        if (x == 5) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n1'));
        if (x == 6) return collect2D->mpScreen->search(MULTI_CHAR('fuku_n2'));
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

    // Scale all panes
    for (u8 y = 0; y < 6; y++) {
        for (u8 x = 0; x < 7; x++) {
            J2DPane* pane = get_target_pane(collect2D, x, y);
            if (pane) {
                // (6,0) only skips the item scale while a PAGE owns the heart -
                // without a page it is a normal selectable grid item.
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
    bool inEquipGrid = (curX >= 3 && curX <= 6 && curY <= 2) || (curX == 6 && curY == 0) || (curSlot != nullptr && curSlot->autoLayout.on);
    if (inEquipGrid) {
        collect2D->mpStick->checkTrigger();

        u8 targetX = curX;
        u8 targetY = curY;
        bool moved = false;

        // One trigger read per direction (checkDown/UpTrigger mutate repeat-timer
        // state). dir: 0=left 1=right 2=up 3=down, -1=nothing.
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

        // Auto-managed mod slots declare their own neighbours in the SlotSpec.
        SlotCell nav = (dir >= 0) ? slot_nav_target(curX, curY, dir) : SlotCell{};

        if (dir < 0) {
            // nothing
        } else if (slot_cell_set(nav)) {
            targetX = nav.x;
            targetY = nav.y;
            moved = true;
        } else if (dir == 1) {   // right
            for (int tx = curX + 1; tx <= 6; tx++) {
                if (collect2D->field_0x22d[tx][curY] != 0) {
                    targetX = tx;
                    moved = true;
                    break;
                }
            }
        } else if (dir == 0) {   // left
            for (int tx = curX - 1; tx >= 3; tx--) {
                if (collect2D->field_0x22d[tx][curY] != 0) {
                    targetX = tx;
                    moved = true;
                    break;
                }
            }
            if (!moved && !cl_vanilla_layout_hidden()) {
                targetX = 2;
                targetY = (curY == 0) ? 3 : 4;
                moved = true;
            }
        } else {                 // up (2) / down (3)
            // Pick the slot in the next row that's visually closest, so vertical
            // nav still lines up when a row is shifted (starter-equip slot off).
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
                targetX = 3;
                targetY = 3;
                moved = true;
            }
        }

        if (moved) {
            collect2D->mCursorX = targetX;
            collect2D->mCursorY = targetY;
            Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CURSOR_COMMON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            collect2D->cursorPosSet();
            collect2D->setItemNameString(collect2D->mCursorX, collect2D->mCursorY);
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

// The mouse-hover scan (dMenu_Collect2D_c::pointerWait) walks cells in row-major
// order and stops at the FIRST whose pane hit-box (bounds + 8px) contains the
// cursor. heart_n and kamen_n both carry huge .blo bounds from their vanilla
// (Heart Container / Mirror of Twilight) layout, and the mod parks them off
// the visible grid on the main page - but their oversized hit-boxes still
// shadow Hylian Shield (5,1) and Magic Armor (6,2). Shrink both hit-boxes to
// off-screen for the duration of the scan, then put them back.
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
        // Only while a PAGE owns the heart is it parked off the grid. Without a
        // page it is a normal selectable grid item at (6,0) - keep its real
        // hit-box so the cursor/mouse can reach it.
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

    // modelbgn (the 3D Mirror-of-Twilight backdrop plate) has the same problem as
    // heart_n/kamen_n: huge vanilla .blo bounds, slid off-screen every frame by
    // collection_page_apply() but never bounds-neutralized, so its oversized
    // hit-box still shadowed the shield/clothes rows (whose Y sits closest to
    // modelbgn's own Y) even though it's invisible on the main page.
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

// dusk::menu_pointer::hit_pane lives in dusklight/src/dusk/ (engine-internal,
// not part of the public mod ABI collection-lib links against) - confirmed by a
// direct LNK2019 unresolved-external when called as a normal C++ call. Resolved
// instead via HookService::resolve() at init time (collection_lib.cpp), which
// does a symbol-table lookup only - no detour/patch attempt, so it can't fail
// the way GetItemTagHook's install() did. Mangled name pins the (CPaneMgr*,
// float) overload specifically (hit_pane also has a J2DPane* overload; the
// unqualified name is ambiguous and would resolve MOD_CONFLICT).
bool (*g_hitPaneFn)(CPaneMgr*, f32) = nullptr;
const char* const kHitPaneMangledName = "?hit_pane@menu_pointer@dusk@@YA_NPEAVCPaneMgr@@M@Z";

// TargetId turned out to be a plain `using TargetId = u16;` alias (no distinct
// enum type - mangles identically to a raw unsigned short param). Registers
// which cell the pointer is over; called before the click check below.
void (*g_setHoverTargetFn)(u16) = nullptr;
const char* const kSetHoverTargetMangledName = "?set_hover_target@menu_pointer@dusk@@YAXG@Z";

// peek_click() checks for a pending click without consuming it or requiring a
// prior set_hover_target() call to validate against - used instead of
// consume_click() so a click registers even the one frame set_hover_target
// hasn't caught up yet. Edge-detected ourselves (see activate() below) since
// it has no per-target consumption of its own.
bool (*g_peekClickFn)() = nullptr;
const char* const kPeekClickMangledName = "?peek_click@menu_pointer@dusk@@YA_NXZ";

// getItemTag() cannot be hooked on this build (GetItemTagHook installs with
// MOD_ERROR - confirmed via diagnostic logging - almost certainly because the
// compiled function is too small for the detour mechanism). Every custom-slot
// override in on_get_item_tag_pre is therefore silently inert: vanilla's own
// getItemTag() (and its static tag table) is what actually governs pointerWait()
// for every cell, on every build. It has no entry for Hylian Shield (5,1),
// Reinforced Shield (6,1) or Magic Armor (6,2), so vanilla pointerWait() never
// finds them - not a bounds bug, not a geometry bug, confirmed exhaustively.
//
// Fix: replace pointerWait() wholesale. Run the REAL vanilla implementation
// first via g_orig - untouched behaviour (sound, click activation, everything)
// for the ~39 cells that already work correctly. Only if vanilla found nothing
// this frame do we fall back to testing our 3 known-good panes directly with
// hit_pane() (geometry independently verified correct via HOVER_DIAG) and, on a
// hit, place the cursor exactly as pointerWait() would. consume_click() takes
// no parameters (no enum-encoding to guess, unlike begin_context/set_hover_target,
// which this fallback still avoids), so it's resolved and called the same safe
// way as hit_pane to drive click-to-equip through pointerActivateCurrent() -
// matching what real pointerWait() does for every other cell.
void on_pointer_wait_replace(ModContext*, void* args, void* retval, void*) {
    dMenu_Collect2D_c* self = args ? mods::arg<dMenu_Collect2D_c*>(args, 0) : nullptr;
    bool result = false;
    if (self) {
        if (PointerWaitHook::g_orig) {
            result = PointerWaitHook::g_orig(self);
        }

        if (!result && g_hitPaneFn) {
            auto activate = [&](u8 x, u8 y) {
                // set_hover_target BEFORE the click check - consume_click() (per
                // dusklight's "Refine menu_pointer click events" change) validates
                // the click landed on the currently-registered target, so without
                // this call it has nothing of ours to validate against.
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
                // peek_click() has no target concept, so a held-down click would
                // re-report true every frame - track our own edge (not-clicked ->
                // clicked) so a single click activates exactly once, matching
                // vanilla's button-press semantics.
                static bool s_wasClicked = false;
                bool isClicked = g_peekClickFn && g_peekClickFn();
                if (isClicked && !s_wasClicked) {
                    self->pointerActivateCurrent();
                    result = true;
                }
                s_wasClicked = isClicked;
            };

            // Every autoLayout (custom) slot has the exact same problem as the 2
            // hardcoded vanilla cells below - vanilla's own tag table has no
            // entry for any cell whose content depends on our mod (x<3 hidden
            // columns AND a 4th shield/tunic column both read back as "empty"
            // from vanilla's perspective). Iterate the registry instead of a
            // fixed cell list so a future custom item (see the 7th-slot fix)
            // is covered automatically.
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

            // Hylian Shield (5,1) / Magic Armor (6,2): pure vanilla cells with no
            // SlotSpec of their own, so the loop above can't find them.
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

    // While ANY page is the target the item grid is slid out/invisible - name
    // strings for its cells would only flicker (the cursor can still wander
    // the hidden cells after a drop). Force every setter to Null.
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
                collect2D->field_0x180 = 0x1a4; // Wooden Sword
                collect2D->mItemNameString = 0x2a4;
            } else if (x == 4 && cl_item23_swapped(1)) {
                // Relocated native Master Sword pane (item2/3-swapped row) -
                // same native message ids as the x==5 case below.
                collect2D->field_0x180 = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? 0x1ae : 0x18e; // Master Sword
                collect2D->mItemNameString = collect2D->field_0x180 + 0x100;
            } else if (x == 5) {
                collect2D->field_0x180 = dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ? 0x1ae : 0x18e; // Master Sword
                collect2D->mItemNameString = collect2D->field_0x180 + 0x100;
            } else if (x == 6) {
                collect2D->field_0x180 = 0x186; // Heart Container
                collect2D->mItemNameString = 0x286;
            }
        } else if (y == 1) {
            if (x == 3) {
                collect2D->field_0x180 = 0x18f; // Wooden Shield
                collect2D->mItemNameString = 0x28f;
            } else if (x == 5) {
                collect2D->field_0x180 = 0x191; // Hylian Shield
                collect2D->mItemNameString = 0x291;
            }
        } else if (y == 2) {
            if (x == 4) {
                collect2D->field_0x180 = 0x194; // Kokiri Clothes
                collect2D->mItemNameString = 0x294;
            } else if (x == 5) {
                collect2D->field_0x180 = 0x196; // Zora Armor
                collect2D->mItemNameString = 0x296;
            } else if (x == 6) {
                collect2D->field_0x180 = 0x195; // Magic Armor
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
            // DIAGNOSTIC (2026-09-18): same check as on_get_string_local_pre -
            // confirm whether the NAME hook fires at all for hidden-column
            // (x<3) slots. Remove once the name/description investigation
            // concludes.
            if (slot->x < 3) {
                static int s_nameLogs = 0;
                if (s_nameLogs < 20) {
                    log_collect_info("name hook fired: cell(%d,%d) '%s' msgID=0x%x", slot->x, slot->y, slot->name.str, msgID);
                    s_nameLogs++;
                }
            }
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
            // DIAGNOSTIC (2026-09-18): descriptions reportedly go missing for
            // items parked on the hidden x<3 columns (item 5+ on any row) while
            // their name text shows fine. Logs a few samples to check whether
            // this hook even fires for those cells and, if so, whether the
            // textboxes the caller hands write_slot_description are null -
            // remove once the cause is confirmed.
            if (slot->x < 3) {
                static int s_descLogs = 0;
                if (s_descLogs < 20) {
                    J2DTextBox* tb0 = mods::arg<J2DTextBox*>(args, 2);
                    J2DTextBox* tb1 = mods::arg<J2DTextBox*>(args, 3);
                    log_collect_info("desc hook fired: cell(%d,%d) msgID=0x%x box0=%p box1=%p",
                                     slot->x, slot->y, msgID, (void*)tb0, (void*)tb1);
                    s_descLogs++;
                }
            }
            return write_slot_description(args, ret, slot->description.str);
        }
    }
    return HOOK_CONTINUE;
}


