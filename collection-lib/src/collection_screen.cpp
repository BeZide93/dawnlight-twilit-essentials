#include "collection_internal.hpp"

#include "JSystem/J2DGraph/J2DMaterial.h"
#include "JSystem/J2DGraph/J2DMaterialFactory.h"
#include "JSystem/J2DGraph/J2DPictureEx.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JSupport/JSUMemoryStream.h"

#include <algorithm>

// Puts the layout (collection_layout.cpp) onto dMenu_Collect2D_c.
//
// Native items keep their native panes, cells and code paths; the library only moves them.
// Custom items get panes cloned from the native ones of their row, and every column that
// is not native gets its cell in the screen's tables filled in. Positions are taken from the
// screen's own panes, so patched layouts (.blo overlays) carry over.

DEFINE_HOOK(&dMenu_Collect2D_c::_create, MenuCollect2DCreateHook);
DEFINE_HOOK(&dMenu_Collect2D_c::_delete, MenuCollect2DDeleteHook);
DEFINE_HOOK(&dMenu_Collect2D_c::screenSet, ScreenSetHook);
DEFINE_HOOK(&dMenu_Collect2D_c::menuCollectWide, MenuCollectWideHook);
DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorSword, SetEquipFrameColorSwordHook);
DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorShield, SetEquipFrameColorShieldHook);
DEFINE_HOOK(&dMenu_Collect2D_c::setEquipItemFrameColorClothes, SetEquipFrameColorClothesHook);
DEFINE_HOOK(&J2DScreen::draw, ClScreenDrawHook);

namespace {

struct ColPanes {
    J2DPane*       icon = nullptr;    // native icon pane, or the library's container
    J2DPicture*    pic = nullptr;     // custom slots: the icon picture
    J2DPicture*    frame = nullptr;   // native frame (native cell) or a clone
    const ResTIMG* tex = nullptr;     // texture currently shown by pic
    J2DPicture*    flourish[2] = {};
};

struct Pos {
    f32 x = 0.0f, y = 0.0f;
};

struct ScreenState {
    dMenu_Collect2D_c* collect = nullptr;
    J2DScreen*         screen = nullptr;
    bool               built = false;

    // Templates of a row: column-1 icon pane and its picture.
    J2DPane*    iconTmpl[kClRows] = {};
    J2DPicture* picTmpl[kClRows] = {};
    J2DPicture* frameTmpl = nullptr;
    J2DPicture* connTmpl = nullptr;
    // A native icon pane: menuCollectWide() keeps its scale current (Wii menu scaling).
    J2DPane*    refIcon = nullptr;

    J2DPane*    nativeIcon[kClRows][3] = {};
    J2DPicture* nativeFrame[kClRows][3] = {};
    Pos         nativeIconPos[kClRows][3];
    Pos         nativeFramePos[kClRows][3];

    Pos iconCol1[kClRows];      // column 1 icon position
    Pos frameCol1[kClRows];     // column 1 frame position
    Pos leftConnPos[kClRows];   // connector left of column 1
    Pos gap1ConnPos[kClRows];   // connector between columns 1 and 2
    f32 iconDx = 59.0f;
    f32 frameDx = 59.0f;

    ColPanes    cols[kClRows][kClMaxCols + 1];
    J2DPicture* leftConn[kClRows] = {};
    J2DPicture* gapConn[kClRows][kClMaxCols] = {};     // [row][g]: joins columns g and g+1
    bool        gapNative[kClRows][kClMaxCols] = {};
    Pos         gapNativePos[kClRows][kClMaxCols];

    J2DPane* heart = nullptr;
    J2DPane* kamen = nullptr;
    J2DPane* modelbgn = nullptr;
    Pos      heartPos, kamenPos, modelbgnPos;
    bool     heartOnMain = true;
    bool     maskOnMain = true;

    J2DPane* hdRoot = nullptr;

    // The native tables as screenSet() left them, before the library changed any cell.
    bool snap = false;
    u8   native22d[7][6] = {};
    u16  native184[7][6] = {};
    u16  native1d8[7][6] = {};
};

ScreenState s;

const JUtility::TColor kFrameBlack(0, 0, 0, 0);
const JUtility::TColor kFrameOn(255, 255, 0, 255);
const JUtility::TColor kFrameOff(107, 107, 107, 255);

Pos pane_pos(J2DPane* p) { return Pos{p->getTranslateX(), p->getTranslateY()}; }

J2DPane* find(u64 tag) { return s.screen->search(tag); }

J2DPicture* find_pic(u64 tag) { return static_cast<J2DPicture*>(s.screen->search(tag)); }

// Same local rectangle, anchor and position as `tmpl`. The J2DPane constructors take the
// bounds as a rectangle in parent space and move the origin to its top-left corner, so
// the template's bounds must be set again afterwards.
void copy_geometry(J2DPane* pane, J2DPane* tmpl) {
    pane->mBounds = tmpl->mBounds;
    pane->setBasePosition(static_cast<J2DBasePosition>(tmpl->mBasePosition));
    pane->scale(tmpl->getScaleX(), tmpl->getScaleY());
    pane->translate(tmpl->getTranslateX(), tmpl->getTranslateY());
}

// J2DPicture keeps its texture coordinates protected. A pointer to a base member formed
// through a derived class works on any J2DPicture.
struct PictureTexCoords : J2DPicture {
    using Member = JGeometry::TVec2<s16> (J2DPicture::*)[4];
    static Member member() { return &PictureTexCoords::field_0x10a; }
};

// ---------------------------------------------------------------------------
// Exact copies of layout pictures
//
// The layout's pictures are J2DPictureEx: their look comes from a material (TEV stages,
// colors, blending) that a plain J2DPicture only approximates. A copy is built the way
// J2DScreen built the original: from the picture's block in the .blo, with a material of
// its own created from the MAT1 block.
// ---------------------------------------------------------------------------

constexpr u32 kScreenFlags = 0x1020000;   // what dMenu_Collect2D_c::_create loads the screen with
const char* const kScreenBlo = "zelda_collect_soubi_screen.blo";

u32 be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3]; }
u16 be16(const u8* p) { return static_cast<u16>((p[0] << 8) | p[1]); }

