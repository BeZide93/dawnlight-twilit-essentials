#include "collection_internal.hpp"

#include "JSystem/JKernel/JKRArchive.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <cmath>
#include <iterator>

DEFINE_HOOK(&dSelect_cursor_c::update, ClSelectCursorUpdateHook);
DEFINE_HOOK(&dSelect_cursor_c::draw, ClSelectCursorDrawHook);
DEFINE_HOOK(&dMenu_Collect3D_c::_move, ClCollect3DMoveHook);

const JUtility::TColor kClHdFrameOn(246, 244, 198, 255);
const JUtility::TColor kClHdFrameOff(132, 134, 104, 255);

namespace {

struct HdCell {
    u8  x, y;
    f32 cx, cy, size;
};

const HdCell kHdCells[] = {
    {4, 2, 338.0f, 80.0f, 46.0f},   {3, 2, 396.0f, 80.0f, 46.0f},   {5, 2, 454.0f, 80.0f, 46.0f},
    {3, 1, 260.0f, 145.0f, 46.0f},  {4, 1, 320.0f, 145.0f, 46.0f},
    {3, 0, 478.0f, 145.0f, 46.0f},  {4, 0, 538.0f, 145.0f, 46.0f},
    {5, 0, 158.0f, 205.0f, 112.0f}, {6, 0, 655.0f, 200.0f, 80.0f},
    {0, 3, 75.0f, 310.0f, 44.0f},   {1, 3, 130.0f, 310.0f, 44.0f},
    {0, 4, 185.0f, 310.0f, 44.0f},  {1, 4, 240.0f, 310.0f, 44.0f},
    {2, 3, 75.0f, 370.0f, 44.0f},   {2, 4, 130.0f, 370.0f, 44.0f},
    {3, 3, 185.0f, 370.0f, 44.0f},  {3, 4, 240.0f, 370.0f, 44.0f},
    {0, 5, 158.0f, 416.0f, 156.0f}, {1, 5, 638.0f, 416.0f, 156.0f},
};
constexpr int kHdEquipFrames = 7;

constexpr f32 kCanvasW = 796.0f;
constexpr f32 kCanvasH = 448.0f;

constexpr f32 kSwordX = 478.0f;
constexpr f32 kShieldX = 320.0f;
constexpr f32 kClothesX = 396.0f;
constexpr f32 kSideRowY = 145.0f;
constexpr f32 kClothesY = 80.0f;
constexpr f32 kSideDx = 60.0f;
constexpr f32 kClothesDx = 58.0f;
constexpr f32 kMaskAfterSword = 77.0f;
constexpr f32 kHeartDrop = 16.0f;
constexpr f32 kMaskDrop = 20.0f;
constexpr f32 kHeartCursorScale = 0.75f;

constexpr f32 kFlourishSize = 24.0f;
const char* const kFlourishTexture = "tt_kazari_2nd_okan_64.bti";

const HdCell* hd_cell(u8 x, u8 y) {
    for (const HdCell& cell : kHdCells) {
        if (cell.x == x && cell.y == y) return &cell;
    }
    return nullptr;
}

f32 canvas_scale() {
    return std::min((mDoGph_gInf_c::getSafeMaxXF() - mDoGph_gInf_c::getSafeMinXF()) / kCanvasW,
                    (mDoGph_gInf_c::getSafeMaxYF() - mDoGph_gInf_c::getSafeMinYF()) / kCanvasH);
}

struct NavCell {
    u8   x, y;
    f32  cx, cy, size;
    bool available;
};

int nearest_cell(const NavCell* cells, int count, int current, int dir) {
    const NavCell& origin = cells[current];
    const bool horizontal = dir == 0 || dir == 1;
    const f32 sign = dir == 0 || dir == 2 ? -1.0f : 1.0f;
    int best = current;
    int bestLane = 2;
    f32 bestScore = 1.0e30f;
    f32 bestSideways = 1.0e30f;
    for (int i = 0; i < count; i++) {
        if (!cells[i].available || i == current) continue;
        const f32 dx = cells[i].cx - origin.cx;
        const f32 dy = cells[i].cy - origin.cy;
        const f32 forward = (horizontal ? dx : dy) * sign;
        const f32 sideways = std::fabs(horizontal ? dy : dx);
        if (forward < 1.0f) continue;
        const bool overlaps = sideways <= (origin.size + cells[i].size) * 0.5f + 8.0f;
        const int lane = horizontal || overlaps ? 0 : 1;
        const f32 score = horizontal ? forward + 3.0f * sideways + (sideways >= 1.0f ? 10000.0f : 0.0f) +
                                           (sideways > forward ? 10000.0f : 0.0f)
                                     : overlaps ? forward : forward + 3.0f * sideways;
        if (lane < bestLane ||
            (lane == bestLane &&
             (score < bestScore || (!horizontal && score == bestScore && sideways < bestSideways)))) {
            bestLane = lane;
            bestScore = score;
            bestSideways = sideways;
            best = i;
        }
    }
    return best;
}

void position_cursor(dSelect_cursor_c* cursor) {
    dMenu_Collect2D_c* c = s_currentCollect2D;
    if (c == nullptr || cursor == nullptr || cursor != c->mpDrawCursor || !screen_active(c) ||
        !screen_hd_active()) {
        return;
    }
    const u8 x = c->mCursorX;
    const u8 y = c->mCursorY;
    const bool heart = x == 5 && y == 0;
    if (y >= kClRows || (!heart && hd_cell(x, y) != nullptr)) return;
    J2DPane* target = heart ? screen_cell_pane(x, y) : screen_cell_frame(x, y);
    if (target == nullptr) target = screen_cell_pane(x, y);
    if (target == nullptr) return;

    CPaneMgr probe;
    Mtx mtx;
    const Vec first = probe.getGlobalVtx(target, &mtx, 0, false, 0);
    const Vec last = probe.getGlobalVtx(target, &mtx, 3, false, 0);
    const f32 centerX = (first.x + last.x) * 0.5f;
    const f32 centerY = (first.y + last.y) * 0.5f;
    cursor->setPos(centerX, centerY, target, false);
    if (cursor->mpPaneMgr != nullptr) cursor->mpPaneMgr->translate(centerX, centerY);

    const f32 scale = canvas_scale();
    const f32 shrink = heart ? kHeartCursorScale : 1.0f;
    const f32 halfWidth = std::max(1.0f, (std::fabs(last.x - first.x) * 0.5f - 2.0f * scale) * shrink);
    const f32 halfHeight = std::max(1.0f, (std::fabs(last.y - first.y) * 0.5f - 2.0f * scale) * shrink);
    const f32 phase = cursor->field_0x40;
    const f32 pulse = 0.96f + 0.04f * (phase < 10.0f ? phase / 10.0f : (20.0f - phase) / 10.0f);
    static const u64 kCorners[4] = {MULTI_CHAR('l_u_null'), MULTI_CHAR('l_d_null'),
                                    MULTI_CHAR('r_u_null'), MULTI_CHAR('r_d_null')};
    for (int corner = 0; corner < 4; corner++) {
        const f32 cx = (corner < 2 ? -halfWidth : halfWidth) * pulse;
        const f32 cy = (corner % 2 == 0 ? -halfHeight : halfHeight) * pulse;
        cursor->field_0x94[corner] = cx;
        cursor->field_0xa4[corner] = cy;
        cursor->field_0x74[corner] = cx;
        cursor->field_0x84[corner] = cy;
        if (J2DPane* pane = cursor->mpScreen->search(kCorners[corner])) cursor->moveCenter(pane, cx, cy);
    }
}

void on_select_cursor_update_post(ModContext*, void* args, void*, void*) {
    if (args != nullptr) position_cursor(mods::arg<dSelect_cursor_c*>(args, 0));
}

HookAction on_select_cursor_draw_pre(ModContext*, void* args, void*, void*) {
    if (args != nullptr) position_cursor(mods::arg<dSelect_cursor_c*>(args, 0));
    return HOOK_CONTINUE;
}

HookAction on_collect3d_move_pre(ModContext*, void* args, void*, void*) {
    dMenu_Collect3D_c* menu = args != nullptr ? mods::arg<dMenu_Collect3D_c*>(args, 0) : nullptr;
    dMenu_Collect2D_c* c = menu != nullptr ? menu->mpCollect2D : nullptr;
    if (c == nullptr || c != s_currentCollect2D || !screen_hd_active() || c->getpMask() == nullptr) {
        return HOOK_CONTINUE;
    }
    const ClHdPos pos = hd_mask_pos();
    hd_place(c->getpMask()->getPanePtr(), pos.x, pos.y, kClHdMaskSize, kClHdMaskSize);
    return HOOK_CONTINUE;
}

Vec s_maskScaleWritten = {0.0f, 0.0f, 0.0f};

void on_collect3d_move_post(ModContext*, void* args, void*, void*) {
    dMenu_Collect3D_c* menu = args != nullptr ? mods::arg<dMenu_Collect3D_c*>(args, 0) : nullptr;
    if (menu == nullptr || menu->mpModel == nullptr || menu->mpCollect2D != s_currentCollect2D ||
        !screen_hd_active()) {
        return;
    }
    const Vec* scale = menu->mpModel->getBaseScale();
    if (scale->x == s_maskScaleWritten.x && scale->y == s_maskScaleWritten.y &&
        scale->z == s_maskScaleWritten.z) {
        return;
    }
    const Vec smaller = {scale->x * kClHdSideScale, scale->y * kClHdSideScale, scale->z * kClHdSideScale};
    menu->mpModel->setBaseScale(smaller);
    s_maskScaleWritten = smaller;
}

}  // namespace

