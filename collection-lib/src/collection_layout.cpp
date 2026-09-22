#include "collection_layout.hpp"
#include "collection_page.hpp"
#include "collection_lib/custom_equip.hpp"
#include "Z2AudioLib/Z2SeMgr.h"

static void addSlot(J2DScreen* screen, const SlotSpec& spec);

static void addSlotRow(J2DScreen* screen, u8 row, u8 item, SlotSpec s) {
    s.at = { row, item };
    addSlot(screen, s);
}
static void addSwordItem(J2DScreen* s, u8 item, SlotSpec spec)  { addSlotRow(s, 1, item, spec); }
static void addShieldItem(J2DScreen* s, u8 item, SlotSpec spec) { addSlotRow(s, 2, item, spec); }
static void addTunicItem(J2DScreen* s, u8 item, SlotSpec spec)  { addSlotRow(s, 3, item, spec); }

static void addSlot(J2DScreen* screen, const SlotSpec& spec) {
    if (!screen) return;

    SlotSpec s = spec;
    SlotCell cell = grid_cell(spec.at.row, spec.at.item);
    s.x = cell.x;
    s.y = cell.y;

    u8 tmplRow = (spec.at.row >= 1 && spec.at.row <= 3) ? static_cast<u8>(spec.at.row - 1) : 2;
    u64 posSrcTag, texSrcTag, frameSrcTag;
    switch (tmplRow) {
    case 0:  posSrcTag = MULTI_CHAR('ken_n0');  texSrcTag = MULTI_CHAR('ken_01');  frameSrcTag = MULTI_CHAR('ken_g_0');  break;
    case 1:  posSrcTag = MULTI_CHAR('tate_n0'); texSrcTag = MULTI_CHAR('tate_00'); frameSrcTag = MULTI_CHAR('tate_g_0'); break;
    default: posSrcTag = MULTI_CHAR('fuku_n0'); texSrcTag = MULTI_CHAR('fuku_00'); frameSrcTag = MULTI_CHAR('fuku_g_0'); break;
    }

    J2DPane*    cont = screen->search(s.iconTag);
    J2DPicture* pic  = static_cast<J2DPicture*>(screen->search(s.iconPicTag));
    J2DPicture* frm  = static_cast<J2DPicture*>(screen->search(s.frameTag));

    if (s.enabled) {
        J2DPane* posSrc   = screen->search(posSrcTag);
        J2DPane* texSrc   = screen->search(texSrcTag);
        J2DPane* frameSrc = screen->search(frameSrcTag);

        if (!frm && frameSrc && frameSrc->getParentPane()) {
            const ResTIMG* frameTex = safe_get_tex_info(frameSrc);
            if (frameTex) {
                frm = JKR_NEW J2DPicture(s.frameTag, frameSrc->mBounds, frameTex, nullptr);
                frm->mKind = 'PIC1';
                frm->setBasePosition((J2DBasePosition)frameSrc->mBasePosition);
                frm->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
                J2DPicture* fp = static_cast<J2DPicture*>(frameSrc);
                frm->setCornerColor(fp->corner(0), fp->corner(1), fp->corner(2), fp->corner(3));
                frm->setBlackWhite(fp->getBlack(), fp->getWhite());
                frameSrc->getParentPane()->appendChild(frm);
            }
        }

        if (!cont && posSrc && posSrc->getParentPane()) {
            cont = JKR_NEW J2DPane(posSrc->getParentPane(), true, s.iconTag, posSrc->mBounds);
            cont->setBasePosition((J2DBasePosition)posSrc->mBasePosition);
            cont->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);

            const ResTIMG* iconTex = s.texOverride ? s.texOverride : safe_get_tex_info(texSrc);
            if (iconTex) {
                pic = JKR_NEW J2DPicture(
                    s.iconPicTag, texSrc ? texSrc->mBounds : posSrc->mBounds, iconTex, nullptr);
                pic->mKind = 'PIC1';
                if (texSrc) pic->setBasePosition((J2DBasePosition)texSrc->mBasePosition);
                pic->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
                pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
                pic->setAlpha(255);
                cont->appendChild(pic);
                pic->translate(0.0f, 0.0f);
            }
        }
    }

    s.icon = cont;
    s.iconPic = pic;
    s.frame = frm;
    if (s.outIcon)    *s.outIcon    = cont;
    if (s.outIconPic) *s.outIconPic = pic;
    if (s.outFrame)   *s.outFrame   = frm;

    if (s.enabled) slot_registry_add(s);
}

bool cl_item_exists(u8 row, u8 item) {

    if (item == 1 && cl_column_claimed(row, 1)) return true;

    if (cl_vanilla_layout_hidden()) {

    } else if (row == 1) {
        if (item >= 2 && item <= 3) return true;
    } else if (row == 2) {
        if (item >= 2 && item <= 3) return true;
    } else if (row == 3) {
        if (item >= 2 && item <= 4) return true;
    }

    for (int i = 0; i < custom_equip_count(); i++) {
        const CustomEquipDef* d = custom_equip_get(i);
        if (d != nullptr) {
            u8 r = (d->kind == CE_SWORD) ? 1 : (d->kind == CE_SHIELD) ? 2 : 3;
            if (r == row && d->item == item) return true;
        }
    }
    return false;
}

static u8 find_closest_item(u8 targetRow, u8 idealItem) {
    u8 bestItem = 1;
    int bestDist = 999;

    u8 maxCol = (targetRow == 3) ? 4 : 3;
    for (int i = 0; i < custom_equip_count(); i++) {
        const CustomEquipDef* d = custom_equip_get(i);
        if (d != nullptr) {
            u8 r = (d->kind == CE_SWORD) ? 1 : (d->kind == CE_SHIELD) ? 2 : 3;
            if (r == targetRow && d->item > maxCol) {
                maxCol = d->item;
            }
        }
    }

    for (u8 it = 1; it <= maxCol; it++) {
        if (cl_item_exists(targetRow, it)) {
            int d = (it > idealItem) ? (it - idealItem) : (idealItem - it);
            if (d < bestDist) {
                bestDist = d;
                bestItem = it;
            }
        }
    }
    return bestItem;
}

