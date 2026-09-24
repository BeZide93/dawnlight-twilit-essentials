#include "collection_internal.hpp"

// Cursor and text of the equipment rows.
//
// Native navigation walks the grid by cell index, which only matches the picture while the
// rows are native. The library walks the visible columns instead, with the native rules:
// straight up/down (skipping rows without that column), the Pieces of Heart and the Fused
// Shadow behind the sword row, down out of the clothes row into the item rows.

DEFINE_HOOK(&dMenu_Collect2D_c::getItemTag, GetItemTagHook);
DEFINE_HOOK(&dMenu_Collect2D_c::cursorMove, CursorMoveHook);
DEFINE_HOOK(&dMenu_Collect2D_c::cursorPosSet, CursorPosSetHook);
DEFINE_HOOK(&dMenu_Collect2D_c::pointerWait, PointerWaitHook);
DEFINE_HOOK(&dMeter2Info_c::getStringKanji, GetStringKanjiHook);
DEFINE_HOOK(&dMsgStringBase_c::getStringLocal, MsgStringGetStringLocalHook);

namespace {

constexpr u8 kNone = 0xFF;
constexpr int kNavCols = kClMaxCols + 2;   // + heart and fused shadow behind the sword row

struct Nav {
    u8 x[kClRows][kNavCols + 1];   // grid cell x per row and visible column (1-based)
};

Nav build_nav(dMenu_Collect2D_c* c) {
    Nav n;
    std::memset(n.x, kNone, sizeof(n.x));
    for (int r = 0; r < kClRows; r++) {
        for (int col = 1; col <= kClMaxCols; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type != ClColType::Empty) n.x[r][col] = column.x;
        }
    }
    int next = layout_last_col(0) + 1;
    if (screen_heart_on_main() && next <= kNavCols) n.x[0][next++] = 5;
    // Native: the fused shadow is only reachable once it is shown.
    if (screen_mask_on_main() && c->field_0x22d[6][0] != 0 && next <= kNavCols) n.x[0][next] = 6;
    return n;
}

int nav_col(const Nav& n, int r, u8 x) {
    for (int col = 1; col <= kNavCols; col++) {
        if (n.x[r][col] == x) return col;
    }
    return 0;
}

bool row_empty(const Nav& n, int r) {
    for (int col = 1; col <= kNavCols; col++) {
        if (n.x[r][col] != kNone) return false;
    }
    return true;
}

void move_cursor(dMenu_Collect2D_c* c, u8 x, u8 y) {
    c->field_0x259 = c->mCursorX;
    c->field_0x25a = c->mCursorY;
    c->mCursorX = x;
    c->mCursorY = y;
    Z2GetAudioMgr()->seStart(y == 5 ? Z2SE_SY_CURSOR_OPTION : Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f,
                             1.0f, -1.0f, -1.0f, 0);
    c->cursorPosSet();
    c->setItemNameString(x, y);
}

// Native: leaving the equipment rows downwards lands on the first item cell of this list.
void down_into_items(dMenu_Collect2D_c* c, u8* x, u8* y) {
    static const u8 kX[8] = {3, 2, 3, 1, 2, 0, 1, 0};
    static const u8 kY[8] = {3, 3, 4, 3, 4, 3, 4, 4};
    for (int i = 0; i < 8; i++) {
        if (c->getItemTag(kX[i], kY[i], true) != 0) {
            *x = kX[i];
            *y = kY[i];
            return;
        }
    }
    *y = 5;
    *x = c->mCursorX <= 2 ? 0 : 1;
}

u8 s_moveFromX = 0;
u8 s_moveFromY = 0;