u64 be64(const u8* p) { return (u64(be32(p)) << 32) | be32(p + 4); }

// J2DPictureEx deletes its material only when a private flag says it owns it. An explicit
// template instantiation may name a private member, which gives access to that flag.
template <u8 J2DPictureEx::*Member>
struct PictureExOwnsMaterial {
    friend u8 J2DPictureEx::*picture_ex_owns_material() { return Member; }
};
u8 J2DPictureEx::*picture_ex_owns_material();
template struct PictureExOwnsMaterial<&J2DPictureEx::field_0x190>;

struct Blo {
    const u8* data = nullptr;
    u32 size = 0;
    const u8* mat1 = nullptr;
};

Blo s_blo;

void find_blo() {
    s_blo = Blo{};
    JKRArchive* arc = dComIfGp_getCollectResArchive();
    const u8* data = arc != nullptr ? static_cast<const u8*>(arc->getResource(0, kScreenBlo)) : nullptr;
    if (data == nullptr) return;
    const u32 size = arc->getExpandedResSize(data);
    if (size < 0x20 || std::memcmp(data, "SCRNblo2", 8) != 0) return;
    s_blo.data = data;
    s_blo.size = size;
    for (u32 pos = 0x20; pos + 8 <= size;) {
        const u32 blockSize = be32(data + pos + 4);
        if (std::memcmp(data + pos, "MAT1", 4) == 0) s_blo.mat1 = data + pos;
        if (blockSize < 8) break;
        pos += blockSize;
    }
}

// The PIC2 block of the picture with this tag.
const u8* find_picture_block(u64 tag, u32* sizeOut) {
    for (u32 pos = 0x20; s_blo.data != nullptr && pos + 8 <= s_blo.size;) {
        const u8* block = s_blo.data + pos;
        const u32 blockSize = be32(block + 4);
        if (blockSize < 8 || pos + blockSize > s_blo.size) break;
        // PIC2 = header, the embedded pane info (header, 4 bytes, flags, tag), picture data.
        if (std::memcmp(block, "PIC2", 4) == 0 && blockSize > 0x20 && be64(block + 8 + 0x10) == tag) {
            *sizeOut = blockSize;
            return block;
        }
        pos += blockSize;
    }
    return nullptr;
}

J2DPictureEx* clone_picture_ex(J2DPane* parent, u64 tag, J2DPicture* tmpl) {
    if (s_blo.mat1 == nullptr) return nullptr;
    u32 blockSize = 0;
    const u8* block = find_picture_block(tmpl->mInfoTag, &blockSize);
    if (block == nullptr) return nullptr;

    // A copy of the block that points at material 0: the material made here.
    u8 copy[0x200];
    if (blockSize > sizeof(copy)) return nullptr;
    std::memcpy(copy, block, blockSize);
    const u32 paneSize = be32(copy + 8 + 4);
    if (8 + paneSize + 6 > blockSize) return nullptr;
    u8* matIndex = copy + 8 + paneSize + 4;   // J2DScrnBlockPictureParameter::field_0x4
    const u16 index = be16(matIndex);
    matIndex[0] = 0;
    matIndex[1] = 0;

    J2DMaterial* material = JKR_NEW J2DMaterial();
    if (material == nullptr) return nullptr;
    J2DMaterialFactory factory(*reinterpret_cast<const J2DMaterialBlock*>(s_blo.mat1));
    factory.create(material, index, kScreenFlags, s.screen->mTexRes, s.screen->mFontRes,
                   dComIfGp_getCollectResArchive());

    JSUMemoryInputStream stream(copy, static_cast<s32>(blockSize));
    J2DPictureEx* pic = JKR_NEW J2DPictureEx(parent, &stream, kScreenFlags, material);
    if (pic == nullptr) {
        JKR_DELETE(material);
        return nullptr;
    }
    pic->*picture_ex_owns_material() = 1;   // freed together with the pane
    pic->mInfoTag = tag;
    return pic;
}