void hd_place(J2DPane* pane, f32 x, f32 y, f32 w, f32 h) {
    if (pane == nullptr || pane->getWidth() <= 0.0f || pane->getHeight() <= 0.0f) return;
    const f32 scale = canvas_scale();
    const f32 left = (mDoGph_gInf_c::getSafeMinXF() + mDoGph_gInf_c::getSafeMaxXF() - kCanvasW * scale) * 0.5f;
    const f32 top = (mDoGph_gInf_c::getSafeMinYF() + mDoGph_gInf_c::getSafeMaxYF() - kCanvasH * scale) * 0.5f;
    f32 parentX = 1.0f;
    f32 parentY = 1.0f;
    for (J2DPane* p = pane->getParentPane(); p != nullptr; p = p->getParentPane()) {
        parentX *= p->getScaleX();
        parentY *= p->getScaleY();
    }
    if (std::fabs(parentX) < 0.001f || std::fabs(parentY) < 0.001f) return;
    const f32 fit = std::min(w / pane->getWidth(), h / pane->getHeight());
    pane->scale(fit * scale / parentX, fit * scale / parentY);
    const Vec center = cl_pane_global_center(pane);
    pane->translate(pane->getTranslateX() + (left + x * scale - center.x) / parentX,
                    pane->getTranslateY() + (top + y * scale - center.y) / parentY);
}