HookAction on_cursor_move_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || c->mpStick == nullptr) return HOOK_CONTINUE;

    if (screen_hd_active()) {
        c->mpStick->checkTrigger();
        int dir;
        if (c->mpStick->checkRightTrigger()) {
            dir = 1;
        } else if (c->mpStick->checkLeftTrigger()) {
            dir = 0;
        } else if (c->mpStick->checkUpTrigger()) {
            dir = 2;
        } else if (c->mpStick->checkDownTrigger()) {
            dir = 3;
        } else {
            return HOOK_SKIP_ORIGINAL;
        }
        u8 tx = kNone;
        u8 ty = kNone;
        if (hd_nav_target(c, dir, &tx, &ty)) move_cursor(c, tx, ty);
        return HOOK_SKIP_ORIGINAL;
    }

    s_moveFromX = c->mCursorX;
    s_moveFromY = c->mCursorY;
    // With a page shown the equipment rows are off screen; the item rows move natively.
    if (collection_page_on_page()) return HOOK_CONTINUE;

    const u8 cx = c->mCursorX;
    const u8 cy = c->mCursorY;
    const int r = screen_equip_row_at(cx, cy);
    if (r < 0) return HOOK_CONTINUE;

    const Nav n = build_nav(c);
    const int col = nav_col(n, r, cx);
    if (col == 0) return HOOK_CONTINUE;

    c->mpStick->checkTrigger();
    const bool right = c->mpStick->checkRightTrigger();
    const bool left = !right && c->mpStick->checkLeftTrigger();
    const bool down = !right && !left && c->mpStick->checkDownTrigger();
    const bool up = !right && !left && !down && c->mpStick->checkUpTrigger();
    if (!right && !left && !down && !up) return HOOK_SKIP_ORIGINAL;

    const bool onHeart = r == 0 && cx == 5 && screen_heart_on_main();
    const bool onMask = r == 0 && cx == 6 && screen_mask_on_main();
    u8 tx = kNone;
    u8 ty = kNone;

    if (right) {
        for (int c2 = col + 1; c2 <= kNavCols && tx == kNone; c2++) {
            if (n.x[r][c2] != kNone) {
                tx = n.x[r][c2];
                ty = static_cast<u8>(r);
            }
        }
        if (tx == kNone) {
            // Native: past the shield row lies the heart, past the clothes row the fused
            // shadow - while they are on the grid. Otherwise the row wraps around.
            const bool maskReachable = screen_mask_on_main() && c->field_0x22d[6][0] != 0;
            if (r == 1 && screen_heart_on_main()) {
                tx = 5;
                ty = 0;
            } else if (r != 0 && maskReachable) {
                tx = 6;
                ty = 0;
            } else {
                for (int c2 = 1; c2 < col && tx == kNone; c2++) {
                    if (n.x[r][c2] != kNone) {
                        tx = n.x[r][c2];
                        ty = static_cast<u8>(r);
                    }
                }
            }
        }
    } else if (left) {
        const u8 px = c->field_0x259;
        const u8 py = c->field_0x25a;
        const int shieldLast = layout_last_col(1);
        if (onMask && screen_equip_row_at(px, py) >= 0 && !(px == 6 && py == 0)) {
            // Native: back to where the cursor came from.
            tx = px;
            ty = py;
        } else if (onHeart && py == 1 && shieldLast != 0 && px == n.x[1][shieldLast]) {
            tx = px;
            ty = py;
        } else {
            for (int c2 = col - 1; c2 >= 1 && tx == kNone; c2--) {
                if (n.x[r][c2] != kNone) {
                    tx = n.x[r][c2];
                    ty = static_cast<u8>(r);
                }
            }
        }
    } else if (onMask) {
        // Native: from the fused shadow, down leaves the equipment rows.
        if (down) down_into_items(c, &tx, &ty);
    } else {
        const int step = down ? 1 : -1;
        // Same column in the next row that has it.
        for (int rr = r + step; rr >= 0 && rr < kClRows && tx == kNone; rr += step) {
            if (n.x[rr][col] != kNone) {
                tx = n.x[rr][col];
                ty = static_cast<u8>(rr);
            }
        }
        // Otherwise the nearest column of the next row that has any.
        for (int rr = r + step; rr >= 0 && rr < kClRows && tx == kNone; rr += step) {
            if (row_empty(n, rr)) continue;
            int best = 0;
            for (int c2 = 1; c2 <= kNavCols; c2++) {
                if (n.x[rr][c2] == kNone) continue;
                const int d = c2 > col ? c2 - col : col - c2;
                const int bd = best > col ? best - col : col - best;
                if (best == 0 || d < bd) best = c2;
            }
            tx = n.x[rr][best];
            ty = static_cast<u8>(rr);
        }
        if (tx == kNone && down) down_into_items(c, &tx, &ty);
    }

    if (tx != kNone) move_cursor(c, tx, ty);
    return HOOK_SKIP_ORIGINAL;
}