// A picture that looks like `tmpl`, appended to `parent`, showing `tex`.
J2DPicture* clone_picture(J2DPane* parent, u64 tag, J2DPicture* tmpl, const ResTIMG* tex) {
    if (parent == nullptr || tmpl == nullptr || tex == nullptr) return nullptr;

    if (J2DPictureEx* ex = clone_picture_ex(parent, tag, tmpl)) {
        copy_geometry(ex, tmpl);
        if (tex != cl_pane_texture(tmpl)) ex->changeTexture(tex, 0);
        return ex;
    }
    static bool s_warned = false;
    if (!s_warned) {
        s_warned = true;
        log_collect_warn("collection-lib: %s has no usable block for pane 0x%llx, using approximate copies",
                         kScreenBlo, static_cast<unsigned long long>(tmpl->mInfoTag));
    }

    // Fallback: a plain picture with the template's colors.
    J2DPicture* pic = JKR_NEW J2DPicture(tag, tmpl->mBounds, tex, nullptr);
    if (pic == nullptr) return nullptr;
    parent->appendChild(pic);
    copy_geometry(pic, tmpl);
    pic->setBlackWhite(tmpl->getBlack(), tmpl->getWhite());
    // The layout shades frames and connectors with vertex colors, and connectors only map a
    // narrow strip of their texture.
    pic->setCornerColor(tmpl->corner(0), tmpl->corner(1), tmpl->corner(2), tmpl->corner(3));
    const PictureTexCoords::Member texCoords = PictureTexCoords::member();
    for (int i = 0; i < 4; i++) (pic->*texCoords)[i] = (tmpl->*texCoords)[i];
    pic->setAlpha(tmpl->getAlpha());
    return pic;
}

// An empty pane with the geometry of `tmpl`, next to it in the tree.
J2DPane* clone_container(J2DPane* tmpl, u64 tag) {
    J2DPane* parent = tmpl->getParentPane();
    if (parent == nullptr) return nullptr;
    J2DPane* pane = JKR_NEW J2DPane(parent, true, tag, tmpl->mBounds);
    if (pane == nullptr) return nullptr;
    copy_geometry(pane, tmpl);
    return pane;
}

u64 custom_icon_tag(int r, u8 x) { return cl_make_tag('c', 'l', 'i', 'c', static_cast<u8>(r), x); }

bool find_templates() {
    static const u64 kPicTmpl[kClRows] = {MULTI_CHAR('ken_01'), MULTI_CHAR('tate_00'), MULTI_CHAR('fuku_00')};
    static const u64 kLeftConn[kClRows] = {MULTI_CHAR('tunagi00'), MULTI_CHAR('tunagi04'), MULTI_CHAR('tunagi07')};
    static const u64 kGap1Conn[kClRows] = {MULTI_CHAR('tunagi01'), MULTI_CHAR('tunagi03'), MULTI_CHAR('tunagi06')};

    for (int r = 0; r < kClRows; r++) {
        for (int k = 1; k <= native_col_count(r); k++) {
            s.nativeIcon[r][k - 1] = find(native_cell(r, k).iconTag);
            s.nativeFrame[r][k - 1] = find_pic(native_cell(r, k).frameTag);
            if (s.nativeIcon[r][k - 1] == nullptr || s.nativeFrame[r][k - 1] == nullptr) return false;
            s.nativeIconPos[r][k - 1] = pane_pos(s.nativeIcon[r][k - 1]);
            s.nativeFramePos[r][k - 1] = pane_pos(s.nativeFrame[r][k - 1]);
        }
        s.iconTmpl[r] = s.nativeIcon[r][0];
        s.picTmpl[r] = find_pic(kPicTmpl[r]);
        s.leftConn[r] = find_pic(kLeftConn[r]);
        s.gapConn[r][1] = find_pic(kGap1Conn[r]);
        if (s.picTmpl[r] == nullptr || s.leftConn[r] == nullptr || s.gapConn[r][1] == nullptr) return false;
        s.gapNative[r][1] = true;
        s.gapNativePos[r][1] = pane_pos(s.gapConn[r][1]);

        s.iconCol1[r] = s.nativeIconPos[r][0];
        s.frameCol1[r] = s.nativeFramePos[r][0];
        s.leftConnPos[r] = pane_pos(s.leftConn[r]);
        s.gap1ConnPos[r] = s.gapNativePos[r][1];
    }

    // The clothes row has the third native column (and its connector, anchored top-left).
    s.gapConn[2][2] = find_pic(MULTI_CHAR('tunagi08'));
    if (s.gapConn[2][2] != nullptr) {
        s.gapNative[2][2] = true;
        s.gapNativePos[2][2] = pane_pos(s.gapConn[2][2]);
    }

    s.frameTmpl = s.nativeFrame[2][0];
    s.connTmpl = s.gapConn[0][1];
    s.refIcon = s.nativeIcon[2][0];

    const f32 iconDx = s.nativeIconPos[2][1].x - s.nativeIconPos[2][0].x;
    const f32 frameDx = s.nativeFramePos[2][1].x - s.nativeFramePos[2][0].x;
    if (iconDx > 1.0f) s.iconDx = iconDx;
    if (frameDx > 1.0f) s.frameDx = frameDx;

    s.heart = find(MULTI_CHAR('heart_n'));
    s.kamen = find(MULTI_CHAR('kamen_n'));
    s.modelbgn = find(MULTI_CHAR('modelbgn'));
    if (s.heart != nullptr) s.heartPos = pane_pos(s.heart);
    if (s.kamen != nullptr) s.kamenPos = pane_pos(s.kamen);
    if (s.modelbgn != nullptr) s.modelbgnPos = pane_pos(s.modelbgn);
    return true;
}

