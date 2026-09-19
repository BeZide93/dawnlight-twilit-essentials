#pragma once

// dusklight-collection-lib - Pages
//
// A Page is a full-screen layer next to the main item grid. The consumer builds
// one by creating a Page and adding elements to it:
//
//     cl::Page* p2 = new cl::Page();
//     p2->add(cl::heart());
//     p2->add(cl::fused_shadow());
//
// add() re-parents the element's pane into the page: the pane is removed from
// whatever parent pane it currently hangs under (the vanilla layout group) and
// appended to the page's own container pane - JSystem's JSUPtrList::append
// detaches it from the old parent automatically. From then on the page owns
// the pane's position: every frame it lays the element out (default: elements
// spaced evenly around the page anchor, so a freshly created page with heart +
// fused shadow looks exactly like the old hardcoded second page - both items
// side by side, roughly screen-centred, clearing the Link doll), slides the
// page in from the right on R and back out on L, runs the page's own cursor
// and neutralizes the oversized vanilla hit-boxes while the items are parked
// off-screen.
//
// Pages must be created BEFORE collectionlib_init() (they live in a small
// static pool) and survive screen rebuilds: elements are stored as pane tags
// and re-resolved against every newly built collection screen. No page
// registered == no second page: every page feature degrades to vanilla
// behaviour.

#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DPane.h"

#include "dolphin/types.h"
#include <cstddef>

namespace cl {

// One item on a Page. Plain value struct - brace-initialize or use the
// factories below, then hand it to Page::add().
struct Element {
    u64 paneTag = 0;        // primary pane, resolved by tag on every screen build (required)
    u64 followerTag = 0;    // optional second pane that rides along (e.g. a 3D-model backdrop)
    f32 followerDx = 0.0f;  // follower offset from the primary pane's layout position
    f32 followerDy = 0.0f;

    bool hideOnMain = false;  // hide() on the main page instead of parking off-screen right
    bool selectable = true;   // participates in the page's own cursor (left/right, focus rect)

    // The element takes over a main-grid cursor cell: the cell becomes
    // non-selectable on the main page and the cursor pops back to this element
    // when the player walks up out of the grid. (Used by the heart container,
    // whose vanilla grid slot it owns.)
    bool claimsCell = false;
    u8   cellX = 0, cellY = 0;

    // Explicit position override in page space. Leave unset for the default
    // layout (elements spaced evenly around the page anchor - see Page).
    bool hasPos = false;
    f32  posX = 0.0f, posY = 0.0f;
};

// The vanilla Heart Container ('heart_n'). Owns its main-grid cell (6,0): while
// the page exists the cell is not selectable on the main page and the heart
// lives on the page instead.
Element heart();

// The Mirror of Twilight ('kamen_n') together with its 3D-model backdrop plate
// ('modelbgn'). The mirror's 3D model tracks the pane's centre on its own.
Element fused_shadow();

// Placeholder element for a crystal pane ('crystal'). The vanilla collection
// layout has no crystal pane yet - until the tag points at a pane that exists,
// the element stays invisible and occupies NO layout slot and is skipped by
// the page cursor (the other elements don't shift for it).
Element crystal();

struct Page {
public:
    // Pages come from a small static pool (no heap). Registration in the page
    // engine happens here - a constructed Page is immediately live.
    Page();
    ~Page();
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    void* operator new(std::size_t size);
    void  operator delete(void* ptr) noexcept;

    // Adds an element to this page: its pane is removed from its current
    // parent and appended to the page's container (the physical re-parent
    // happens when the next collection screen is built - before that only the
    // pane tags exist). The page owns the pane's position from that point on.
    // Returns the stored copy (for per-element tweaks), or nullptr when the
    // page is full.
    Element* add(const Element& element);

    // Convenience: add(Element) for just a pane tag (selectable, no follower,
    // default parking behaviour).
    Element* add(u64 paneTag);

    int element_count() const { return mElementCount; }

    // Default layout ("relativ mittig"): element i of n sits at
    //   (anchorX + (i - (n-1)/2) * spacing, anchorY)
    // in the page's coordinate space (the frame of the first element's original
    // parent). The defaults reproduce the old hardcoded second page: heart
    // left of the anchor, fused shadow right of it, both just above vertical
    // screen centre, clearing the Link doll in the middle.
    void set_anchor(f32 x, f32 y) { mAnchorX = x; mAnchorY = y; }
    void set_spacing(f32 spacing) { mSpacing = spacing; }

    static constexpr int kMaxPages = 4;
    static constexpr int kMaxElements = 8;

    // --- engine-owned data (managed by the library - treat as read-only) ---

    u64       mRootTag = 0;
    J2DPane*  mRootPane = nullptr;   // the page's container pane (created per screen)
    J2DScreen* mScreen = nullptr;    // screen the current pane refs belong to
    Element   mElements[kMaxElements] = {};
    J2DPane*  mPrimaryPane[kMaxElements] = {};   // resolved per screen build
    J2DPane*  mFollowerPane[kMaxElements] = {};
    int       mElementCount = 0;
    f32       mAnchorX = 107.0f;
    f32       mAnchorY = -36.0f;
    f32       mSpacing = 122.0f;
};

}  // namespace cl