bool hd_row_native(int r) {
    for (int col = 1; col <= kClMaxCols; col++) {
        const ClColumn& column = layout_column(r, col);
        if (col <= native_col_count(r)) {
            if (column.type != ClColType::Native || column.x != native_cell(r, col).x) return false;
        } else if (column.type != ClColType::Empty) {
            return false;
        }
    }
    return true;
}

ClHdPos hd_column_pos(int r, int col) {
    const ClColumn& column = layout_column(r, col);
    if (column.type == ClColType::Native && hd_row_native(r)) {
        if (const HdCell* cell = hd_cell(column.x, static_cast<u8>(r))) return ClHdPos{cell->cx, cell->cy};
    }
    const int count = layout_last_col(r);
    if (r == 0) return ClHdPos{kSwordX + (col - 1) * kSideDx, kSideRowY};
    if (r == 1) return ClHdPos{kShieldX - (count - col) * kSideDx, kSideRowY};
    return ClHdPos{kClothesX + (col - (count + 1) * 0.5f) * kClothesDx, kClothesY};
}

ClHdPos hd_heart_pos() {
    const HdCell* cell = hd_cell(5, 0);
    return ClHdPos{cell->cx, cell->cy + kHeartDrop};
}

ClHdPos hd_mask_pos() {
    const HdCell* cell = hd_cell(6, 0);
    const int swords = layout_last_col(0);
    const f32 x = swords != 0 ? hd_column_pos(0, swords).x + kMaskAfterSword : cell->cx;
    return ClHdPos{std::max(x, cell->cx), cell->cy + kMaskDrop};
}

int hd_frame_index(u8 x, u8 y) {
    for (int i = 0; i < kHdEquipFrames; i++) {
        if (kHdCells[i].x == x && kHdCells[i].y == y) return i;
    }
    return -1;
}

