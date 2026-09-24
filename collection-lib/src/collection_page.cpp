#include "collection_internal.hpp"

#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"

#include <chrono>
#include <cmath>

static cl::Page* s_pages[cl::Page::kMaxPages] = {};
static int  s_pageCount = 0;
static int  s_target = 0;     // page the strip moves to (0 = item grid)
static f32  s_strip = 0.0f;   // eased position of the strip, in pages
static int  s_p2sel = -1;     // selected element of the target page, -1 = cursor in the item rows

alignas(8) static unsigned char s_pagePool[sizeof(cl::Page) * cl::Page::kMaxPages];
static bool s_pagePoolUsed[cl::Page::kMaxPages] = {};

void* cl::Page::operator new(std::size_t size) {
    if (size != sizeof(cl::Page)) return nullptr;
    for (int i = 0; i < kMaxPages; i++) {
        if (!s_pagePoolUsed[i]) {
            s_pagePoolUsed[i] = true;
            return s_pagePool + i * sizeof(cl::Page);
        }
    }
    return nullptr;
}

void cl::Page::operator delete(void* ptr) noexcept {
    if (ptr == nullptr) return;
    const std::size_t off = static_cast<unsigned char*>(ptr) - s_pagePool;
    if (off % sizeof(cl::Page) == 0 && off < sizeof(s_pagePool)) {
        s_pagePoolUsed[off / sizeof(cl::Page)] = false;
    }
}

cl::Page::Page() {
    if (s_pageCount < kMaxPages) {
        s_pages[s_pageCount++] = this;
    }
}

cl::Page::~Page() {
    for (int i = 0; i < s_pageCount; i++) {
        if (s_pages[i] == this) {
            for (int j = i + 1; j < s_pageCount; j++) s_pages[j - 1] = s_pages[j];
            s_pageCount--;
            break;
        }
    }
    collection_page_reset();
}

cl::Element* cl::Page::add(const Element& element) {
    if (element.paneTag == 0) return nullptr;
    if (mElementCount >= kMaxElements) return nullptr;
    mElements[mElementCount] = element;
    mPrimaryPane[mElementCount] = nullptr;
    mFollowerPane[mElementCount] = nullptr;
    return &mElements[mElementCount++];
}

cl::Element* cl::Page::add(u64 paneTag) {
    Element e;
    e.paneTag = paneTag;
    return add(e);
}

cl::Element cl::heart() {
    Element e;
    e.paneTag = MULTI_CHAR('heart_n');
    e.hideOnMain = true;
    e.claimsCell = true;
    e.cellX = 5;
    e.cellY = 0;
    return e;
}

cl::Element cl::fused_shadow() {
    Element e;
    e.paneTag = MULTI_CHAR('kamen_n');
    e.followerTag = MULTI_CHAR('modelbgn');
    e.followerDx = -13.0f;
    e.followerDy = -22.0f;
    e.claimsCell = true;
    e.cellX = 6;
    e.cellY = 0;
    return e;
}

cl::Element cl::crystal() {
    Element e;
    e.paneTag = MULTI_CHAR('crystal');
    return e;
}