// The layout's own picture of a vanilla item, for slots that bring no icon.
J2DPicture* native_item_picture(u8 itemNo) {
    u64 tag;
    switch (itemNo) {
    case dItemNo_WOOD_STICK_e:  tag = MULTI_CHAR('ken_00'); break;
    case dItemNo_SWORD_e:       tag = MULTI_CHAR('ken_01'); break;
    case dItemNo_SHIELD_e:      tag = MULTI_CHAR('tate_00'); break;
    case dItemNo_WOOD_SHIELD_e: tag = MULTI_CHAR('tate_01'); break;
    default:                    return nullptr;
    }
    J2DPane* pane = find(tag);
    return pane != nullptr && is_picture(pane) ? static_cast<J2DPicture*>(pane) : nullptr;
}

void create_column_panes(int r, int col, const ClColumn& column) {
    ColPanes& p = s.cols[r][col];
    const int nativeCol = native_col_of_x(r, column.x);

    if (column.type == ClColType::Native) {
        p.icon = s.nativeIcon[r][nativeCol - 1];
        p.frame = s.nativeFrame[r][nativeCol - 1];
        return;
    }

    ResTIMG* tex = custom_equip_icon(column.customId);
    J2DPicture* nativePic = nullptr;
    if (tex == nullptr) {
        const CustomEquipDef* d = custom_equip_get(column.customId);
        if (d != nullptr) nativePic = native_item_picture(d->baseItem);
    }
    J2DPicture* picTmpl = nativePic != nullptr ? nativePic : s.picTmpl[r];
    p.icon = clone_container(s.iconTmpl[r], custom_icon_tag(r, column.x));
    p.pic = clone_picture(p.icon, cl_make_tag('c', 'l', 'p', 'c', static_cast<u8>(r), column.x),
                          picTmpl, tex != nullptr ? tex : cl_pane_texture(picTmpl));
    p.tex = tex;
    if (p.pic != nullptr) {
        if (tex != nullptr || nativePic != nullptr) p.pic->show(); else p.pic->hide();
    }

    // A custom item in a native cell (the native item was replaced) uses that cell's frame.
    if (nativeCol != 0) {
        p.frame = s.nativeFrame[r][nativeCol - 1];
    } else {
        p.frame = clone_picture(s.frameTmpl->getParentPane(),
                                cl_make_tag('c', 'l', 'f', 'r', static_cast<u8>(r), column.x),
                                s.frameTmpl, cl_pane_texture(s.frameTmpl));
    }
}

void create_panes() {
    for (int r = 0; r < kClRows; r++) {
        for (int col = 1; col <= kClMaxCols; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type != ClColType::Empty) create_column_panes(r, col, column);
        }
        for (int g = 1; g < kClMaxCols; g++) {
            if (s.gapConn[r][g] != nullptr) continue;
            if (layout_column(r, g).type == ClColType::Empty ||
                layout_column(r, g + 1).type == ClColType::Empty) {
                continue;
            }
            s.gapConn[r][g] = clone_picture(s.connTmpl->getParentPane(),
                                            cl_make_tag('c', 'l', 'c', 'n', static_cast<u8>(r), static_cast<u8>(g)),
                                            s.connTmpl, cl_pane_texture(s.connTmpl));
        }
    }
}

void set_frame_color(J2DPicture* frame, bool on) {
    if (frame == nullptr) return;
    if (s.hdRoot != nullptr) {
        frame->setBlackWhite(kFrameBlack, on ? kClHdFrameOn : kClHdFrameOff);
    } else {
        frame->setBlackWhite(kFrameBlack, on ? kFrameOn : kFrameOff);
    }
}

// Custom slots on top of what the native setEquipItemFrameColor* painted.
void color_row_frames(int r) {
    for (int col = 1; col <= kClMaxCols; col++) {
        const ClColumn& column = layout_column(r, col);
        J2DPicture* frame = s.cols[r][col].frame;
        if (frame == nullptr) continue;
        if (column.type == ClColType::Native) {
            // The native code also lights the frame for the vanilla item underneath a custom
            // one, and for a Wooden Sword / Ordon Shield that has a column of its own.
            if (!native_cell_equipped(r, column.x)) set_frame_color(frame, false);
        } else if (column.type == ClColType::Custom) {
            set_frame_color(frame, custom_equip_unlocked(column.customId) &&
                                       custom_equip_equipped(column.customId));
        }
    }
}