// Native cursor code uses its own copy of the cell table (getItemTag is inlined there), so it
// can walk from the item rows into cells the layout emptied or that are off screen.
void on_cursor_move_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || screen_hd_active()) return;

    const bool enteredGrid = s_moveFromY >= kClRows && c->mCursorY < kClRows;

    if (collection_page_on_page()) {
        // Up from the item rows while a page is shown goes onto the page.
        const bool stayedUp = c->mCursorX == s_moveFromX && c->mCursorY == s_moveFromY &&
                              c->mCursorY == 3 && c->mpStick != nullptr && c->mpStick->checkUpTrigger();
        if (!enteredGrid && !stayedUp) return;
        if (enteredGrid) {
            c->mCursorX = s_moveFromX;
            c->mCursorY = s_moveFromY;
        }
        if (!collection_page_focus_first(c) && enteredGrid) {
            c->cursorPosSet();
            c->setItemNameString(c->mCursorX, c->mCursorY);
        }
        return;
    }

    if (!enteredGrid || screen_equip_row_at(c->mCursorX, c->mCursorY) >= 0) return;

    // Native order of cells tried when walking up from the item rows, on the real grid.
    static const u8 kUpX[9] = {3, 3, 4, 3, 4, 5, 4, 5, 5};
    static const u8 kUpY[9] = {2, 1, 2, 0, 1, 2, 0, 1, 0};
    u8 tx = kNone;
    u8 ty = kNone;
    for (int i = 0; i < 9 && tx == kNone; i++) {
        if (screen_equip_row_at(kUpX[i], kUpY[i]) >= 0) {
            tx = kUpX[i];
            ty = kUpY[i];
        }
    }
    for (int r = kClRows - 1; r >= 0 && tx == kNone; r--) {
        for (int col = 1; col <= kClMaxCols && tx == kNone; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type == ClColType::Empty) continue;
            tx = column.x;
            ty = static_cast<u8>(r);
        }
    }
    if (tx == kNone) {
        tx = s_moveFromX;
        ty = s_moveFromY;
    }
    c->mCursorX = tx;
    c->mCursorY = ty;
    c->cursorPosSet();
    c->setItemNameString(tx, ty);
}

// ---------------------------------------------------------------------------
// Mouse pointer
//
// The native pointerWait() only tests the cells of its own table. The cells the library
// added get the same treatment here, through the game's menu pointer functions.
// ---------------------------------------------------------------------------

bool (*s_hitRect)(f32, f32, f32, f32, f32) = nullptr;
void (*s_setHoverTarget)(u16) = nullptr;
bool (*s_consumeClick)() = nullptr;

// Cells in the native getItemTag() table of the equipment rows.
bool native_table_cell(u8 x, u8 y) {
    if (y == 0) return x >= 3 && x <= 6;
    if (y == 1) return x == 3 || x == 4;
    return x >= 3 && x <= 5;
}

// dusk::menu_pointer::hit_pane(CPaneMgr*, 8.0f).
bool pointer_hits(CPaneMgr* pm) {
    J2DPane* pane = pm != nullptr ? pm->getPanePtr() : nullptr;
    if (pane == nullptr) return false;
    Mtx mtx;
    f32 left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
    for (u8 i = 0; i < 4; i++) {
        const Vec v = pm->getGlobalVtx(pane, &mtx, i, false, 0);
        if (i == 0 || v.x < left) left = v.x;
        if (i == 0 || v.x > right) right = v.x;
        if (i == 0 || v.y < top) top = v.y;
        if (i == 0 || v.y > bottom) bottom = v.y;
    }
    return s_hitRect(left, top, right, bottom, 8.0f);
}