static f32 smoothstep(f32 t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static f32 page_slide_w() {
    f32 w = mDoGph_gInf_c::getWidthF();
    return (w > 100.0f && w < 4000.0f) ? w : 640.0f;
}

static f32 page_offset(int pageNo) {
    const f32 d = static_cast<f32>(pageNo) - s_strip;
    if (d >= 1.0f) return page_slide_w();
    if (d <= -1.0f) return -page_slide_w();
    if (d >= 0.0f) return smoothstep(d) * page_slide_w();
    return -smoothstep(-d) * page_slide_w();
}

static bool page_near(int pageNo) {
    const f32 d = static_cast<f32>(pageNo) - s_strip;
    return d > -1.0f && d < 1.0f;
}

static bool navigable(const cl::Page* pg, int i) {
    return pg->mElements[i].selectable && pg->mPrimaryPane[i] != nullptr;
}

static int first_navigable(const cl::Page* pg) {
    if (pg == nullptr) return -1;
    for (int i = 0; i < pg->mElementCount; i++) {
        if (navigable(pg, i)) return i;
    }
    return -1;
}

static int next_navigable(const cl::Page* pg, int from) {
    for (int i = from + 1; i < pg->mElementCount; i++) {
        if (navigable(pg, i)) return i;
    }
    return -1;
}

static int prev_navigable(const cl::Page* pg, int from) {
    for (int i = from - 1; i >= 0; i--) {
        if (navigable(pg, i)) return i;
    }
    return -1;
}

static void element_slot(const cl::Page* pg, int slotIndex, int slotCount, const cl::Element& e,
                         f32& x, f32& y) {
    if (e.hasPos) {
        x = e.posX;
        y = e.posY;
        return;
    }
    const f32 n = static_cast<f32>(slotCount);
    x = pg->mAnchorX + (static_cast<f32>(slotIndex) - 0.5f * (n - 1.0f)) * pg->mSpacing;
    y = pg->mAnchorY;
}

static constexpr int kMaxGridPanes = 96;

static void fade_grid(u8 a) {
    J2DPane* panes[kMaxGridPanes];
    const int n = screen_grid_panes(panes, kMaxGridPanes);
    for (int i = 0; i < n; i++) panes[i]->setAlpha(a);
}

// While sliding, grid panes left of the grid frame would cross the Link doll: hide them.
static constexpr f32 kGridFrameLeftEdge = -117.5f;

static void grid_mask_beyond_frame() {
    J2DPane* panes[kMaxGridPanes];
    const int n = screen_grid_panes(panes, kMaxGridPanes);
    for (int i = 0; i < n; i++) {
        if (panes[i]->getTranslateX() < kGridFrameLeftEdge) panes[i]->hide();
    }
}

// Name / description of the selected page element (native text of the cell it claims).
static void show_element_name(dMenu_Collect2D_c* c) {
    if (s_target < 1 || s_p2sel < 0) return;
    const cl::Element& e = s_pages[s_target - 1]->mElements[s_p2sel];
    if (e.claimsCell) {
        screen_show_native_name(c, e.cellX, e.cellY);
    } else {
        c->setItemNameStringNull();
    }
}

void collection_page_reset() {
    s_target = 0;
    s_strip = 0.0f;
    s_p2sel = -1;
}

void collection_page_teardown() {
    collection_page_reset();
    for (int k = 0; k < s_pageCount; k++) {
        cl::Page* pg = s_pages[k];
        pg->mRootPane = nullptr;
        pg->mScreen = nullptr;
        for (int i = 0; i < cl::Page::kMaxElements; i++) {
            pg->mPrimaryPane[i] = nullptr;
            pg->mFollowerPane[i] = nullptr;
        }
    }
}

static void ease_strip(f32 tgt) {
    using clock = std::chrono::steady_clock;
    static clock::time_point s_last = clock::now();
    const clock::time_point now = clock::now();
    f32 dt = std::chrono::duration<f32>(now - s_last).count();
    s_last = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    s_strip += (tgt - s_strip) * (1.0f - std::exp(-21.0f * dt));
    if (tgt - s_strip < 0.0004f && s_strip - tgt < 0.0004f) s_strip = tgt;
}

void collection_page_update() {
    ease_strip(static_cast<f32>(s_target));
}

bool collection_page_active() {
    return s_target != 0 || s_strip > 0.0f;
}

bool collection_page_p2_focused() {
    return s_target >= 1 && s_p2sel >= 0;
}

bool collection_page_on_page() {
    return s_target >= 1;
}

f32 collection_page_grid_dx() {
    const f32 t = (s_strip < 1.0f) ? s_strip : 1.0f;
    return -smoothstep(t) * page_slide_w();
}

static bool pages_enabled() {
    return s_pageCount > 0 && !cl_hd_layout_requested();
}

bool collection_page_claims_cell(u8 x, u8 y) {
    if (!pages_enabled()) return false;
    for (int k = 0; k < s_pageCount; k++) {
        const cl::Page* pg = s_pages[k];
        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            if (e.claimsCell && e.cellX == x && e.cellY == y) return true;
        }
    }
    return false;
}