void fill_tables(dMenu_Collect2D_c* c) {
    std::memcpy(s.native22d, c->field_0x22d, sizeof(s.native22d));
    std::memcpy(s.native184, c->field_0x184, sizeof(s.native184));
    std::memcpy(s.native1d8, c->field_0x1d8, sizeof(s.native1d8));
    s.snap = true;

    // A Wooden Sword / Ordon Shield with a column of its own leaves the native cell that
    // shows it until the Ordon Sword / Wooden Shield is owned.
    if (layout_has_stand_in(dItemNo_WOOD_STICK_e)) {
        c->field_0x22d[3][0] = dComIfGs_isItemFirstBit(dItemNo_SWORD_e) ? 1 : 0;
    }
    if (layout_has_stand_in(dItemNo_WOOD_SHIELD_e)) {
        c->field_0x22d[3][1] = dComIfGs_isItemFirstBit(dItemNo_SHIELD_e) ? 1 : 0;
    }
    // The game empties the clothes row while the Ordon Clothes are worn (it has no cell to
    // switch back from them). With a slot for them, the row stays.
    if (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e &&
        layout_uses_base_item(dItemNo_WEAR_CASUAL_e)) {
        c->field_0x22d[3][2] = dComIfGs_isItemFirstBit(dItemNo_WEAR_KOKIRI_e) ? 1 : 0;
        c->field_0x22d[4][2] = dComIfGs_isItemFirstBit(dItemNo_WEAR_ZORA_e) ? 1 : 0;
        c->field_0x22d[5][2] = dComIfGs_isItemFirstBit(dItemNo_ARMOR_e) ? 1 : 0;
    }

    for (int r = 0; r < kClRows; r++) {
        for (u8 x = 0; x < 7; x++) {
            const int col = layout_col_of_cell(r, x);
            if (col != 0) {
                const ClColumn& column = layout_column(r, col);
                if (column.type == ClColType::Native) continue;
                c->field_0x22d[x][r] = custom_equip_unlocked(column.customId) ? 1 : 0;
                const CustomEquipDef* d = custom_equip_get(column.customId);
                if (d != nullptr && d->name == nullptr && d->baseItem != dItemNo_NONE_e) {
                    // No text of its own: the game's name and description of the base item.
                    c->field_0x184[x][r] = static_cast<u16>(kClItemNameMsg + d->baseItem);
                    c->field_0x1d8[x][r] = static_cast<u16>(kClItemNameMsg + 0x100 + d->baseItem);
                } else {
                    c->field_0x184[x][r] = layout_name_msg(r, x);
                    c->field_0x1d8[x][r] = layout_desc_msg(r, x);
                }
                continue;
            }
            if (r == 0 && x == 5 && s.heartOnMain) continue;
            if (r == 0 && x == 6 && s.maskOnMain) continue;
            c->field_0x22d[x][r] = 0;
            c->field_0x184[x][r] = 0;
            c->field_0x1d8[x][r] = 0;
        }
    }
}

// The pane a cell's cursor manager must point at, nullptr for a cell outside the grid.
J2DPane* cell_pane(int r, u8 x) {
    const int col = layout_col_of_cell(r, x);
    if (col != 0) {
        const ClColumn& column = layout_column(r, col);
        return column.type == ClColType::Native ? s.nativeIcon[r][native_col_of_x(r, x) - 1]
                                                : s.cols[r][col].icon;
    }
    if (r == 0 && x == 5 && s.heartOnMain) return find(MULTI_CHAR('heart_kn'));
    if (r == 0 && x == 6 && s.maskOnMain) return s.kamen;
    return nullptr;
}

// screenSet() creates the CPaneMgr of every cell from its own tag table (getItemTag is
// inlined there), so cells the library added have none and replaced cells point at the
// hidden native pane. The cursor and the pointer use these managers.
void setup_cell_managers(dMenu_Collect2D_c* c) {
    JKRHeap* oldHeap = c->mpHeap != nullptr ? mDoExt_setCurrentHeap(c->mpHeap) : nullptr;
    for (int r = 0; r < kClRows; r++) {
        for (u8 x = 0; x < 7; x++) {
            J2DPane* pane = cell_pane(r, x);
            CPaneMgr*& pm = c->mpSelPm[x][r];
            if (pm != nullptr && pm->getPanePtr() == pane) continue;
            if (pm != nullptr) {
                JKR_DELETE(pm);
                pm = nullptr;
            }
            if (pane != nullptr) pm = JKR_NEW CPaneMgr(c->mpScreen, pane->mInfoTag, 0, NULL);
        }
    }
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
}

// screenSet() moves a restored cursor that sits on a cell unknown to its tag table.
void restore_cursor(dMenu_Collect2D_c* c) {
    const u8 x = dMeter2Info_getCollectCursorPosX();
    const u8 y = dMeter2Info_getCollectCursorPosY();
    if (screen_equip_row_at(x, y) < 0 || (c->mCursorX == x && c->mCursorY == y)) return;
    c->mCursorX = x;
    c->mCursorY = y;
    c->field_0x259 = x;
    c->field_0x25a = y;
}

bool screen_build(dMenu_Collect2D_c* c) {
    s = ScreenState{};
    s.collect = c;
    s.screen = c->mpScreen;

    collectionlib_run_slot_registration();

    if (!find_templates()) {
        log_collect_info("collection-lib: Collection screen layout not recognized, leaving it native");
        return false;
    }

    s.heartOnMain = !collection_page_claims_cell(5, 0) && layout_col_of_cell(0, 5) == 0;
    s.maskOnMain = !collection_page_claims_cell(6, 0) && layout_col_of_cell(0, 6) == 0;

    find_blo();
    JKRHeap* oldHeap = c->mpHeap != nullptr ? mDoExt_setCurrentHeap(c->mpHeap) : nullptr;
    create_panes();
    collection_page_sync_screen(s.screen);
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);

    s.built = true;
    screen_apply_layout(c);
    return true;
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

void on_menu_collect_2d_create_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    s_currentCollect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    custom_equip_menu_doll_begin();
}

HookAction on_menu_collect_2d_delete_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_end();
    s_currentCollect2D = nullptr;
    // The panes belong to the screen that is deleted now.
    s = ScreenState{};
    collection_page_teardown();
    return HOOK_CONTINUE;
}