void on_pointer_wait_post(ModContext*, void* args, void* ret, void*) {
    if (args == nullptr || ret == nullptr || *static_cast<bool*>(ret)) return;
    if (s_hitRect == nullptr || s_setHoverTarget == nullptr || s_consumeClick == nullptr) return;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || collection_page_on_page()) return;

    for (u8 y = 0; y < kClRows; y++) {
        for (u8 x = 0; x < 7; x++) {
            if (native_table_cell(x, y) || layout_col_of_cell(y, x) == 0) continue;
            if (!pointer_hits(c->mpSelPm[x][y])) continue;

            s_setHoverTarget(static_cast<u16>(x + y * 7));
            if (c->mCursorX != x || c->mCursorY != y) {
                Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CURSOR_COMMON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                c->mCursorX = x;
                c->mCursorY = y;
                c->cursorPosSet();
                c->setItemNameString(x, y);
            }
            if (s_consumeClick()) {
                c->pointerActivateCurrent();
                *static_cast<bool*>(ret) = true;
            }
            return;
        }
    }
}

template <class Fn>
void resolve_fn(const HookService* hook_svc, const char* name, Fn* out) {
    void* addr = nullptr;
    if (hook_svc->resolve == nullptr || hook_svc->resolve(g_modCtx, name, &addr, nullptr) != MOD_OK) {
        log_collect_info("collection-lib: '%s' not found, no mouse pointer on added slots", name);
        return;
    }
    *out = reinterpret_cast<Fn>(addr);
}

HookAction on_get_item_tag_pre(ModContext*, void* args, void* ret, void*) {
    if (args == nullptr || ret == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    const int x = mods::arg<int>(args, 1);
    const int y = mods::arg<int>(args, 2);
    if (!screen_active(c) || y < 0 || y >= kClRows || x < 0 || x >= 7) return HOOK_CONTINUE;

    u64& tag = *static_cast<u64*>(ret);
    if (collection_page_on_page()) {
        // The equipment rows are off screen: not selectable (cursor or pointer).
        tag = 0;
        return HOOK_SKIP_ORIGINAL;
    }

    const int col = layout_col_of_cell(y, static_cast<u8>(x));
    if (col != 0) {
        if (layout_column(y, col).type == ClColType::Native) return HOOK_CONTINUE;
        tag = screen_custom_icon_tag(y, static_cast<u8>(x));
        return HOOK_SKIP_ORIGINAL;
    }
    if (y == 0 && ((x == 5 && screen_heart_on_main()) || (x == 6 && screen_mask_on_main()))) {
        return HOOK_CONTINUE;
    }
    tag = 0;
    return HOOK_SKIP_ORIGINAL;
}

// The pane of a cell. The equipment rows come from the layout, not from getItemTag(): its
// hook cannot be relied on (the function is inlined into the native menu code, and the
// hooking backend may fail to patch it).
J2DPane* any_cell_pane(dMenu_Collect2D_c* c, u8 x, u8 y) {
    if (y < kClRows) return screen_cell_pane(x, y);
    const u64 tag = c->getItemTag(x, y, true);
    return tag != 0 ? c->mpScreen->search(tag) : nullptr;
}

// Native cursorPosSet(), except that cell (6,0) only gets the small fused-shadow cursor while
// it still is the fused shadow.
HookAction on_cursor_pos_set_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c) || c->mpDrawCursor == nullptr) return HOOK_CONTINUE;

    const u8 cx = c->mCursorX;
    const u8 cy = c->mCursorY;
    if (cx >= 7 || cy >= 6) return HOOK_CONTINUE;
    const bool maskCell = screen_mask_on_main();

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 6; j++) {
            if ((i == 0 && j == 0) || (i == 6 && j == 0 && maskCell)) continue;
            J2DPane* pane = any_cell_pane(c, static_cast<u8>(i), static_cast<u8>(j));
            if (pane == nullptr) continue;
            const bool selected = i == cx && j == cy;
            if (j == 5) {
                const f32 sc = selected ? g_drawHIO.mCollectScreen.mSelectSaveOptionScale
                                        : g_drawHIO.mCollectScreen.mUnselectSaveOptionScale;
                pane->scale(sc, sc);
            } else {
                const f32 sc = selected ? g_drawHIO.mCollectScreen.mSelectItemScale
                                        : g_drawHIO.mCollectScreen.mUnselectItemScale;
                pane->scale(sc, sc);
            }
        }
    }

    c->mpDrawCursor->setAlphaRate(1.0f);
    CPaneMgr* pm = c->mpSelPm[cx][cy];
    J2DPane* pane = pm != nullptr ? pm->getPanePtr() : nullptr;
    if (pane == nullptr) pane = any_cell_pane(c, cx, cy);
    if (pane != nullptr) {
        const Vec pos = cl_pane_global_center(pane);
        c->mpDrawCursor->setPos(pos.x, pos.y, pane, false);
    }

    if (cy == 5) {
        c->mpDrawCursor->setParam(1.1f, 0.85f, 0.05f, 0.5f, 0.5f);
    } else if (cx == 6 && cy == 0 && maskCell) {
        c->mpDrawCursor->setParam(0.6f, 0.85f, 0.03f, 0.6f, 0.6f);
    } else {
        c->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.7f, 0.7f);
    }
    return HOOK_SKIP_ORIGINAL;
}