static void add_custom_equip_slots(J2DScreen* screen) {
    custom_equip_reset_registry();
    collectionlib_run_slot_registration();

    u8 maxItemInRow[4] = {};
    for (int id = 0; id < custom_equip_count(); id++) {
        const CustomEquipDef* d = custom_equip_get(id);
        const u8 row = d->kind == CE_SWORD ? 1 : d->kind == CE_SHIELD ? 2 : 3;
        if (d->item > maxItemInRow[row]) maxItemInRow[row] = d->item;
    }

    for (int id = 0; id < custom_equip_count(); id++) {
        const CustomEquipDef* d = custom_equip_get(id);
        const u8 row  = d->kind == CE_SWORD ? 1 : d->kind == CE_SHIELD ? 2 : 3;
        const f32 rowY = row == 1 ? s_ken_n0_origY : row == 2 ? s_tate_n0_origY : s_fuku_n0_origY;

        SlotSpec s{};
        s.enabled     = true;
        s.iconTag     = custom_equip_icon_tag(id);
        s.iconPicTag  = custom_equip_pic_tag(id);
        s.frameTag    = custom_equip_frame_tag(id);
        s.texOverride = custom_equip_icon(id);
        s.name        = d->name;
        s.description = d->description;
        s.onEquip     = &custom_equip_on_equip;
        s.unlockFn    = &custom_equip_is_unlocked;
        s.equippedFn  = &custom_equip_is_equipped;
        s.autoLayout.on   = true;
        s.autoLayout.posX = collection_slot_x(static_cast<f32>(d->item - 1));
        s.autoLayout.posY = rowY;
        if (d->item > 1) s.autoLayout.navLeft = { row, static_cast<u8>(d->item - 1) };
        if (d->item == maxItemInRow[row]) s.autoLayout.navRight = { row, 1 };
        if (row > 1) {
            u8 upItem = find_closest_item(row - 1, d->item);
            s.autoLayout.navUp = { static_cast<u8>(row - 1), upItem };
        }
        if (row < 3) {
            u8 downItem = find_closest_item(row + 1, d->item);
            s.autoLayout.navDown = { static_cast<u8>(row + 1), downItem };
        }

        if (row == 1)      addSwordItem(screen, d->item, s);
        else if (row == 2) addShieldItem(screen, d->item, s);
        else               addTunicItem(screen, d->item, s);
    }
}

void update_screen_bases(J2DScreen* screen, JKRExpHeap* heap) {
    if (!screen) return;

    if (screen != s_cachedScreen) {
        s_cachedScreen = screen;
        s_picTunagiKen2 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tuna_k2')));
        s_picTunagiTate2 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tuna_t2')));
        s_picTunagiFuku3 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tuna_f3')));

        J2DPane* tunagi01 = screen->search(MULTI_CHAR('tunagi01'));
        J2DPane* tunagi03 = screen->search(MULTI_CHAR('tunagi03'));
        J2DPane* tunagi06 = screen->search(MULTI_CHAR('tunagi06'));

        for (int ci = 0; ci < 6; ci++) {
            s_customConnectors[ci] = nullptr;
        }
        s_customConnectorCount = 0;
        s_customConnectorParent[0] = tunagi01 ? tunagi01->getParentPane() : nullptr;
        s_customConnectorParent[1] = tunagi03 ? tunagi03->getParentPane() : nullptr;
        s_customConnectorParent[2] = tunagi06 ? tunagi06->getParentPane() : nullptr;
        s_customConnectorTemplate[0] = tunagi01 ? static_cast<J2DPicture*>(tunagi01) : nullptr;
        s_customConnectorTemplate[1] = tunagi03 ? static_cast<J2DPicture*>(tunagi03) : nullptr;
        s_customConnectorTemplate[2] = tunagi06 ? static_cast<J2DPicture*>(tunagi06) : nullptr;

        JKRHeap* oldHeap = nullptr;
        if (heap != nullptr) {
            oldHeap = mDoExt_setCurrentHeap(heap);
        }

        slot_registry_clear();

        for (int vi = 0; vi < cl_vanilla_slot_count(); ++vi) {
            const CollectionVanillaSlotDef* vd = cl_vanilla_slot_get(vi);
            const u8 vrow = cl_vanilla_slot_row(vi);
            const u8 vitem = cl_vanilla_slot_item(vi);
            const SlotCell vc = grid_cell(vrow, vitem);

            if (vrow == 3 && vitem == 1) {
                SlotSpec vs{};
                vs.enabled = true;
                vs.iconTag = MULTI_CHAR('fuku_ord');
                vs.iconPicTag = MULTI_CHAR('fuku_io');
                vs.frameTag = MULTI_CHAR('fuku_go');
                vs.texOverride = vd->icon;
                if (vd->name != nullptr) vs.name = SlotText(vd->name);
                else vs.name = SlotText(static_cast<u32>(vd->nameMsgId));
                if (vd->description != nullptr) vs.description = SlotText(vd->description);
                else vs.description = SlotText(static_cast<u32>(vd->descMsgId));
                vs.unlockFn = &cl_vanilla_slot_unlocked;
                vs.equippedFn = &cl_vanilla_slot_equipped;
                vs.autoLayout.on = true;
                vs.autoLayout.posX = collection_slot_x(0.0f);
                vs.autoLayout.posY = s_fuku_n0_origY;
                addTunicItem(screen, 1, vs);
            }
        }

        add_custom_equip_slots(screen);

        if (!cl_column_occupied(1, 2) && !cl_item23_swapped(1)) {
            addSwordItem(screen, 2, { {}, true,
                MULTI_CHAR('ken_mid'), MULTI_CHAR('ken_im'), MULTI_CHAR('ken_gm'),
                nullptr, 0x18d, 0x28d, nullptr });
        }

        if (!cl_column_occupied(2, 2) && !cl_item23_swapped(2)) {
            addShieldItem(screen, 2, { {}, true,
                MULTI_CHAR('tate_mid'), MULTI_CHAR('tate_im'), MULTI_CHAR('tate_gm'),
                nullptr, 0x190, 0x290, nullptr });
        }

        if (!s_picTunagiKen2 && tunagi01 && tunagi01->getParentPane()) {
            const ResTIMG* tex = safe_get_tex_info(tunagi01);
            if (tex) {
                s_picTunagiKen2 = JKR_NEW J2DPicture(MULTI_CHAR('tuna_k2'), tunagi01->mBounds, tex, nullptr);
                s_picTunagiKen2->setBasePosition((J2DBasePosition)tunagi01->mBasePosition);
                s_picTunagiKen2->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
                static_cast<CustomPicture*>(s_picTunagiKen2)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi01));
                tunagi01->getParentPane()->appendChild(s_picTunagiKen2);
            }
        }

        if (!s_picTunagiTate2 && tunagi03 && tunagi03->getParentPane()) {
            const ResTIMG* tex = safe_get_tex_info(tunagi03);
            if (tex) {
                s_picTunagiTate2 = JKR_NEW J2DPicture(MULTI_CHAR('tuna_t2'), tunagi03->mBounds, tex, nullptr);
                s_picTunagiTate2->setBasePosition((J2DBasePosition)tunagi03->mBasePosition);
                s_picTunagiTate2->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
                static_cast<CustomPicture*>(s_picTunagiTate2)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi03));
                tunagi03->getParentPane()->appendChild(s_picTunagiTate2);
            }
        }

        if (!s_picTunagiFuku3 && tunagi06 && tunagi06->getParentPane()) {
            const ResTIMG* tex = safe_get_tex_info(tunagi06);
            if (tex) {
                s_picTunagiFuku3 = JKR_NEW J2DPicture(MULTI_CHAR('tuna_f3'), tunagi06->mBounds, tex, nullptr);
                s_picTunagiFuku3->setBasePosition((J2DBasePosition)tunagi06->mBasePosition);
                s_picTunagiFuku3->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
                static_cast<CustomPicture*>(s_picTunagiFuku3)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi06));
                tunagi06->getParentPane()->appendChild(s_picTunagiFuku3);
            }
        }

        collection_page_sync_screen(screen);

        if (oldHeap != nullptr) {
            mDoExt_setCurrentHeap(oldHeap);
        }
    }
}