HookAction on_screen_set_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return HOOK_CONTINUE;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (c == nullptr || c->mpScreen == nullptr) return HOOK_CONTINUE;
    collection_page_reset();
    // Panes must exist before screenSet(): it creates the CPaneMgr of every cell getItemTag()
    // names, the library's cells included.
    screen_build(c);
    return HOOK_CONTINUE;
}

void on_screen_set_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c)) return;

    fill_tables(c);
    screen_apply_layout(c);
    setup_cell_managers(c);
    restore_cursor(c);
    screen_refresh_frames(c);
    // screenSet() showed name and cursor with the native tables.
    c->setItemNameString(c->mCursorX, c->mCursorY);
    c->cursorPosSet();
}

void on_menu_collect_wide_post(ModContext*, void* args, void*, void*) {
    if (args == nullptr) return;
    dMenu_Collect2D_c* c = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!screen_active(c)) return;
    // menuCollectWide() puts the native panes back to their layout positions every frame,
    // right before drawing.
    screen_apply_layout(c);
    collection_page_apply(c);
}

HookAction on_screen_draw_pre(ModContext*, void* args, void*, void*) {
    if (args == nullptr || !s.built || s.hdRoot == nullptr) return HOOK_CONTINUE;
    if (mods::arg<J2DScreen*>(args, 0) != s.screen) return HOOK_CONTINUE;
    screen_apply_layout(s.collect);
    return HOOK_CONTINUE;
}

void on_set_equip_frame_sword_post(ModContext*, void* args, void*, void*) {
    if (args != nullptr && screen_active(mods::arg<dMenu_Collect2D_c*>(args, 0))) color_row_frames(0);
}

void on_set_equip_frame_shield_post(ModContext*, void* args, void*, void*) {
    if (args != nullptr && screen_active(mods::arg<dMenu_Collect2D_c*>(args, 0))) color_row_frames(1);
}

void on_set_equip_frame_clothes_post(ModContext*, void* args, void*, void*) {
    if (args != nullptr && screen_active(mods::arg<dMenu_Collect2D_c*>(args, 0))) color_row_frames(2);
}

void on_mw_execute_post(ModContext*, void*, void*, void*) {
    if (!s_needReloadCollect) return;
    s_needReloadCollect = false;
    dMw_c* mw = dMeter2Info_getMenuWindowClass();
    if (mw != nullptr && s_currentCollect2D != nullptr && mw->isPauseWindow()) {
        mw->dMw_collect_delete(true);
        mw->dMw_collect_create();
    }
}

void hd_attach(dMenu_Collect2D_c* c) {
    if (s.hdRoot != nullptr || !cl_hd_layout_requested()) return;
    J2DPane* root = find(kClHdRootTag);
    if (root == nullptr) return;
    s.hdRoot = root;

    const ResTIMG* hdFrameTex = cl_pane_texture(s.nativeFrame[0][0]);
    JKRHeap* oldHeap = c->mpHeap != nullptr ? mDoExt_setCurrentHeap(c->mpHeap) : nullptr;
    for (int r = 0; r < kClRows; r++) {
        for (int col = 1; col <= kClMaxCols; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type != ClColType::Custom) continue;
            ColPanes& p = s.cols[r][col];
            if (native_col_of_x(r, column.x) == 0 && p.frame != nullptr) {
                if (hdFrameTex != nullptr && cl_pane_texture(p.frame) != hdFrameTex) {
                    p.frame->changeTexture(hdFrameTex, 0);
                    p.frame->setTexCoord(p.frame->getTexture(0), BIND15, MIRROR0, false);
                    p.frame->setCornerColor(JUtility::TColor(255, 255, 255, 255));
                    p.frame->setAlpha(255);
                }
                root->appendChild(p.frame);
                for (int corner = 0; corner < 2; corner++) {
                    const u64 tag = cl_make_tag('c', 'l', 'f', 'l', static_cast<u8>(r),
                                                static_cast<u8>(col * 2 + corner));
                    p.flourish[corner] = hd_new_flourish(root, tag, corner != 0);
                }
            }
            if (p.icon != nullptr) root->appendChild(p.icon);
        }
    }
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    screen_refresh_frames(c);
}

void place_hd_column(int r, int col, const ClColumn& column, ColPanes& p) {
    const ClHdPos pos = hd_column_pos(r, col);
    hd_place(p.icon, pos.x, pos.y, kClHdIconSize, kClHdIconSize);
    hd_place(p.frame, pos.x, pos.y, kClHdFrameSize, kClHdFrameSize);

    const int frameIndex = native_col_of_x(r, column.x) != 0 ? hd_frame_index(column.x, static_cast<u8>(r)) : -1;
    if (frameIndex >= 0) {
        hd_place_flourishes(find(hd_native_flourish_tag(frameIndex, 0)),
                            find(hd_native_flourish_tag(frameIndex, 1)), pos);
        return;
    }
    const bool lit = p.frame != nullptr && p.frame->getWhite().r > 200;
    for (J2DPicture* flourish : p.flourish) {
        if (flourish == nullptr) continue;
        if (lit) flourish->show(); else flourish->hide();
    }
    hd_place_flourishes(p.flourish[0], p.flourish[1], pos);
}

}  // namespace

// ---------------------------------------------------------------------------
// Internal API
// ---------------------------------------------------------------------------

bool screen_active(const dMenu_Collect2D_c* c) {
    return c != nullptr && s.built && s.collect == c && s.screen == c->mpScreen;
}

