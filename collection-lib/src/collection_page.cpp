#include "collection_page.hpp"

#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"
#include "d/d_menu_window.h"     // dMw_LEFT/RIGHT_TRIGGER
#include "d/d_select_cursor.h"   // dSelect_cursor_c
#include "d/d_lib.h"             // STControl (mpStick - the analog cursor input)
#include "Z2AudioLib/Z2SeMgr.h"

#include <chrono>
#include <cmath>

// ---------------------------------------------------------------------------
// Page engine
//
// Pages are consumer-created via cl::Page (collection_lib/collection_page.hpp).
// Everything below degrades to a no-op when no page exists: the grid keeps its
// vanilla layout, the heart stays selectable in its grid cell and R/L do
// nothing.
//
// Motion model: the pages (main grid = slot 0, first created page = slot 1,
// ...) sit on one horizontal strip. s_strip is the eased camera position on
// that strip; a page's on-screen offset is (pageNo - s_strip) slots, shaped by
// smoothstep. Every R/L transition therefore slides - main<->page exactly like
// the old hardcoded page did, and page<->page slides the outgoing page left
// while the incoming one enters from the right.
// ---------------------------------------------------------------------------

static cl::Page* s_pages[cl::Page::kMaxPages] = {};
static int  s_pageCount = 0;
static int  s_target = 0;    // 0 = main grid, 1..n = page (index + 1)
static f32  s_strip = 0.0f;  // eased camera position on the page strip
static int  s_p2sel = -1;    // page cursor: element index within the target page

// --- cl::Page -----------------------------------------------------------------

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
    return nullptr;   // pool exhausted (kMaxPages) - the caller must check
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
    collection_page_reset();   // engine may have been focused on this page
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
    e.cellX = 6;
    e.cellY = 0;
    return e;
}

cl::Element cl::fused_shadow() {
    Element e;
    e.paneTag = MULTI_CHAR('kamen_n');
    e.followerTag = MULTI_CHAR('modelbgn');
    // Backdrop plate rides up-left of the mirror pane - the offsets the old
    // hardcoded page-2 layout was tuned to.
    e.followerDx = -13.0f;
    e.followerDy = -22.0f;
    return e;
}

cl::Element cl::crystal() {
    Element e;
    // The vanilla collection layout has no crystal pane yet - adjust this tag
    // once a crystal pane exists. Until then the element stays unresolved:
    // invisible, no layout slot, skipped by the page cursor.
    e.paneTag = MULTI_CHAR('crystal');
    return e;
}

// --- engine helpers -----------------------------------------------------------