static void layout_managed_slots(dMenu_Collect2D_c* collect2D) {
    J2DPane* ref = collect2D->mpScreen ? collect2D->mpScreen->search(MULTI_CHAR('fuku_n0')) : nullptr;
    f32 gridDx = collection_page_grid_dx();

    for (int i = 0; i < slot_count(); i++) {
        const SlotSpec* s = slot_get(i);
        if (!s->autoLayout.on) continue;

        J2DPane*    cont = s->icon;
        J2DPicture* pic  = s->iconPic;
        J2DPicture* frm  = s->frame;

        const f32 rowShift = cl_column_occupied(static_cast<u8>(s->y + 1), 1) ? 0.0f : -(s_col_dx + 5.0f);
        const f32 px = s->autoLayout.posX + gridDx + rowShift;
        const f32 py = s->autoLayout.posY;
        const bool vis = is_collect_item_unlocked(s->x, s->y);

        if (cont) {
            cont->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
            set_pane_pos(cont, px, py);
            if (ref) cont->scale(ref->getScaleX(), ref->getScaleY());
            if (vis) cont->show(); else cont->hide();
        }
        if (pic) {
            pic->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
            pic->translate(0.0f, 0.0f);
            if (s->texOverride && s->texOverride != slot_applied_tex(i)) {
                pic->changeTexture(s->texOverride, 0);
                slot_set_applied_tex(i, s->texOverride);
            }
            if (vis) pic->show(); else pic->hide();
        }
        if (frm) {
            const f32 frameY = (s->at.row == 1) ? s_ken_g0_origY : (s->at.row == 2) ? s_tate_g0_origY : s_fuku_g0_origY;
            frm->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
            set_pane_pos(frm, px - 24.5f, frameY);
            if (vis) frm->show(); else frm->hide();
        }
    }

    static const f32 kConnY[3] = { s_ken_g0_origY, s_tate_g0_origY, s_fuku_g0_origY };
    const f32 dxC    = s_col_dx + 5.0f;
    const f32 baseXC = s_ken_n0_origX + gridDx;

    int connIdx = 0;
    for (u8 row = 1; row <= 3; row++) {
        const int tmpl = row - 1;
        J2DPane*    parent = s_customConnectorParent[tmpl];
        J2DPicture* srcPic = s_customConnectorTemplate[tmpl];
        if (!parent || !srcPic) continue;

        u8 prevItem = (row == 3) ? 4 : 3;

        u8 cols[8];
        int n = 0;
        for (int i = 0; i < custom_equip_count() && n < 8; i++) {
            const CustomEquipDef* d = custom_equip_get(i);
            if (!d) continue;
            const u8 r = d->kind == CE_SWORD ? 1 : d->kind == CE_SHIELD ? 2 : 3;
            if (r != row) continue;
            if (d->item <= prevItem) continue;
            int k = n;
            while (k > 0 && cols[k - 1] > d->item) {
                cols[k] = cols[k - 1];
                k--;
            }
            cols[k] = d->item;
            n++;
        }

        const f32 shiftC = cl_column_occupied(row, 1) ? 0.0f : -dxC;

        for (int k = 0; k < n && connIdx < 6; k++) {
            const u8 item = cols[k];
            const f32 connX = baseXC + shiftC + ((prevItem - 1) + (item - 1)) * 0.5f * dxC - 24.5f;
            const f32 connY = kConnY[tmpl];

            J2DPicture* cp = s_customConnectors[connIdx];
            if (!cp) {
                const ResTIMG* tex = safe_get_tex_info(srcPic);
                if (tex) {
                    cp = JKR_NEW J2DPicture(static_cast<u64>(0x63636E00 + connIdx),
                                            srcPic->mBounds, tex, nullptr);
                    cp->setBasePosition((J2DBasePosition)srcPic->mBasePosition);
                    static_cast<CustomPicture*>(cp)->copyVisualsFrom(srcPic);
                    parent->appendChild(cp);
                    s_customConnectors[connIdx] = cp;
                }
            }
            if (cp) {
                static_cast<CustomPicture*>(cp)->copyVisualsFrom(srcPic);
                cp->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
                set_pane_pos(cp, connX, connY);
                cp->show();
            }
            prevItem = item;
            connIdx++;
        }
    }
    s_customConnectorCount = connIdx;
    for (int ci = connIdx; ci < 6; ci++) {
        if (s_customConnectors[ci]) s_customConnectors[ci]->hide();
    }
}

static void configure_managed_slots(dMenu_Collect2D_c* collect2D) {
    for (int i = 0; i < slot_count(); i++) {
        const SlotSpec* s = slot_get(i);
        if (!s->autoLayout.on || s->x >= 7 || s->y >= 6) continue;

        const bool selectable = is_collect_item_unlocked(s->x, s->y);
        collect2D->field_0x22d[s->x][s->y] = selectable ? 1 : 0;
        collect2D->field_0x184[s->x][s->y] = slot_name_id(s);
        collect2D->field_0x1d8[s->x][s->y] = slot_desc_id(s);
    }
}