bool screen_heart_on_main() { return s.heartOnMain; }
bool screen_mask_on_main() { return s.maskOnMain; }

u64 screen_custom_icon_tag(int r, u8 x) {
    const int col = layout_col_of_cell(r, x);
    if (col == 0 || s.cols[r][col].icon == nullptr) return 0;
    return s.cols[r][col].icon->mInfoTag;
}

J2DPane* screen_cell_pane(u8 x, u8 y) {
    if (!s.built || y >= kClRows || x >= 7) return nullptr;
    return cell_pane(y, x);
}

J2DPicture* screen_cell_frame(u8 x, u8 y) {
    if (!s.built || y >= kClRows || x >= 7) return nullptr;
    const int col = layout_col_of_cell(y, x);
    return col != 0 ? s.cols[y][col].frame : nullptr;
}

bool screen_hd_active() { return s.built && s.hdRoot != nullptr; }

int screen_equip_row_at(u8 x, u8 y) {
    if (y >= kClRows || x >= 7) return -1;
    if (layout_col_of_cell(y, x) != 0) return y;
    if (y == 0 && ((x == 5 && s.heartOnMain) || (x == 6 && s.maskOnMain))) return 0;
    return -1;
}

void screen_refresh_frames(dMenu_Collect2D_c* c) {
    if (!screen_active(c)) return;
    // Force the native functions to repaint; their post hooks add the custom slots.
    c->mEquippedSword = 0xFF;
    c->mEquippedShield = 0xFF;
    c->mEquippedClothes = 0xFF;
    c->setEquipItemFrameColorSword(-1);
    c->setEquipItemFrameColorShield(-1);
    c->setEquipItemFrameColorClothes(-1);
}

void screen_apply_layout(dMenu_Collect2D_c* c) {
    if (!screen_active(c)) return;

    hd_attach(c);
    const bool hd = s.hdRoot != nullptr;
    const f32 dx = collection_page_grid_dx();
    const f32 scaleX = s.refIcon->getScaleX();
    const f32 scaleY = s.refIcon->getScaleY();

    bool nativeIconUsed[kClRows][3] = {};
    bool nativeFrameUsed[kClRows][3] = {};

    for (int r = 0; r < kClRows; r++) {
        const bool hdRow = hd && !hd_row_native(r);
        for (int col = 1; col <= kClMaxCols; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type == ClColType::Empty) continue;
            ColPanes& p = s.cols[r][col];
            const int nativeCol = native_col_of_x(r, column.x);

            if (p.icon != nullptr) {
                bool visible;
                if (column.type == ClColType::Native) {
                    // Same rule screenSet() uses for native panes.
                    visible = c->field_0x22d[column.x][r] != 0;
                    nativeIconUsed[r][nativeCol - 1] = true;
                } else {
                    visible = custom_equip_unlocked(column.customId);
                    c->field_0x22d[column.x][r] = visible ? 1 : 0;
                    if (!hd) p.icon->scale(scaleX, scaleY);
                    ResTIMG* tex = custom_equip_icon(column.customId);
                    if (p.pic != nullptr && tex != nullptr && tex != p.tex) {
                        p.pic->changeTexture(tex, 0);
                        p.pic->show();
                        p.tex = tex;
                    }
                }
                if (visible) p.icon->show(); else p.icon->hide();
            }
            if (p.frame != nullptr) {
                p.frame->show();
                if (nativeCol != 0) nativeFrameUsed[r][nativeCol - 1] = true;
            }

            if (hd) {
                if (hdRow) place_hd_column(r, col, column, p);
                continue;
            }
            Pos icon{s.iconCol1[r].x + (col - 1) * s.iconDx, s.iconCol1[r].y};
            Pos frame{s.frameCol1[r].x + (col - 1) * s.frameDx, s.frameCol1[r].y};
            if (nativeCol != 0 && nativeCol == col) {
                // In its native column: exactly the native spot.
                if (column.type == ClColType::Native) icon = s.nativeIconPos[r][nativeCol - 1];
                frame = s.nativeFramePos[r][nativeCol - 1];
            }
            if (p.icon != nullptr) cl_set_pane_pos(p.icon, icon.x + dx, icon.y);
            if (p.frame != nullptr) cl_set_pane_pos(p.frame, frame.x + dx, frame.y);
        }

        for (int k = 1; k <= native_col_count(r); k++) {
            if (!nativeIconUsed[r][k - 1]) s.nativeIcon[r][k - 1]->hide();
            if (!nativeFrameUsed[r][k - 1]) s.nativeFrame[r][k - 1]->hide();
        }
        if (hd) continue;

        // Connectors: one left of column 1, one between every two neighboring columns.
        if (layout_column(r, 1).type != ClColType::Empty) {
            cl_set_pane_pos(s.leftConn[r], s.leftConnPos[r].x + dx, s.leftConnPos[r].y);
            s.leftConn[r]->show();
        } else {
            s.leftConn[r]->hide();
        }
        for (int g = 1; g < kClMaxCols; g++) {
            J2DPicture* conn = s.gapConn[r][g];
            if (conn == nullptr) continue;
            const bool used = layout_column(r, g).type != ClColType::Empty &&
                              layout_column(r, g + 1).type != ClColType::Empty;
            if (!used) {
                conn->hide();
                continue;
            }
            const Pos pos = s.gapNative[r][g]
                ? s.gapNativePos[r][g]
                : Pos{s.gap1ConnPos[r].x + (g - 1) * s.frameDx, s.gap1ConnPos[r].y};
            cl_set_pane_pos(conn, pos.x + dx, pos.y);
            conn->show();
        }
    }

    if (hd) {
        const ClHdPos heart = hd_heart_pos();
        const ClHdPos mask = hd_mask_pos();
        if (c->mpHeartParent != nullptr) {
            hd_place(c->mpHeartParent->getPanePtr(), heart.x, heart.y, kClHdHeartSize, kClHdHeartSize);
        }
        hd_place(s.kamen, mask.x, mask.y, kClHdMaskSize, kClHdMaskSize);
        return;
    }

    // Rows longer than the native ones push the heart (behind the sword and shield rows) and
    // the fused shadow (behind the heart and the clothes row) to the right.
    const int heartPush = std::max(0, std::max(layout_last_col(0) - native_col_count(0),
                                               layout_last_col(1) - native_col_count(1)));
    const int maskPush = std::max(heartPush, layout_last_col(2) - native_col_count(2));
    if (s.heartOnMain && s.heart != nullptr && heartPush > 0) {
        cl_set_pane_pos(s.heart, s.heartPos.x + heartPush * s.frameDx, s.heartPos.y);
    }
    if (s.maskOnMain && maskPush > 0) {
        const f32 push = maskPush * s.frameDx;
        if (s.kamen != nullptr) cl_set_pane_pos(s.kamen, s.kamenPos.x + push, s.kamenPos.y);
        if (s.modelbgn != nullptr) cl_set_pane_pos(s.modelbgn, s.modelbgnPos.x + push, s.modelbgnPos.y);
    }

    // Heart / fused shadow whose cell went to a sword column and that no page hosts.
    if (!s.heartOnMain && !collection_page_claims_cell(5, 0) && s.heart != nullptr) {
        s.heart->hide();
    }
    if (!s.maskOnMain && !collection_page_claims_cell(6, 0)) {
        // The 3D model follows the pane, so hiding is not enough: park both off screen.
        if (s.kamen != nullptr) s.kamen->translate(-4000.0f, s.kamen->getTranslateY());
        if (s.modelbgn != nullptr) s.modelbgn->translate(-4000.0f, s.modelbgn->getTranslateY());
    }
}