static void page_attach(cl::Page* pg, J2DPane* pane, int k, J2DScreen* screen) {
    if (pg->mRootPane == nullptr) {
        // The root sits where the pane's old parents put it, so page coordinates keep
        // meaning what they meant in the layout.
        f32 tx = 0.0f, ty = 0.0f;
        for (J2DPane* p = pane->getParentPane();
             p != nullptr && p != static_cast<J2DPane*>(screen);
             p = p->getParentPane()) {
            tx += p->getTranslateX();
            ty += p->getTranslateY();
        }

        pg->mRootTag = 0x636C506700ULL + static_cast<u64>(k);
        JGeometry::TBox2<f32> empty;
        empty.set(0.0f, 0.0f, 0.0f, 0.0f);
        J2DPane* root = JKR_NEW J2DPane(screen, true, pg->mRootTag, empty);
        if (root == nullptr) return;
        root->setBasePosition(J2DBasePosition_0);
        root->translate(tx, ty);
        pg->mRootPane = root;
    }
    pg->mRootPane->appendChild(pane);
}

void collection_page_sync_screen(J2DScreen* screen) {
    if (screen == nullptr || !pages_enabled()) return;
    for (int k = 0; k < s_pageCount; k++) {
        cl::Page* pg = s_pages[k];
        if (pg->mScreen != screen) {
            pg->mRootPane = nullptr;
            for (int i = 0; i < cl::Page::kMaxElements; i++) {
                pg->mPrimaryPane[i] = nullptr;
                pg->mFollowerPane[i] = nullptr;
            }
            pg->mScreen = screen;
        }

        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            if (pg->mPrimaryPane[i] == nullptr && e.paneTag != 0) {
                J2DPane* pane = screen->search(e.paneTag);
                if (pane != nullptr) {
                    page_attach(pg, pane, k, screen);
                    pg->mPrimaryPane[i] = pane;
                }
            }
            if (pg->mFollowerPane[i] == nullptr && e.followerTag != 0) {
                J2DPane* pane = screen->search(e.followerTag);
                if (pane != nullptr) {
                    page_attach(pg, pane, k, screen);
                    pg->mFollowerPane[i] = pane;
                }
            }
        }
    }
}

// Back on the item grid: cursor, name and A button of the grid cell again.
static void return_to_grid(dMenu_Collect2D_c* c) {
    c->cursorPosSet();
    c->setItemNameString(c->mCursorX, c->mCursorY);
}

bool collection_page_focus_first(dMenu_Collect2D_c* c) {
    if (c == nullptr || s_target < 1) return false;
    const int first = first_navigable(s_pages[s_target - 1]);
    if (first < 0) return false;
    s_p2sel = first;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    show_element_name(c);
    return true;
}