void cl_apply_blank_layout(dMenu_Collect2D_c* collect2D) {
    if (collect2D == nullptr || collect2D->mpScreen == nullptr) return;
    if (!cl_vanilla_layout_hidden()) return;

    J2DScreen* screen = collect2D->mpScreen;
    J2DPane* ken_n0 = screen->search(MULTI_CHAR('ken_n0'));
    J2DPane* ken_n1 = screen->search(MULTI_CHAR('ken_n1'));
    J2DPane* tate_n0 = screen->search(MULTI_CHAR('tate_n0'));
    J2DPane* tate_n1 = screen->search(MULTI_CHAR('tate_n1'));
    J2DPane* fuku_n0 = screen->search(MULTI_CHAR('fuku_n0'));
    J2DPane* fuku_n1 = screen->search(MULTI_CHAR('fuku_n1'));
    J2DPane* fuku_n2 = screen->search(MULTI_CHAR('fuku_n2'));
    J2DPane* heart_n = screen->search(MULTI_CHAR('heart_n'));
    J2DPane* ken_g0 = screen->search(MULTI_CHAR('ken_g_0'));
    J2DPane* ken_g1 = screen->search(MULTI_CHAR('ken_g_1'));
    J2DPane* tate_g0 = screen->search(MULTI_CHAR('tate_g_0'));
    J2DPane* tate_g1 = screen->search(MULTI_CHAR('tate_g_1'));
    J2DPane* fuku_g0 = screen->search(MULTI_CHAR('fuku_g_0'));
    J2DPane* fuku_g1 = screen->search(MULTI_CHAR('fuku_g_1'));
    J2DPane* fuku_g2 = screen->search(MULTI_CHAR('fuku_g_2'));
    J2DPane* p0 = screen->search(MULTI_CHAR('fuku_00'));
    J2DPane* p1 = screen->search(MULTI_CHAR('fuku_01'));
    J2DPane* p2 = screen->search(MULTI_CHAR('fuku_02'));

    J2DPane* ken_00 = screen->search(MULTI_CHAR('ken_00'));
    J2DPane* ken_01 = screen->search(MULTI_CHAR('ken_01'));
    J2DPane* tate_00 = screen->search(MULTI_CHAR('tate_00'));
    J2DPane* tate_01 = screen->search(MULTI_CHAR('tate_01'));

    J2DPane* panes[] = { ken_n0, ken_n1, ken_g0, ken_g1,
                         tate_n0, tate_n1, tate_g0, tate_g1,
                         fuku_n0, fuku_n1, fuku_n2, p0, p1, p2,
                         fuku_g0, fuku_g1, fuku_g2, heart_n,
                         ken_00, ken_01, tate_00, tate_01 };
    int hidden = 0, found = 0;
    for (J2DPane* vp : panes) {
        if (vp == nullptr) continue;
        found++;

        vp->hide();
        vp->translate(0.0f, -1000.0f);
        hidden++;
    }

    for (u8 vy = 0; vy <= 2; vy++) {
        for (u8 vx = 3; vx <= 6; vx++) {
            collect2D->field_0x22d[vx][vy] = 0;

            collect2D->field_0x184[vx][vy] = 0;
            collect2D->field_0x1d8[vx][vy] = 0;
        }
    }

    static int s_blankLogs = 0;
    if (s_blankLogs < 3) {
        log_collect_info("blank layout: hid %d/%d vanilla panes, cursor flags zeroed", hidden, found);
        s_blankLogs++;
    }
}