void screen_show_native_name(dMenu_Collect2D_c* c, u8 x, u8 y) {
    if (!screen_active(c) || !s.snap || x >= 7 || y >= 6) {
        if (c != nullptr) c->setItemNameStringNull();
        return;
    }
    // Cell (0,0) is never part of the grid: borrow it to run the native text code.
    const u8 saved22d = c->field_0x22d[0][0];
    const u16 saved184 = c->field_0x184[0][0];
    const u16 saved1d8 = c->field_0x1d8[0][0];
    c->field_0x22d[0][0] = s.native22d[x][y];
    c->field_0x184[0][0] = s.native184[x][y];
    c->field_0x1d8[0][0] = s.native1d8[x][y];
    c->setItemNameString(0, 0);
    c->field_0x22d[0][0] = saved22d;
    c->field_0x184[0][0] = saved184;
    c->field_0x1d8[0][0] = saved1d8;
}

int screen_grid_panes(J2DPane** out, int max) {
    int n = 0;
    auto add = [&](J2DPane* p) {
        if (p != nullptr && n < max) out[n++] = p;
    };
    if (!s.built) return 0;
    for (int r = 0; r < kClRows; r++) {
        for (int k = 0; k < native_col_count(r); k++) {
            add(s.nativeIcon[r][k]);
            add(s.nativeFrame[r][k]);
        }
        for (int col = 1; col <= kClMaxCols; col++) {
            if (s.cols[r][col].pic != nullptr) add(s.cols[r][col].icon);
            if (s.cols[r][col].frame != nullptr && native_col_of_x(r, layout_column(r, col).x) == 0) {
                add(s.cols[r][col].frame);
            }
        }
        add(s.leftConn[r]);
        for (int g = 1; g < kClMaxCols; g++) add(s.gapConn[r][g]);
    }
    return n;
}

void screen_install_hooks(const HookService* hook_svc) {
    CL_HOOK_POST(MenuCollect2DCreateHook, on_menu_collect_2d_create_post);
    CL_HOOK_PRE(MenuCollect2DDeleteHook, on_menu_collect_2d_delete_pre);
    CL_HOOK_PRE(ScreenSetHook, on_screen_set_pre);
    CL_HOOK_POST(ScreenSetHook, on_screen_set_post);
    CL_HOOK_POST_PRIO(MenuCollectWideHook, on_menu_collect_wide_post, kClAfterOtherMods);
    CL_HOOK_PRE_PRIO(ClScreenDrawHook, on_screen_draw_pre, kClAfterOtherMods);
    CL_HOOK_POST(SetEquipFrameColorSwordHook, on_set_equip_frame_sword_post);
    CL_HOOK_POST(SetEquipFrameColorShieldHook, on_set_equip_frame_shield_post);
    CL_HOOK_POST(SetEquipFrameColorClothesHook, on_set_equip_frame_clothes_post);
    CL_HOOK_POST(MwExecuteHook, on_mw_execute_post);
    hd_install_hooks(hook_svc);
}

void screen_shutdown() { s = ScreenState{}; }