const char* custom_text(u32 msgId, bool* isDesc) {
    int r = 0;
    u8 x = 0;
    if (!layout_msg_lookup(msgId, &r, &x, isDesc)) return nullptr;
    const CustomEquipDef* d = custom_equip_get(layout_custom_at_cell(r, x));
    const char* text = d != nullptr ? (*isDesc ? d->description : d->name) : nullptr;
    return text != nullptr ? text : "";
}

HookAction on_get_string_kanji_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    const u32 msgId = mods::arg<u32>(args, 1);
    TEXT_SPAN out = mods::arg<TEXT_SPAN>(args, 2);

    bool isDesc = false;
    if (const char* text = custom_text(msgId, &isDesc)) {
        if (out) SAFE_STRCPY(out, text);
        return HOOK_SKIP_ORIGINAL;
    }
    if (msgId == kClUnequipMsg && cl_unequip_enabled() && screen_active(s_currentCollect2D)) {
        if (out) SAFE_STRCPY(out, "Unequip");
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction on_get_string_local_pre(ModContext*, void* args, void* ret, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    bool isDesc = false;
    const char* text = custom_text(mods::arg<u32>(args, 1), &isDesc);
    if (text == nullptr) return HOOK_CONTINUE;

    dMsgStringBase_c* msgStr = mods::arg<dMsgStringBase_c*>(args, 0);
    J2DTextBox* boxes[2] = {mods::arg<J2DTextBox*>(args, 2), mods::arg<J2DTextBox*>(args, 3)};
    COutFont_c* outFont = mods::arg<COutFont_c*>(args, 5);
    for (J2DTextBox* tb : boxes) {
        if (tb == nullptr) continue;
        if (msgStr != nullptr) msgStr->resetStringLocal(tb);
        if (outFont != nullptr) outFont->reset(tb);
        if (tb->getStringPtr()) SAFE_STRCPY(tb->getStringPtr(), text);
    }
    if (ret != nullptr) *static_cast<f32*>(ret) = 0.0f;
    return HOOK_SKIP_ORIGINAL;
}

}  // namespace

void nav_install_hooks(const HookService* hook_svc) {
    resolve_fn(hook_svc, "dusk::menu_pointer::hit_rect", &s_hitRect);
    resolve_fn(hook_svc, "dusk::menu_pointer::set_hover_target", &s_setHoverTarget);
    resolve_fn(hook_svc, "dusk::menu_pointer::consume_click", &s_consumeClick);
    CL_HOOK_POST(PointerWaitHook, on_pointer_wait_post);

    CL_HOOK_PRE(GetItemTagHook, on_get_item_tag_pre);
    CL_HOOK_PRE_PRIO(CursorMoveHook, on_cursor_move_pre, kClBeforeOtherMods);
    CL_HOOK_POST(CursorMoveHook, on_cursor_move_post);
    CL_HOOK_PRE(CursorPosSetHook, on_cursor_pos_set_pre);
    CL_HOOK_PRE(GetStringKanjiHook, on_get_string_kanji_pre);
    CL_HOOK_PRE(MsgStringGetStringLocalHook, on_get_string_local_pre);
}