void apply_collect_shifts(dMenu_Collect2D_c* collect2D) {
    if (!collect2D || !collect2D->mpScreen) return;
    J2DScreen* screen = collect2D->mpScreen;
    update_screen_bases(screen, collect2D->mpHeap);

    J2DPane* ken_n0 = screen->search(MULTI_CHAR('ken_n0'));
    J2DPane* ken_n1 = screen->search(MULTI_CHAR('ken_n1'));
    J2DPane* tate_n0 = screen->search(MULTI_CHAR('tate_n0'));
    J2DPane* tate_n1 = screen->search(MULTI_CHAR('tate_n1'));
    J2DPane* fuku_n0 = screen->search(MULTI_CHAR('fuku_n0'));
    J2DPane* fuku_n1 = screen->search(MULTI_CHAR('fuku_n1'));
    J2DPane* fuku_n2 = screen->search(MULTI_CHAR('fuku_n2'));
    J2DPane* heart_n = screen->search(MULTI_CHAR('heart_n'));
    J2DPane* kamen_n = screen->search(MULTI_CHAR('kamen_n'));
    J2DPane* modelbgn = screen->search(MULTI_CHAR('modelbgn'));

    J2DPane* ken_g0 = screen->search(MULTI_CHAR('ken_g_0'));
    J2DPane* ken_gm = slot_frame(4, 0);
    J2DPane* ken_g1 = screen->search(MULTI_CHAR('ken_g_1'));

    J2DPane* tate_g0 = screen->search(MULTI_CHAR('tate_g_0'));
    J2DPane* tate_gm = slot_frame(4, 1);
    J2DPane* tate_g1 = screen->search(MULTI_CHAR('tate_g_1'));

    J2DPane* fuku_go = slot_frame(3, 2);
    J2DPane* fuku_g0 = screen->search(MULTI_CHAR('fuku_g_0'));
    J2DPane* fuku_g1 = screen->search(MULTI_CHAR('fuku_g_1'));
    J2DPane* fuku_g2 = screen->search(MULTI_CHAR('fuku_g_2'));

    f32 dx = s_col_dx + 5.0f;

    f32 baseX = s_ken_n0_origX + collection_page_grid_dx();
    f32 swordFrameBaseX = baseX - 24.5f;
    f32 shieldFrameBaseX = baseX - 24.5f;
    f32 clothesFrameBaseX = baseX - 24.5f;

    f32 rowShift = (cl_column_occupied(1, 1) || cl_column_occupied(3, 1)) ? 0.0f : -dx;

    f32 shieldShift = cl_column_occupied(2, 1) ? 0.0f : -dx;

    const bool swordSwapped = cl_item23_swapped(1);
    const f32 kenMidCol = swordSwapped ? 2.0f : 1.0f;
    const f32 kenN1Col  = swordSwapped ? 1.0f : 2.0f;
    set_pane_pos(ken_n0, baseX, s_ken_n0_origY);
    set_pane_pos(slot_icon(4, 0), baseX + kenMidCol * dx + rowShift, s_ken_n0_origY);
    set_pane_pos(ken_n1, baseX + kenN1Col * dx + rowShift, s_ken_n0_origY);

    set_pane_pos(ken_g0, swordFrameBaseX, s_ken_g0_origY);
    set_pane_pos(ken_gm, swordFrameBaseX + kenMidCol * dx + rowShift, s_ken_g0_origY);
    set_pane_pos(ken_g1, swordFrameBaseX + kenN1Col * dx + rowShift, s_ken_g0_origY);

    const bool shieldSwapped = cl_item23_swapped(2);
    const f32 tateMidCol = shieldSwapped ? 2.0f : 1.0f;
    const f32 tateN1Col  = shieldSwapped ? 1.0f : 2.0f;
    set_pane_pos(tate_n0, baseX, s_tate_n0_origY);
    set_pane_pos(slot_icon(4, 1), baseX + tateMidCol * dx + shieldShift, s_tate_n0_origY);
    set_pane_pos(tate_n1, baseX + tateN1Col * dx + shieldShift, s_tate_n0_origY);

    set_pane_pos(tate_g0, shieldFrameBaseX, s_tate_g0_origY);
    set_pane_pos(tate_gm, shieldFrameBaseX + tateMidCol * dx + shieldShift, s_tate_g0_origY);
    set_pane_pos(tate_g1, shieldFrameBaseX + tateN1Col * dx + shieldShift, s_tate_g0_origY);

    set_pane_pos(slot_icon(3, 2), baseX, s_fuku_n0_origY);
    set_pane_pos(fuku_n0, baseX + dx + rowShift, s_fuku_n0_origY);
    set_pane_pos(fuku_n1, baseX + 2.0f * dx + rowShift, s_fuku_n0_origY);
    set_pane_pos(fuku_n2, baseX + 3.0f * dx + rowShift, s_fuku_n0_origY);

    set_pane_pos(fuku_go, clothesFrameBaseX, s_fuku_g0_origY);
    set_pane_pos(fuku_g0, clothesFrameBaseX + dx + rowShift, s_fuku_g0_origY);
    set_pane_pos(fuku_g1, clothesFrameBaseX + 2.0f * dx + rowShift, s_fuku_g0_origY);
    set_pane_pos(fuku_g2, clothesFrameBaseX + 3.0f * dx + rowShift, s_fuku_g0_origY);

    f32 mirrorShiftX = 40.0f;

    set_pane_pos(heart_n, baseX + 3.0f * dx + rowShift, s_heart_n_origY);
    set_pane_pos(kamen_n, s_kamen_n_origX + mirrorShiftX, s_kamen_n_origY);
    set_pane_pos(modelbgn, s_modelbgn_origX + mirrorShiftX - mirrorShiftX / 2.0f, s_modelbgn_origY);

    if (slot_icon(4, 0)) slot_icon(4, 0)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
    if (slot_frame(4, 0)) slot_frame(4, 0)->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
    if (slot_iconPic(4, 0)) {
        slot_iconPic(4, 0)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
        slot_iconPic(4, 0)->translate(0.0f, 0.0f);
    }

    if (slot_icon(4, 1)) slot_icon(4, 1)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
    if (slot_frame(4, 1)) slot_frame(4, 1)->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
    if (slot_iconPic(4, 1)) {
        slot_iconPic(4, 1)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
        slot_iconPic(4, 1)->translate(0.0f, 0.0f);
    }

    if (slot_icon(3, 2)) slot_icon(3, 2)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
    if (slot_frame(3, 2)) slot_frame(3, 2)->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
    if (slot_iconPic(3, 2)) {
        slot_iconPic(3, 2)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
        slot_iconPic(3, 2)->translate(0.0f, 0.0f);
        ResTIMG* ordonClothesTex = get_ordon_clothes_texture();
        if (ordonClothesTex) {
            slot_iconPic(3, 2)->changeTexture(ordonClothesTex, 0);
        }
    }

    if (slot_icon(3, 2)) slot_icon(3, 2)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
    if (slot_frame(3, 2)) slot_frame(3, 2)->mBounds.set(-23.5f, -23.5f, 23.5f, 23.5f);
    if (slot_iconPic(3, 2)) {
        slot_iconPic(3, 2)->mBounds.set(-22.5f, -22.5f, 22.5f, 22.5f);
        slot_iconPic(3, 2)->translate(0.0f, 0.0f);
        ResTIMG* ordonClothesTex = get_ordon_clothes_texture();
        if (ordonClothesTex) {
            slot_iconPic(3, 2)->changeTexture(ordonClothesTex, 0);
        }
    }

    J2DPane* tunagi00 = screen->search(MULTI_CHAR('tunagi00'));
    J2DPane* tunagi01 = screen->search(MULTI_CHAR('tunagi01'));
    J2DPane* tunagi_k2 = screen->search(MULTI_CHAR('tuna_k2'));

    J2DPane* tunagi04 = screen->search(MULTI_CHAR('tunagi04'));
    J2DPane* tunagi03 = screen->search(MULTI_CHAR('tunagi03'));
    J2DPane* tunagi_t2 = screen->search(MULTI_CHAR('tuna_t2'));

    J2DPane* tunagi07 = screen->search(MULTI_CHAR('tunagi07'));
    J2DPane* tunagi06 = screen->search(MULTI_CHAR('tunagi06'));
    J2DPane* tunagi08 = screen->search(MULTI_CHAR('tunagi08'));
    J2DPane* tunagi_f3 = screen->search(MULTI_CHAR('tuna_f3'));

    set_pane_pos(tunagi00, swordFrameBaseX - 0.5f * dx + rowShift, -44.0f);
    set_pane_pos(tunagi01, swordFrameBaseX + 0.5f * dx + rowShift, -44.0f);
    set_pane_pos(tunagi_k2, swordFrameBaseX + 1.5f * dx + rowShift, -44.0f);

    set_pane_pos(tunagi04, shieldFrameBaseX - 0.5f * dx + shieldShift, 13.0f);
    set_pane_pos(tunagi03, shieldFrameBaseX + 0.5f * dx + shieldShift, 13.0f);
    set_pane_pos(tunagi_t2, shieldFrameBaseX + 1.5f * dx + shieldShift, 13.0f);

    set_pane_pos(tunagi07, clothesFrameBaseX - 0.5f * dx + rowShift, 70.0f);
    set_pane_pos(tunagi06, clothesFrameBaseX + 0.5f * dx + rowShift, 70.0f);
    if (tunagi08) {
        if (tunagi06) static_cast<CustomPicture*>(tunagi08)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi06));
        tunagi08->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
        set_pane_pos(tunagi08, clothesFrameBaseX + 1.5f * dx + rowShift, 70.0f);
    }
    set_pane_pos(tunagi_f3, clothesFrameBaseX + 2.5f * dx + rowShift, 70.0f);

    if (s_picTunagiKen2 && tunagi01) {
        static_cast<CustomPicture*>(s_picTunagiKen2)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi01));
        s_picTunagiKen2->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
    }
    if (s_picTunagiTate2 && tunagi03) {
        static_cast<CustomPicture*>(s_picTunagiTate2)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi03));
        s_picTunagiTate2->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
    }
    if (s_picTunagiFuku3 && tunagi06) {
        static_cast<CustomPicture*>(s_picTunagiFuku3)->copyVisualsFrom(static_cast<J2DPicture*>(tunagi06));
        s_picTunagiFuku3->mBounds.set(-6.0f, -18.0f, 6.0f, 18.0f);
    }

    bool hasWoodSword = cl_column_claimed(1, 1) && cl_vanilla_slot_unlocked(3, 0);
    bool hasOrdonSword = is_collect_item_unlocked(4, 0);

    const u8 eqSwordForMaster = custom_equip_active(CE_SWORD) ? dItemNo_NONE_e : dComIfGs_getSelectEquipSword();
    bool hasMasterSword = dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e) ||
                           dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e) ||
                           (eqSwordForMaster == dItemNo_MASTER_SWORD_e) ||
                           (eqSwordForMaster == dItemNo_LIGHT_SWORD_e);
    bool hasHeart = (dComIfGs_getMaxLife() > 15);

    bool hasOrdonShield = cl_column_claimed(2, 1) && cl_vanilla_slot_unlocked(3, 1);
    bool hasWoodShield = is_collect_item_unlocked(4, 1);
    bool hasHylianShield = is_collect_item_unlocked(5, 1);

    bool hasKokiriClothes = is_collect_item_unlocked(4, 2);
    bool hasZoraArmor = is_collect_item_unlocked(5, 2);
    bool hasMagicArmor = is_collect_item_unlocked(6, 2);

    u8 starterSlot = (cl_column_occupied(1, 1) || cl_column_occupied(3, 1)) ? 1 : 0;
    collect2D->field_0x22d[3][0] = starterSlot;

    const u8 midCell = cl_vanilla_layout_hidden() ? 0 : 1;
    collect2D->field_0x22d[4][0] = midCell;
    collect2D->field_0x22d[5][0] = 1;
    collect2D->field_0x22d[6][0] = 1;

    collect2D->field_0x22d[3][1] = (cl_column_occupied(1, 1) || cl_column_occupied(3, 1)) ? 1 : 0;
    collect2D->field_0x22d[4][1] = midCell;
    collect2D->field_0x22d[5][1] = 1;
    collect2D->field_0x22d[6][1] = 0;

    collect2D->field_0x22d[3][2] = starterSlot;
    collect2D->field_0x22d[4][2] = midCell;
    collect2D->field_0x22d[5][2] = 1;
    collect2D->field_0x22d[6][2] = 1;

    if (!cl_column_occupied(1, 1) && collect2D->mCursorX == 3 &&
        collect2D->mCursorY <= 2) {
        collect2D->mCursorX = 4;
    }

    if (tunagi00) { if (cl_column_occupied(1, 1)) tunagi00->show(); else tunagi00->hide(); }
    if (tunagi01) tunagi01->show();
    if (tunagi_k2) tunagi_k2->show();

    if (tunagi04) { if (cl_column_occupied(2, 1)) tunagi04->show(); else tunagi04->hide(); }
    if (tunagi03) tunagi03->show();
    if (tunagi_t2) tunagi_t2->show();

    if (tunagi07) { if (cl_column_occupied(3, 1)) tunagi07->show(); else tunagi07->hide(); }
    if (tunagi06) tunagi06->show();
    if (tunagi08) tunagi08->show();
    if (tunagi_f3) tunagi_f3->show();

    if (ken_g0) { if (cl_column_claimed(1, 1)) ken_g0->show(); else ken_g0->hide(); }
    if (ken_gm) ken_gm->show();
    if (ken_g1) ken_g1->show();

    if (tate_g0) { if (ordon_shield_slot_present()) tate_g0->show(); else tate_g0->hide(); }
    if (tate_gm) tate_gm->show();
    if (tate_g1) tate_g1->show();

    if (fuku_go) { if (cl_column_claimed(3, 1)) fuku_go->show(); else fuku_go->hide(); }
    if (fuku_g0) fuku_g0->show();
    if (fuku_g1) fuku_g1->show();
    if (fuku_g2) fuku_g2->show();

    if (cl_vanilla_layout_hidden()) {
        J2DPane* const kBlankHide[] = {
            ken_gm, ken_g1,
            tate_gm, tate_g1,
            fuku_g1, fuku_g2,
            tunagi01, tunagi_k2,
            tunagi04, tunagi03, tunagi_t2,
            tunagi07, tunagi06, tunagi08, tunagi_f3,
        };
        for (J2DPane* p : kBlankHide) {
            if (p != nullptr) p->hide();
        }
    }

    if (ken_n0 && slot_icon(4, 0)) slot_icon(4, 0)->scale(ken_n0->getScaleX(), ken_n0->getScaleY());
    if (tate_n0 && slot_icon(4, 1)) slot_icon(4, 1)->scale(tate_n0->getScaleX(), tate_n0->getScaleY());
    if (fuku_n0 && slot_icon(3, 2)) slot_icon(3, 2)->scale(fuku_n0->getScaleX(), fuku_n0->getScaleY());

    J2DPane* ken_00 = screen->search(MULTI_CHAR('ken_00'));
    J2DPane* ken_01 = screen->search(MULTI_CHAR('ken_01'));
    if (ken_n0) { if (hasWoodSword) ken_n0->show(); else ken_n0->hide(); }
    if (ken_00) { if (hasWoodSword) { ken_00->show(); ken_00->translate(0.0f, 0.0f); } else ken_00->hide(); }
    if (ken_01) ken_01->hide();

    if (slot_icon(4, 0)) { if (hasOrdonSword) slot_icon(4, 0)->show(); else slot_icon(4, 0)->hide(); }
    if (slot_iconPic(4, 0)) { if (hasOrdonSword) { slot_iconPic(4, 0)->show(); slot_iconPic(4, 0)->translate(0.0f, 0.0f); } else slot_iconPic(4, 0)->hide(); }

    if (ken_n1) { if (hasMasterSword && (swordSwapped || !cl_column_occupied(1, 3))) ken_n1->show(); else ken_n1->hide(); }

    if (heart_n) heart_n->show();

    J2DPane* tate_00 = screen->search(MULTI_CHAR('tate_00'));
    J2DPane* tate_01 = screen->search(MULTI_CHAR('tate_01'));
    if (tate_n0) { if (hasOrdonShield) tate_n0->show(); else tate_n0->hide(); }
    if (tate_01) { if (hasOrdonShield) { tate_01->show(); tate_01->translate(0.0f, 0.0f); } else tate_01->hide(); }
    if (tate_00) tate_00->hide();

    if (slot_icon(4, 1)) { if (hasWoodShield) slot_icon(4, 1)->show(); else slot_icon(4, 1)->hide(); }
    if (slot_iconPic(4, 1)) { if (hasWoodShield) { slot_iconPic(4, 1)->show(); slot_iconPic(4, 1)->translate(0.0f, 0.0f); } else slot_iconPic(4, 1)->hide(); }

    if (tate_n1) { if (hasHylianShield && (shieldSwapped || !cl_column_occupied(2, 3))) tate_n1->show(); else tate_n1->hide(); }

    if (slot_icon(3, 2)) { if (cl_column_claimed(3, 1)) slot_icon(3, 2)->show(); else slot_icon(3, 2)->hide(); }
    if (slot_iconPic(3, 2)) { if (cl_column_claimed(3, 1)) slot_iconPic(3, 2)->show(); else slot_iconPic(3, 2)->hide(); }

    bool showKokiriClothes = hasKokiriClothes && !cl_column_occupied(3, 2);
    if (fuku_n0) { if (showKokiriClothes) fuku_n0->show(); else fuku_n0->hide(); }
    J2DPane* p0 = screen->search(MULTI_CHAR('fuku_00'));
    if (p0) { if (showKokiriClothes) p0->show(); else p0->hide(); }

    bool showZoraArmor = hasZoraArmor && !cl_column_occupied(3, 3);
    if (fuku_n1) { if (showZoraArmor) fuku_n1->show(); else fuku_n1->hide(); }
    J2DPane* p1 = screen->search(MULTI_CHAR('fuku_01'));
    if (p1) { if (showZoraArmor) p1->show(); else p1->hide(); }

    bool showMagicArmor = hasMagicArmor && !cl_column_occupied(3, 4);
    if (fuku_n2) { if (showMagicArmor) fuku_n2->show(); else fuku_n2->hide(); }
    J2DPane* p2 = screen->search(MULTI_CHAR('fuku_02'));
    if (p2) { if (showMagicArmor) p2->show(); else p2->hide(); }

    cl_apply_blank_layout(collect2D);

    layout_managed_slots(collect2D);
    configure_managed_slots(collect2D);

    collection_page_apply(collect2D);

    if (cl_vanilla_layout_hidden()) {
        bool cursorOk = false;
        for (int i = 0; i < slot_count() && !cursorOk; i++) {
            const SlotSpec* sl = slot_get(i);
            cursorOk = sl->autoLayout.on && sl->x == collect2D->mCursorX
                   && sl->y == collect2D->mCursorY && is_collect_item_unlocked(sl->x, sl->y);
        }
        if (!cursorOk) {
            for (int i = 0; i < slot_count(); i++) {
                const SlotSpec* sl = slot_get(i);
                if (sl->autoLayout.on && sl->x >= 3 && is_collect_item_unlocked(sl->x, sl->y)) {
                    collect2D->mCursorX = sl->x;
                    collect2D->mCursorY = sl->y;
                    break;
                }
            }
        }
    }

    update_frame_highlights(collect2D);

    cl_apply_slot_moves(screen, baseX, dx);

    cl_suppress_removed_cells(collect2D);
}

