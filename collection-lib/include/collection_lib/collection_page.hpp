#pragma once

// A Page is a full-screen layer next to the main item grid:
//   cl::Page* p2 = new cl::Page();
//   p2->add(cl::heart());
//   p2->add(cl::fused_shadow());
// Create pages before collectionlib_init()

#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DPane.h"

#include "dolphin/types.h"
#include <cstddef>

namespace cl {

struct Element {
    u64 paneTag = 0;
    u64 followerTag = 0;
    f32 followerDx = 0.0f;
    f32 followerDy = 0.0f;

    bool hideOnMain = false;
    bool selectable = true;

    bool claimsCell = false;
    u8   cellX = 0, cellY = 0;

    bool hasPos = false;
    f32  posX = 0.0f, posY = 0.0f;
};

Element heart();
Element fused_shadow();
Element crystal();

struct Page {
public:
    Page();
    ~Page();
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    void* operator new(std::size_t size);
    void  operator delete(void* ptr) noexcept;

    Element* add(const Element& element);
    Element* add(u64 paneTag);

    int element_count() const { return mElementCount; }

    void set_anchor(f32 x, f32 y) { mAnchorX = x; mAnchorY = y; }
    void set_spacing(f32 spacing) { mSpacing = spacing; }

    static constexpr int kMaxPages = 4;
    static constexpr int kMaxElements = 8;

    u64       mRootTag = 0;
    J2DPane*  mRootPane = nullptr;
    J2DScreen* mScreen = nullptr;
    Element   mElements[kMaxElements] = {};
    J2DPane*  mPrimaryPane[kMaxElements] = {};
    J2DPane*  mFollowerPane[kMaxElements] = {};
    int       mElementCount = 0;
    f32       mAnchorX = 107.0f;
    f32       mAnchorY = -36.0f;
    f32       mSpacing = 122.0f;
};

}