static f32 smoothstep(f32 t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static f32 page_slide_w() {
    f32 w = mDoGph_gInf_c::getWidthF();
    return (w > 100.0f && w < 4000.0f) ? w : 640.0f;
}

// On-screen X offset of page `pageNo` for the current strip position: 0 while
// it is the focused page, +W when parked to the right, -W when parked to the
// left, smoothstepped in between (that shaped ease is what the old hardcoded
// single page used).
static f32 page_offset(int pageNo) {
    const f32 d = static_cast<f32>(pageNo) - s_strip;
    if (d >= 1.0f) return page_slide_w();
    if (d <= -1.0f) return -page_slide_w();
    if (d >= 0.0f) return smoothstep(d) * page_slide_w();
    return -smoothstep(-d) * page_slide_w();
}

// A page is on/near the screen while its slot is within one slot of the camera.
static bool page_near(int pageNo) {
    const f32 d = static_cast<f32>(pageNo) - s_strip;
    return d > -1.0f && d < 1.0f;
}

// An element may take the page cursor only when it is marked selectable AND
// its pane actually resolved against the live screen - placeholder elements
// (e.g. a tag that does not exist in the layout yet) must be invisible to the
// navigation, otherwise the cursor would focus an empty slot.
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

// Default layout: element i of n spaced evenly around the page anchor. The
// defaults (anchor 107/-36, spacing 122) put two elements exactly where the old
// hardcoded page-2 had them: heart at (46,-36), fused shadow at (168,-36).
// Default layout: element i of n spaced evenly around the page anchor. The
// defaults (anchor 107/-36, spacing 122) put two elements exactly where the old
// hardcoded page-2 had them: heart at (46,-36), fused shadow at (168,-36).
// slotIndex/slotCount count over RESOLVED elements only - placeholders whose
// pane never resolved must not shift the others or leave a gap in the row.
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

// Every pane that belongs to the item grid (containers - child icons/pics ride
// the parent alpha). Faded out as the grid slides so it doesn't smear over the
// Link doll on the way off-screen.
static const u64 kGridTags[] = {
    MULTI_CHAR('ken_n0'),  MULTI_CHAR('ken_n1'),
    MULTI_CHAR('tate_n0'), MULTI_CHAR('tate_n1'),
    MULTI_CHAR('fuku_n0'), MULTI_CHAR('fuku_n1'), MULTI_CHAR('fuku_n2'),
    MULTI_CHAR('ken_g_0'),  MULTI_CHAR('ken_g_1'),
    MULTI_CHAR('tate_g_0'), MULTI_CHAR('tate_g_1'),
    MULTI_CHAR('fuku_g_0'), MULTI_CHAR('fuku_g_1'), MULTI_CHAR('fuku_g_2'),
    MULTI_CHAR('tunagi00'), MULTI_CHAR('tunagi01'), MULTI_CHAR('tunagi03'),
    MULTI_CHAR('tunagi04'), MULTI_CHAR('tunagi06'), MULTI_CHAR('tunagi07'),
    MULTI_CHAR('tunagi08'),
    MULTI_CHAR('tuna_k2'), MULTI_CHAR('tuna_t2'), MULTI_CHAR('tuna_f3'),
    MULTI_CHAR('ken_mid'), MULTI_CHAR('tate_mid'), MULTI_CHAR('fuku_ord'),
    MULTI_CHAR('ken_gm'),  MULTI_CHAR('tate_gm'),  MULTI_CHAR('fuku_go'),
    MULTI_CHAR('fuku_her'),
};

static void fade_grid(J2DScreen* s, u8 a) {
    for (u64 tag : kGridTags) {
        if (J2DPane* p = s->search(tag)) p->setAlpha(a);
    }
    for (int i = 0; i < slot_count(); i++) {
        const SlotSpec* slot = slot_get(i);
        if (slot && slot->autoLayout.on) {
            if (slot->icon) slot->icon->setAlpha(a);
            if (slot->frame) slot->frame->setAlpha(a);
        }
    }
    for (int i = 0; i < s_customConnectorCount; i++) {
        if (s_customConnectors[i]) s_customConnectors[i]->setAlpha(a);
    }
}

// The grid's inner left edge in row space: the leftmost column the layout can
// ever produce is ken_n0 at -93 (starter-gear slot off shifts the row one
// column left), its frame reaches ~24 further - everything left of -117.5 is
// outside the frame.
static constexpr f32 kGridFrameLeftEdge = -117.5f;

// While the grid slides, HIDE every grid pane once it passes the frame's left
// edge - the slots (frame pictures included - alpha alone doesn't stick for
// them) must vanish behind the frame instead of drifting across the Link
// doll. Vanilla re-evaluates the visibility of every one of these panes each
// frame, so they re-appear on their own when the slide brings them back
// inside the frame.
static void grid_mask_beyond_frame(J2DScreen* s, f32 dx) {
    for (u64 tag : kGridTags) {
        J2DPane* p = s->search(tag);
        if (p != nullptr && p->getTranslateX() + dx < kGridFrameLeftEdge) p->hide();
    }
    for (int i = 0; i < slot_count(); i++) {
        const SlotSpec* slot = slot_get(i);
        if (slot && slot->autoLayout.on) {
            if (slot->icon != nullptr && slot->icon->getTranslateX() + dx < kGridFrameLeftEdge) {
                slot->icon->hide();
            }
            if (slot->frame != nullptr && slot->frame->getTranslateX() + dx < kGridFrameLeftEdge) {
                slot->frame->hide();
            }
        }
    }
    for (int i = 0; i < s_customConnectorCount; i++) {
        J2DPane* p = s_customConnectors[i];
        if (p != nullptr && p->getTranslateX() + dx < kGridFrameLeftEdge) p->hide();
    }
}

// --- engine entry points ------------------------------------------------------

// Resets only the ANIMATION state (menu always reopens on the main page). The
// pane references are deliberately KEPT: on TARGET_PC menuCollectWide() runs
// inside dMenu_Collect2D_c::_create, which means the first sync has already
// happened by the time the create-post hook resets - clearing refs here would
// leave the pages pane-less (update_screen_bases skips re-syncing because the
// screen pointer hasn't changed). Stale refs can't happen: teardown() drops
// them before the screen is deleted, and the incremental sync repairs
// everything else.
void collection_page_reset() {
    s_target = 0;
    s_strip = 0.0f;
    s_p2sel = -1;
}

// The menu's screen is being deleted (or the mod is shutting down): drop every
// pane reference. The pages themselves (tags, layout) survive and re-resolve
// on the next screen build.
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

// Render-rate easing for the page strip. apply() runs BOTH in the simulation
// hooks and in the draw-phase wide hook (menuCollectWide from _draw), so the
// strip is eased with wall-clock delta time: the animation advances every
// rendered frame instead of stepping per simulation tick, and the speed is
// identical at any framerate. Exponential smoothing composes across the
// interleaved calls (the elapsed time is only ever counted once), so no
// phase tracking is needed. Mirrors what dusk::interp does for the item
// wheel / map, whose capture state mods cannot reach.
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
    if (!is_collection_menu_enabled()) {
        collection_page_reset();
        return;
    }
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
    // The main grid slides one slot left at most, however many pages exist.
    const f32 t = (s_strip < 1.0f) ? s_strip : 1.0f;
    return -smoothstep(t) * page_slide_w();
}