void update_frame_highlights(dMenu_Collect2D_c* collect2D) {
    if (!collect2D || !collect2D->mpScreen) return;
    J2DScreen* screen = collect2D->mpScreen;

    u8 currentSword = dComIfGs_getSelectEquipSword();
    u8 currentShield = dComIfGs_getSelectEquipShield();
    u8 currentClothes = dComIfGs_getSelectEquipClothes();

    if (custom_equip_active(CE_SWORD))  currentSword   = 0xFF;
    if (custom_equip_active(CE_SHIELD)) currentShield  = 0xFF;
    if (custom_equip_active(CE_TUNIC))  currentClothes = 0xFF;

    J2DPicture* ken_g0 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('ken_g_0')));
    if (ken_g0) {
        bool eq = (currentSword == dItemNo_WOOD_STICK_e);
        ken_g0->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                              eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* picKenMidFrame = slot_frame(4, 0);
    if (picKenMidFrame) {
        bool eq = (currentSword == dItemNo_SWORD_e);
        picKenMidFrame->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                      eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* ken_g1 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('ken_g_1')));
    if (ken_g1) {
        bool eq = (currentSword == dItemNo_MASTER_SWORD_e || currentSword == dItemNo_LIGHT_SWORD_e);
        ken_g1->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                              eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* tate_g0 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tate_g_0')));
    if (tate_g0) {
        bool eq = (currentShield == dItemNo_WOOD_SHIELD_e);
        tate_g0->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* picTateMidFrame = slot_frame(4, 1);
    if (picTateMidFrame) {
        bool eq = (currentShield == dItemNo_SHIELD_e);
        picTateMidFrame->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                       eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* tate_g1 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tate_g_1')));
    if (tate_g1) {
        bool eq = (currentShield == dItemNo_HYLIA_SHIELD_e);
        tate_g1->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* picFukuStartFrame = slot_frame(3, 2);
    if (picFukuStartFrame) {
        bool eq = (currentClothes == dItemNo_WEAR_CASUAL_e);
        picFukuStartFrame->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                         eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* fuku_g0 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('fuku_g_0')));
    if (fuku_g0) {
        bool eq = (currentClothes == dItemNo_WEAR_KOKIRI_e);
        fuku_g0->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* fuku_g1 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('fuku_g_1')));
    if (fuku_g1) {
        bool eq = (currentClothes == dItemNo_WEAR_ZORA_e);
        fuku_g1->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* fuku_g2 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('fuku_g_2')));
    if (fuku_g2) {
        bool eq = (currentClothes == dItemNo_ARMOR_e);
        fuku_g2->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
    }

    J2DPicture* io = slot_iconPic(3, 2);
    if (io) {
        io->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        io->setAlpha(255);
    }
    J2DPicture* t00 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tate_00')));
    if (t00) {
        t00->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        t00->setAlpha(255);
    }
    J2DPicture* t01 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('tate_01')));
    if (t01) {
        t01->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        t01->setAlpha(255);
    }
    J2DPicture* k00 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('ken_00')));
    if (k00) {
        k00->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        k00->setAlpha(255);
    }
    J2DPicture* k01 = static_cast<J2DPicture*>(screen->search(MULTI_CHAR('ken_01')));
    if (k01) {
        k01->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        k01->setAlpha(255);
    }
    J2DPicture* tim = slot_iconPic(4, 1);
    if (tim) {
        tim->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        tim->setAlpha(255);
    }
    J2DPicture* kim = slot_iconPic(4, 0);
    if (kim) {
        kim->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
        kim->setAlpha(255);
    }

    for (int i = 0; i < slot_count(); i++) {
        const SlotSpec* s = slot_get(i);
        if (!s->autoLayout.on) continue;
        if (s->frame) {
            bool eq = is_collect_item_equipped(s->x, s->y);
            s->frame->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                eq ? JUtility::TColor(255, 255, 0, 255) : JUtility::TColor(107, 107, 107, 255));
        }
        if (s->iconPic) {
            s->iconPic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
            s->iconPic->setAlpha(255);
        }
    }
}