u64 hd_native_flourish_tag(int frameIndex, int corner) {
    return MULTI_CHAR('hd_cef00') + static_cast<u64>(frameIndex * 2 + corner);
}

void hd_place_flourishes(J2DPane* topLeft, J2DPane* bottomRight, ClHdPos cell) {
    const f32 offset = kClHdIconSize * 0.5f + 3.0f;
    const f32 half = kFlourishSize * 0.5f;
    hd_place(topLeft, cell.x - offset - 6.0f + half, cell.y - offset - 6.0f + half, kFlourishSize, kFlourishSize);
    hd_place(bottomRight, cell.x + offset - 18.0f + half, cell.y + offset - 18.0f + half, kFlourishSize,
             kFlourishSize);
}

J2DPicture* hd_new_flourish(J2DPane* root, u64 tag, bool bottomRight) {
    JKRArchive* arc = dComIfGp_getCollectResArchive();
    const ResTIMG* tex = arc != nullptr ? static_cast<const ResTIMG*>(arc->getResource('TIMG', kFlourishTexture))
                                        : nullptr;
    if (root == nullptr || tex == nullptr) return nullptr;
    JGeometry::TBox2<f32> box;
    box.set(0.0f, 0.0f, kFlourishSize, kFlourishSize);
    J2DPicture* pic = JKR_NEW J2DPicture(tag, box, tex, nullptr);
    if (pic == nullptr) return nullptr;
    pic->setTexCoord(pic->getTexture(0), BIND15,
                     bottomRight ? static_cast<J2DMirror>(J2DMirror_X | J2DMirror_Y) : MIRROR0, false);
    pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), kClHdFrameOn);
    pic->setCornerColor(JUtility::TColor(255, 255, 255, 255));
    pic->setAlpha(255);
    root->appendChild(pic);
    pic->hide();
    return pic;
}

bool hd_nav_target(dMenu_Collect2D_c* c, int dir, u8* x, u8* y) {
    NavCell cells[kClRows * kClMaxCols + std::size(kHdCells)];
    int count = 0;
    int current = -1;
    auto add = [&](u8 cx, u8 cy, ClHdPos pos, f32 size, bool available) {
        if (cx == c->mCursorX && cy == c->mCursorY) current = count;
        cells[count++] = NavCell{cx, cy, pos.x, pos.y, size, available};
    };

    for (int r = 0; r < kClRows; r++) {
        for (int col = 1; col <= kClMaxCols; col++) {
            const ClColumn& column = layout_column(r, col);
            if (column.type == ClColType::Empty) continue;
            add(column.x, static_cast<u8>(r), hd_column_pos(r, col), kClHdIconSize,
                c->field_0x22d[column.x][r] != 0);
        }
    }
    for (const HdCell& cell : kHdCells) {
        const ClHdPos pos{cell.cx, cell.cy};
        if (cell.y == 0 && cell.x == 5) {
            add(cell.x, cell.y, hd_heart_pos(), kClHdHeartSize, true);
        } else if (cell.y == 0 && cell.x == 6) {
            add(cell.x, cell.y, hd_mask_pos(), kClHdMaskSize, c->field_0x22d[6][0] != 0);
        } else if (cell.y >= kClRows) {
            add(cell.x, cell.y, pos, cell.size, cell.y == 5 || c->field_0x22d[cell.x][cell.y] != 0);
        }
    }

    if (current < 0) return false;
    const int best = nearest_cell(cells, count, current, dir);
    if (best == current) return false;
    *x = cells[best].x;
    *y = cells[best].y;
    return true;
}

void hd_install_hooks(const HookService* hook_svc) {
    CL_HOOK_POST_PRIO(ClSelectCursorUpdateHook, on_select_cursor_update_post, kClAfterOtherMods);
    CL_HOOK_PRE_PRIO(ClSelectCursorDrawHook, on_select_cursor_draw_pre, kClAfterOtherMods);
    CL_HOOK_PRE_PRIO(ClCollect3DMoveHook, on_collect3d_move_pre, kClAfterOtherMods);
    CL_HOOK_POST_PRIO(ClCollect3DMoveHook, on_collect3d_move_post, kClAfterOtherMods);
}
