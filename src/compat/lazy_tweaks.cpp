#include "lazy_tweaks.hpp"

#include "../util.hpp"

#include "SSystem/SComponent/c_counter.h"
#include "d/actor/d_a_alink.h"
#include "d/d_kantera_icon_meter.h"
#include "d/d_meter2_draw.h"
#include "d/d_pane_class.h"
#include "f_pc/f_pc_profile_lst.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRExpHeap.h"

#include <cstdint>
#include <cstring>

namespace {

struct LazyTweaksMeterTail {
    J2DPicture* itemNumTex[3][3];
    ResTIMG* itemXYTex[3][2][3];
    f32 buttonZItemBaseAlpha[2];
    CPaneMgr* itemXYZ;
};

int s_build = -1;

bool detect_build() {
    if (host_config_var_exists("game.zItemScale")) {
        return true;
    }
    const size_t legacySize = sizeof(daAlink_c) + sizeof(daPy_anmHeap_c);
    return static_cast<size_t>(g_profile_ALINK.base.base.process_size) == legacySize;
}

bool in_heap(JKRHeap* heap, const void* ptr, size_t size) {
    if (heap == nullptr || ptr == nullptr) {
        return false;
    }
    const uintptr_t start = reinterpret_cast<uintptr_t>(heap->getStartAddr());
    const uintptr_t end = reinterpret_cast<uintptr_t>(heap->getEndAddr());
    const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    return p >= start && p <= end && size <= end - p;
}

const LazyTweaksMeterTail* meter_tail(dMeter2Draw_c* draw) {
    if (draw == nullptr || !lazy_tweaks_build()) {
        return nullptr;
    }
    JKRHeap* heap = draw->heap;
    const auto* tail = reinterpret_cast<const LazyTweaksMeterTail*>(
        reinterpret_cast<const u8*>(draw) + sizeof(dMeter2Draw_c));
    if (!in_heap(heap, tail, sizeof(LazyTweaksMeterTail))) {
        return nullptr;
    }
    CPaneMgr* itemR = draw->mpItemR;
    CPaneMgr* itemZ = tail->itemXYZ;
    if (itemZ == itemR || !in_heap(heap, itemR, sizeof(CPaneMgr)) ||
        !in_heap(heap, itemZ, sizeof(CPaneMgr)))
    {
        return nullptr;
    }
    if (itemZ->getPanePtr() == nullptr || itemZ->getPanePtr() != itemR->getPanePtr()) {
        return nullptr;
    }
    return tail;
}

}

bool lazy_tweaks_build() {
    if (s_build < 0) {
        s_build = detect_build() ? 1 : 0;
    }
    return s_build == 1;
}

bool lazy_tweaks_hide_midna_icon() {
    if (!lazy_tweaks_build()) {
        return false;
    }
    static u32 s_frame = 0xFFFFFFFFu;
    static bool s_hide = false;
    const u32 frame = g_Counter.mCounter0;
    if (s_frame != frame) {
        s_frame = frame;
        s_hide = host_config_bool("game.hideMidnaIcon", false);
    }
    return s_hide;
}

J2DPicture* lazy_tweaks_item_num_tex(dMeter2Draw_c* draw, int button, int digit) {
    if (button < 0 || button > 2 || digit < 0 || digit > 2) {
        return nullptr;
    }
    const LazyTweaksMeterTail* tail = meter_tail(draw);
    if (tail == nullptr) {
        return nullptr;
    }
    J2DPicture* pic = tail->itemNumTex[button][digit];
    return in_heap(draw->heap, pic, sizeof(J2DPicture)) ? pic : nullptr;
}

dKantera_icon_c* lazy_tweaks_z_kantera_meter(dMeter2Draw_c* draw) {
    if (meter_tail(draw) == nullptr) {
        return nullptr;
    }
    dKantera_icon_c* meter = nullptr;
    std::memcpy(&meter, draw->field_0xb4, sizeof(meter));
    return in_heap(draw->heap, meter, sizeof(dKantera_icon_c)) ? meter : nullptr;
}