void on_menu_collect_2d_create_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    s_currentCollect2D = collect2D;
    custom_equip_menu_doll_begin();
    if (!is_collection_menu_enabled()) return;
    collection_page_reset();
    apply_collect_shifts(collect2D);
}

HookAction on_menu_collect_2d_delete_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_end();
    s_currentCollect2D = nullptr;
    s_picTunagiKen2 = nullptr;
    s_picTunagiTate2 = nullptr;
    s_picTunagiFuku3 = nullptr;
    for (int ci = 0; ci < 6; ci++) s_customConnectors[ci] = nullptr;
    s_customConnectorCount = 0;
    for (int ri = 0; ri < 3; ri++) {
        s_customConnectorParent[ri] = nullptr;
        s_customConnectorTemplate[ri] = nullptr;
    }
    s_capturedScreen = nullptr;
    s_cachedScreen = nullptr;
    slot_registry_clear();
    collection_page_teardown();
    return HOOK_CONTINUE;
}

HookAction on_screen_set_pre(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (collect2D && collect2D->mpScreen) {
        update_screen_bases(collect2D->mpScreen, collect2D->mpHeap);
        apply_collect_shifts(collect2D);
    }
    return HOOK_CONTINUE;
}

void on_screen_set_post(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return;

    collect2D->field_0x184[3][0] = 0x1a4;
    collect2D->field_0x1d8[3][0] = 0x2a4;
    collect2D->field_0x184[4][0] = 0x18d;
    collect2D->field_0x1d8[4][0] = 0x28d;
    collect2D->field_0x184[5][0] = 0x18e;
    collect2D->field_0x1d8[5][0] = 0x28e;
    collect2D->field_0x184[6][0] = 0x186;
    collect2D->field_0x1d8[6][0] = 0x286;

    collect2D->field_0x184[3][1] = 0x190;
    collect2D->field_0x1d8[3][1] = 0x290;
    collect2D->field_0x184[4][1] = 0x18f;
    collect2D->field_0x1d8[4][1] = 0x28f;
    collect2D->field_0x184[5][1] = 0x191;
    collect2D->field_0x1d8[5][1] = 0x291;

    collect2D->field_0x184[3][2] = 0x193;
    collect2D->field_0x1d8[3][2] = 0x293;
    collect2D->field_0x184[4][2] = 0x194;
    collect2D->field_0x1d8[4][2] = 0x294;
    collect2D->field_0x184[5][2] = 0x196;
    collect2D->field_0x1d8[5][2] = 0x296;
    collect2D->field_0x184[6][2] = 0x195;
    collect2D->field_0x1d8[6][2] = 0x295;

    apply_collect_shifts(collect2D);
    update_frame_highlights(collect2D);

    if (collect2D->mpScreen) {
        auto setupSelPm = [&](int x, int y, J2DPane* pane) {
            if (!pane) {

                collect2D->mpSelPm[x][y] = nullptr;
                return;
            }
            if (!collect2D->mpSelPm[x][y]) {
                JKRHeap* oldHeap = collect2D->mpHeap ? mDoExt_setCurrentHeap(collect2D->mpHeap) : nullptr;
                CPaneMgr* pm = JKR_NEW CPaneMgr();
                if (pm) {
                    pm->mFlags = 0;
                    pm->initiate(pane, (JKRExpHeap*)collect2D->mpHeap);
                    collect2D->mpSelPm[x][y] = pm;
                }
                if (oldHeap) mDoExt_setCurrentHeap(oldHeap);
            } else {
                collect2D->mpSelPm[x][y]->mPane = pane;
                collect2D->mpSelPm[x][y]->reinit();
            }
        };

        setupSelPm(3, 0, cl_column_claimed(1, 1)
                             ? collect2D->mpScreen->search(MULTI_CHAR('ken_n0'))
                             : nullptr);

        setupSelPm(4, 0, cl_item23_swapped(1)
                             ? collect2D->mpScreen->search(MULTI_CHAR('ken_n1'))
                             : slot_icon(4, 0));
        setupSelPm(5, 0, collect2D->mpScreen->search(MULTI_CHAR('ken_n1')));
        setupSelPm(6, 0, collect2D->mpScreen->search(MULTI_CHAR('heart_n')));

        setupSelPm(3, 1, cl_column_claimed(2, 1)
                             ? collect2D->mpScreen->search(MULTI_CHAR('tate_n0'))
                             : nullptr);
        setupSelPm(4, 1, slot_icon(4, 1));
        setupSelPm(5, 1, collect2D->mpScreen->search(MULTI_CHAR('tate_n1')));

        setupSelPm(3, 2, cl_column_claimed(3, 1)
                             ? (slot_icon(3, 2) ? slot_icon(3, 2) : collect2D->mpScreen->search(MULTI_CHAR('fuku_ord')))
                             : nullptr);
        setupSelPm(4, 2, collect2D->mpScreen->search(MULTI_CHAR('fuku_n0')));
        setupSelPm(5, 2, collect2D->mpScreen->search(MULTI_CHAR('fuku_n1')));
        setupSelPm(6, 2, collect2D->mpScreen->search(MULTI_CHAR('fuku_n2')));

        setupSelPm(0, 5, collect2D->mpScreen->search(MULTI_CHAR('save_n')));
        setupSelPm(1, 5, collect2D->mpScreen->search(MULTI_CHAR('option_n')));

        for (int i = 0; i < slot_count(); i++) {
            const SlotSpec* s = slot_get(i);
            if (s->autoLayout.on && s->icon && s->x < 7 && s->y < 6) {
                setupSelPm(s->x, s->y, s->icon);
            }
        }
    }
}