void collection_page_handle_input(dMenu_Collect2D_c* c) {
    if (c == nullptr || !pages_enabled()) return;

    const int prevPage = s_target;
    if (mDoCPd_c::getTrigR(PAD_1)) {
        if (s_target < s_pageCount) s_target++;
    } else if (mDoCPd_c::getTrigL(PAD_1)) {
        if (s_target > 0) s_target--;
    }
    if (s_target != prevPage) {
        if (c->mpDrawCursor != nullptr) c->mpDrawCursor->onPlayAllAnime();
        Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CHANGE_WINDOW, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);

        if (s_target >= 1) {
            s_p2sel = first_navigable(s_pages[s_target - 1]);
            if (s_p2sel >= 0) {
                show_element_name(c);
            } else if (c->mCursorY < kClRows) {
                // Nothing to select on this page: the grid cursor would sit off screen.
                c->mCursorX = 3;
                c->mCursorY = 3;
                return_to_grid(c);
            }
        } else {
            s_p2sel = -1;
            return_to_grid(c);
        }
        return;
    }

    const bool onTarget = s_target >= 1 &&
                          s_strip - static_cast<f32>(s_target) < 0.5f &&
                          static_cast<f32>(s_target) - s_strip < 0.5f;
    if (!onTarget || s_p2sel < 0) return;

    bool right = dMw_RIGHT_TRIGGER() != 0;
    bool left = dMw_LEFT_TRIGGER() != 0;
    bool down = dMw_DOWN_TRIGGER() != 0;
    if (c->mpStick != nullptr) {
        c->mpStick->checkTrigger();
        if (c->mpStick->checkRightTrigger()) right = true;
        if (c->mpStick->checkLeftTrigger()) left = true;
        if (c->mpStick->checkDownTrigger()) down = true;
    }

    const cl::Page* pg = s_pages[s_target - 1];
    const int prevSel = s_p2sel;

    if (right) {
        const int nxt = next_navigable(pg, s_p2sel);
        if (nxt >= 0) s_p2sel = nxt;
    } else if (left || down) {
        const int prv = left ? prev_navigable(pg, s_p2sel) : -1;
        if (prv >= 0) {
            s_p2sel = prv;
        } else {
            // Off the page into the item rows below.
            s_p2sel = -1;
            c->mCursorX = 3;
            c->mCursorY = 3;
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            return_to_grid(c);
            return;
        }
    }

    if (s_p2sel != prevSel) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        show_element_name(c);
    }
}

void collection_page_apply(dMenu_Collect2D_c* c) {
    if (c == nullptr || c->mpScreen == nullptr || !pages_enabled()) return;

    ease_strip(static_cast<f32>(s_target));

    const f32 p = smoothstep((s_strip < 1.0f) ? s_strip : 1.0f);
    const bool showPage = p > 0.001f;

    const f32 fadeT = smoothstep(p < 0.6f ? p / 0.6f : 1.0f);
    fade_grid(static_cast<u8>(255.0f * (1.0f - fadeT)));
    if (s_strip > 0.001f) grid_mask_beyond_frame();

    for (int k = 0; k < s_pageCount; k++) {
        cl::Page* pg = s_pages[k];
        const int pageNo = k + 1;
        const bool pageVisible = showPage && page_near(pageNo);
        const f32 pageSlide = page_offset(pageNo);

        int slotCount = 0;
        for (int i = 0; i < pg->mElementCount; i++) {
            if (pg->mPrimaryPane[i] != nullptr) slotCount++;
        }

        int slotIndex = 0;
        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            J2DPane* prim = pg->mPrimaryPane[i];
            J2DPane* foll = pg->mFollowerPane[i];
            if (prim == nullptr) continue;

            f32 x, y;
            element_slot(pg, slotIndex, slotCount, e, x, y);
            slotIndex++;
            cl_set_pane_pos(prim, x + pageSlide, y);
            if (foll != nullptr) cl_set_pane_pos(foll, x + e.followerDx + pageSlide, y + e.followerDy);

            if (e.hideOnMain) {
                if (pageVisible) prim->show(); else prim->hide();
            }
        }
    }

    if (showPage && s_target >= 1 && s_p2sel >= 0 && c->mpDrawCursor != nullptr) {
        const cl::Page* pg = s_pages[s_target - 1];
        J2DPane* sel = s_p2sel < pg->mElementCount ? pg->mPrimaryPane[s_p2sel] : nullptr;
        if (sel != nullptr) {
            const cl::Element& e = pg->mElements[s_p2sel];
            const Vec pos = cl_pane_global_center(sel);
            c->mpDrawCursor->setAlphaRate(1.0f);
            c->mpDrawCursor->setPos(pos.x, pos.y, sel, false);
            if (e.claimsCell && e.cellX == 6 && e.cellY == 0) {
                c->mpDrawCursor->setParam(0.6f, 0.85f, 0.03f, 0.6f, 0.6f);   // native fused-shadow cursor
            } else {
                c->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.7f, 0.7f);
            }
        }
    }
}