bool collection_page_claims_cell(u8 x, u8 y) {
    for (int k = 0; k < s_pageCount; k++) {
        const cl::Page* pg = s_pages[k];
        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            if (e.claimsCell && e.cellX == x && e.cellY == y) return true;
        }
    }
    return false;
}

// Creates (once per screen) the page's container pane and re-parents `pane`
// into it - appendChild (JSUPtrList::append) removes the pane from its
// previous parent's child list first, so the vanilla tree stays intact.
//
// The container is hosted directly under the SCREEN root: as its last child it
// draws after every top-level group (Link doll, frame graphics), so page
// elements are never occluded by them. The original parent's accumulated
// translate is baked into the container so element coordinates stay in the
// same frame the vanilla panes (and the tuned layout) live in - translation
// only; a scaled ancestor would need matrix compensation.
static void page_attach(cl::Page* pg, J2DPane* pane, int k, J2DScreen* screen) {
    if (pg->mRootPane == nullptr) {
        f32 tx = 0.0f, ty = 0.0f;
        for (J2DPane* p = pane->getParentPane();
             p != nullptr && p != static_cast<J2DPane*>(screen);
             p = p->getParentPane()) {
            tx += p->getTranslateX();
            ty += p->getTranslateY();
        }

        pg->mRootTag = 0x636C506700ULL + static_cast<u64>(k);   // 'clPg' + page index
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

// Incremental + idempotent: (re-)resolves and attaches whatever is still
// missing. Safe to call every frame - a settled page costs a few pointer
// compares per element. This is also what repairs pages whose pane tags only
// resolve against a later screen state.
void collection_page_sync_screen(J2DScreen* screen) {
    if (screen == nullptr) return;
    for (int k = 0; k < s_pageCount; k++) {
        cl::Page* pg = s_pages[k];
        if (pg->mScreen != screen) {
            // A screen the page hasn't seen: its old panes are gone.
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

void collection_page_handle_input(dMenu_Collect2D_c* collect2D) {
    if (collect2D == nullptr || s_pageCount == 0) return;

    // --- R / L page toggle ---
    const int prevPage = s_target;
    if (mDoCPd_c::getTrigR(PAD_1)) {
        if (s_target < s_pageCount) s_target++;
    } else if (mDoCPd_c::getTrigL(PAD_1)) {
        if (s_target > 0) s_target--;
    }
    if (s_target != prevPage) {
        if (s_target >= 1) {
            s_p2sel = first_navigable(s_pages[s_target - 1]);
        } else {
            s_p2sel = -1;            // s_animPage keeps pointing at the page sliding out
        }
        collect2D->setItemNameStringNull();
        // The vanilla cursor-pulse anime is (re-)armed by the wait_proc paths
        // we skip while a page is focused - re-arm it here so the selection
        // cursor keeps animating on the page.
        if (collect2D->mpDrawCursor != nullptr) {
            collect2D->mpDrawCursor->onPlayAllAnime();
        }
        Z2GetAudioMgr()->seStart(Z2SE_SY_MENU_CHANGE_WINDOW, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }

    // --- On-page element navigation ---
    // ONLY while a page element is focused (s_p2sel >= 0). Once dropped into
    // the grid (s_p2sel < 0) we must NOT touch mpStick - STControl::check*Trigger
    // latches a repeat-delay, so reading it here would starve the vanilla
    // cursorMove() and make grid nav stutter. on_wait_proc_pre lets wait_proc run
    // in that state (collection_page_p2_focused() == false), and the "walked up
    // out of the collection items onto the page" pop is handled in
    // collection_page_apply.
    const bool onTarget = s_target >= 1 &&
                          s_strip - static_cast<f32>(s_target) < 0.5f &&
                          static_cast<f32>(s_target) - s_strip < 0.5f;
    if (onTarget && s_p2sel >= 0) {
        const int prevSel = s_p2sel;
        bool right = dMw_RIGHT_TRIGGER() != 0;
        bool left  = dMw_LEFT_TRIGGER() != 0;
        bool down  = dMw_DOWN_TRIGGER() != 0;
        if (collect2D->mpStick) {
            collect2D->mpStick->checkTrigger();
            if (collect2D->mpStick->checkRightTrigger()) right = true;
            if (collect2D->mpStick->checkLeftTrigger())  left  = true;
            if (collect2D->mpStick->checkDownTrigger())  down  = true;
        }

        const cl::Page* pg = s_pages[s_target - 1];

        // Page switching is L/R shoulder ONLY - LEFT/DOWN here drops the selection
        // into the grid slots below (never a page flip).
        auto drop_to_grid = [&]() {
            s_p2sel = -1;
            collect2D->mCursorX = 3;
            collect2D->mCursorY = 3;
            collect2D->cursorPosSet();
            collect2D->setItemNameString(3, 3);
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        };

        if (right) {
            const int nxt = next_navigable(pg, s_p2sel);
            if (nxt >= 0) s_p2sel = nxt;
        } else if (left) {
            const int prv = prev_navigable(pg, s_p2sel);
            if (prv >= 0) s_p2sel = prv;
            else { drop_to_grid(); return; }
        } else if (down) {
            drop_to_grid();
            return;
        }

        if (s_p2sel != prevSel) {
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }
}

void collection_page_apply(dMenu_Collect2D_c* collect2D) {
    if (collect2D == nullptr || collect2D->mpScreen == nullptr || s_pageCount == 0) return;
    J2DScreen* screen = collect2D->mpScreen;

    // Self-healing: incremental sync repairs pages whose panes haven't been
    // resolved/attached yet. Settled pages cost a few pointer compares.
    {
        JKRExpHeap* heap = collect2D->mpHeap;
        JKRHeap* oldHeap = (heap != nullptr) ? mDoExt_setCurrentHeap(heap) : nullptr;
        collection_page_sync_screen(screen);
        if (oldHeap != nullptr) {
            mDoExt_setCurrentHeap(oldHeap);
        }
    }

    // Render-rate easing: in the draw phase this call runs per rendered frame
    // (menuCollectWide from _draw), so the strip - and with it every grid and
    // page position below - advances smoothly between simulation ticks.
    ease_strip(static_cast<f32>(s_target));

    const f32 p = smoothstep((s_strip < 1.0f) ? s_strip : 1.0f);
    const bool showPage = p > 0.001f;
    const f32 dTgt = static_cast<f32>(s_target) - s_strip;
    const bool onTargetPage = dTgt > -1.0f && dTgt < 1.0f;

    // Grid fades to nothing over the first 60% of the slide away from the main
    // grid; it stays gone while browsing deeper pages.
    const f32 fadeT = smoothstep(p < 0.6f ? p / 0.6f : 1.0f);
    fade_grid(screen, static_cast<u8>(255.0f * (1.0f - fadeT)));

    // While ANY page is showing, grid content that slid past the frame's left
    // edge is hidden outright (the frame pictures included - alpha alone
    // doesn't fully stick for them). On a page the grid is one full width
    // left, so this hides everything; on the way back vanilla re-shows each
    // pane as it re-enters the frame.
    if (s_strip > 0.001f) {
        grid_mask_beyond_frame(screen, collection_page_grid_dx());
    }

    for (int k = 0; k < s_pageCount; k++) {
        cl::Page* pg = s_pages[k];
        const int pageNo = k + 1;
        const bool pageVisible = showPage && page_near(pageNo);
        const f32 pageSlide = page_offset(pageNo);

        // Only RESOLVED elements participate: an element whose pane tag never
        // resolved (placeholder) occupies no layout slot, so the others don't
        // shift for it and no gap is left in the row.
        int slotCount = 0;
        for (int i = 0; i < pg->mElementCount; i++) {
            if (pg->mPrimaryPane[i] != nullptr) slotCount++;
        }

        int slotIndex = 0;
        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            J2DPane* prim = pg->mPrimaryPane[i];
            J2DPane* foll = pg->mFollowerPane[i];
            if (prim == nullptr) continue;   // unresolved: nothing to position

            f32 x, y;
            element_slot(pg, slotIndex, slotCount, e, x, y);
            slotIndex++;
            set_pane_pos(prim, x + pageSlide, y);
            if (foll != nullptr) set_pane_pos(foll, x + e.followerDx + pageSlide, y + e.followerDy);

            // hideOnMain elements are hidden on the main page (the heart);
            // everything else just sits parked off the edge.
            if (e.hideOnMain) {
                if (pageVisible) prim->show(); else prim->hide();
            }
        }

        // Claimed grid cells are not selectable while their page exists - the
        // element lives on the page instead.
        for (int i = 0; i < pg->mElementCount; i++) {
            const cl::Element& e = pg->mElements[i];
            if (!e.claimsCell || slot_at(e.cellX, e.cellY) != nullptr) continue;
            collect2D->field_0x22d[e.cellX][e.cellY] = 0;
            if (!pageVisible && collect2D->mCursorX == e.cellX && collect2D->mCursorY == e.cellY) {
                if (e.cellX > 0) collect2D->mCursorX = static_cast<u8>(e.cellX - 1);
            }
        }
    }

    if (showPage && s_target >= 1 && s_p2sel >= 0) {
        collect2D->setItemNameStringNull();

        // Draw the selection rect on the focused page element.
        const cl::Page* pg = s_pages[s_target - 1];
        if (collect2D->mpDrawCursor && pg != nullptr && s_p2sel < pg->mElementCount) {
            J2DPane* sel = pg->mPrimaryPane[s_p2sel];
            if (sel != nullptr) {
                collect2D->mpDrawCursor->setAlphaRate(1.0f);
                collect2D->mpDrawCursor->setPos(sel->getTranslateX(), sel->getTranslateY(), sel, false);
                collect2D->mpDrawCursor->setParam(1.0f, 1.0f, 0.1f, 0.7f, 0.7f);
            }
        }
    } else if (showPage && s_target >= 1 && s_p2sel < 0 && onTargetPage && collect2D->mCursorY <= 2) {
        // In the grid section, vanilla cursorMove walked the cursor up out of the
        // collection items into the (hidden, slid-off) equipment rows -> pop back
        // onto the page's first navigable element.
        cl::Page* pg = s_pages[s_target - 1];
        const int first = first_navigable(pg);
        if (first >= 0) {
            s_p2sel = first;
            const cl::Element& e = pg->mElements[first];
            if (e.claimsCell) {
                collect2D->mCursorX = e.cellX;
                collect2D->mCursorY = e.cellY;
            } else {
                collect2D->mCursorX = 6;
                collect2D->mCursorY = 0;
            }
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }

    // The Link doll (linki_n) is deliberately left alone - it stays on both pages.
}