void on_menu_collect_wide_post(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    apply_collect_shifts(collect2D);
    update_frame_highlights(collect2D);
}

void cl_suppress_removed_cells(dMenu_Collect2D_c* collect2D) {
    if (collect2D == nullptr || collect2D->mpScreen == nullptr) return;

    for (int i = 0; i < cl_removed_cell_count(); i++) {
        CollectionSlot rc = cl_removed_cell_at(i);
        SlotCell c = grid_cell(rc.row, rc.item);
        if (!slot_cell_set(c)) continue;

        collect2D->field_0x22d[c.x][c.y] = 0;
        collect2D->field_0x184[c.x][c.y] = 0;
        collect2D->field_0x1d8[c.x][c.y] = 0;

        u32 nTag = 0, picTag = 0, gTag = 0;
        if (rc.row == 1) {
            nTag = MULTI_CHAR('ken_n0'); picTag = MULTI_CHAR('ken_00'); gTag = MULTI_CHAR('ken_g_0');
        } else if (rc.row == 2) {
            nTag = MULTI_CHAR('tate_n0'); picTag = MULTI_CHAR('tate_00'); gTag = MULTI_CHAR('tate_g_0');
        } else if (rc.row == 3) {
            nTag = MULTI_CHAR('fuku_n0'); picTag = MULTI_CHAR('fuku_00'); gTag = MULTI_CHAR('fuku_g_0');
        }
        J2DPane* n = collect2D->mpScreen->search(nTag);
        J2DPane* pic = collect2D->mpScreen->search(picTag);
        J2DPane* g = collect2D->mpScreen->search(gTag);

        if (n != nullptr) { n->hide(); n->translate(0.0f, -1000.0f); }
        if (pic != nullptr) { pic->hide(); pic->translate(0.0f, -1000.0f); }
        if (g != nullptr) { g->hide(); g->translate(0.0f, -1000.0f); }

        if (collect2D->mCursorX == c.x && collect2D->mCursorY == c.y) {
            for (u8 ty = 0; ty <= 2; ty++) {
                bool moved = false;
                for (u8 tx = 3; tx <= 6 && !moved; tx++) {
                    if (collect2D->field_0x22d[tx][ty] != 0) {
                        collect2D->mCursorX = tx;
                        collect2D->mCursorY = ty;
                        moved = true;
                    }
                }
            }
        }
    }
}

void on_menu_collect_2d_move_post(ModContext*, void* args, void*, void*) {
    if (!is_collection_menu_enabled() || !args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return;
    apply_collect_shifts(collect2D);
    update_frame_highlights(collect2D);
}

void on_mw_execute_post(ModContext*, void*, void*, void*) {
    if (s_needReloadCollect) {
        s_needReloadCollect = false;
        dMw_c* mw = dMeter2Info_getMenuWindowClass();
        if (mw && s_currentCollect2D != nullptr && mw->isPauseWindow()) {
            mw->dMw_collect_delete(true);
            mw->dMw_collect_create();
        }
    }
}
