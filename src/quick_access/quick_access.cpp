#include "quick_access.hpp"
#include "quick_access_internal.hpp"
#include "quick_access_bottles.hpp"
#include "../z_button/z_button.hpp"
#include "../z_button/z_mobile.hpp"
#include "../controls/controls.hpp"

#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_meter2_info.h"
#include "d/d_kantera_icon_meter.h"
#include "d/d_msg_object.h"
#define private public
#define protected public
#include "d/d_meter2_draw.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#undef protected
#undef private
#include "d/d_pane_class.h"
#include "dusk/config_var.hpp"
#include "d/d_select_cursor.h"
#include "d/d_meter_HIO.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "Z2AudioLib/Z2LinkMgr.h"
#include "mods/svc/resource.h"
#include "mods/svc/save.h"
#include "mods/svc/hook.hpp"
#include "../boss_rush/boss_rush.hpp"

#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>

#include <cmath>
#include <cstdio>
#include <cstring>

bool g_configQuickAccessEnabled = false;
int g_configQuickAccessAppearance = QA_APPEARANCE_RADIAL;

extern const ResourceService* get_resource_service();

bool s_menuOpen = false;
bool s_editMode = false;
int s_selectedSlot = SLOT_NONE;
f32 s_menuAlpha = 0.0f;
f32 s_glowTimer = 0.0f;

static ModContext* s_modCtx = nullptr;
static const SaveService* s_saveSvc = nullptr;

static u8 s_customItems[QA_QUICK_SLOTS] = {
    QA_ITEM_NONE, QA_ITEM_NONE, QA_ITEM_NONE, QA_ITEM_NONE,
};
static u8 s_assignedItem = QA_ITEM_NONE;

static SaveObserverHandle s_saveObserver = 0;
static const char* kQuickAccessBlobName = "quickAccessItemsV1";

struct QuickAccessBlob {
    u8 items[QA_QUICK_SLOTS];
    u8 assignedItem;
    u8 pad[3];
};
static_assert(sizeof(QuickAccessBlob) == 8, "blob size");

static void reset_custom_items() {
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        s_customItems[i] = QA_ITEM_NONE;
    }
    s_assignedItem = QA_ITEM_NONE;
}

static void on_new_save_reset_items(ModContext*, uint32_t, void*) {
    reset_custom_items();
}

static void refresh_wheel_down_slot_cache();

static void on_save_loaded_restore_items(ModContext* ctx, uint32_t, void*) {
    reset_custom_items();
    if (s_saveSvc == nullptr) return;

    QuickAccessBlob blob{};
    size_t size = sizeof(blob);
    if (s_saveSvc->get_blob(ctx, kQuickAccessBlobName, &blob, &size) == MOD_OK &&
        size == sizeof(blob)) {
        for (int i = 0; i < QA_QUICK_SLOTS; i++) {
            s_customItems[i] = (blob.items[i] == 0) ? QA_ITEM_NONE : blob.items[i];
        }
        s_assignedItem = (blob.assignedItem == 0) ? QA_ITEM_NONE : blob.assignedItem;
    }

    refresh_wheel_down_slot_cache();
}

int qa_custom_count() {
    int n = 0;
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        if (s_customItems[i] != QA_ITEM_NONE) {
            n++;
        }
    }
    return n;
}

u8 qa_custom_item(int idx) {
    return (idx >= 0 && idx < QA_QUICK_SLOTS) ? s_customItems[idx] : QA_ITEM_NONE;
}

bool qa_custom_contains(u8 itemNo) {
    for (int i = 0; i < qa_custom_count(); i++) {
        if (s_customItems[i] == itemNo) {
            return true;
        }
    }
    return false;
}

void qa_custom_store() {
    if (s_saveSvc == nullptr || s_modCtx == nullptr) return;
    QuickAccessBlob blob{};
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        blob.items[i] = s_customItems[i];
    }
    blob.assignedItem = s_assignedItem;
    s_saveSvc->set_blob(s_modCtx, kQuickAccessBlobName, &blob, sizeof(blob));

    if (itemwheel_filter_active()) {
        quick_access_itemwheel_refresh();
    }
}

bool qa_custom_add_item(u8 itemNo) {
    if (itemNo == QA_ITEM_NONE || qa_custom_contains(itemNo)) {
        return false;
    }
    if (qa_custom_count() >= QA_QUICK_SLOTS) {
        return false;
    }
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        if (s_customItems[i] == QA_ITEM_NONE) {
            s_customItems[i] = itemNo;
            qa_custom_store();
            return true;
        }
    }
    return false;
}

bool qa_custom_remove_item(u8 itemNo) {
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        if (s_customItems[i] == itemNo) {
            s_customItems[i] = QA_ITEM_NONE;
            if (s_assignedItem == itemNo) {
                s_assignedItem = QA_ITEM_NONE;
            }
            qa_custom_store();
            return true;
        }
    }
    return false;
}

bool qa_custom_replace_at(int idx, u8 itemNo) {
    if (idx < 0 || idx >= QA_QUICK_SLOTS || itemNo == QA_ITEM_NONE || qa_custom_contains(itemNo)) {
        return false;
    }
    const u8 old = s_customItems[idx];
    s_customItems[idx] = itemNo;
    if (s_assignedItem == old) {
        s_assignedItem = itemNo;
    }
    qa_custom_store();
    return true;
}

u8 qa_strip_assigned_item() {
    return s_assignedItem;
}

static u8 s_lastWheelDownSlot = 0xFF;

static void refresh_wheel_down_slot_cache() {
    s_lastWheelDownSlot = dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN);
}

static void sync_wheel_down_assignment() {
    if (g_configCustomZButtonEnabled) {
        return;
    }
    const u8 slot = dComIfGs_getSelectItemIndex(SELECT_ITEM_DOWN);
    if (slot == s_lastWheelDownSlot) {
        return;
    }
    s_lastWheelDownSlot = slot;

    const u8 item = (slot < MAX_ITEM_SLOTS) ? dComIfGs_getItem(slot, false) : QA_ITEM_NONE;
    if (item != s_assignedItem) {
        s_assignedItem = item;
        qa_custom_store();
    }
}

void qa_draw_solid_ring(f32 cx, f32 cy, f32 innerR, f32 outerR, JUtility::TColor color, int segs) {
    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (segs + 1) * 2);
    for (int s = 0; s <= segs; s++) {
        f32 angle = static_cast<f32>(s) * (6.2831853f / static_cast<f32>(segs));
        f32 c = std::cos(angle);
        f32 s_sin = std::sin(angle);

        GXPosition3f32(cx + c * outerR, cy + s_sin * outerR, 0.0f);
        GXColor1u32(color);
        GXPosition3f32(cx + c * innerR, cy + s_sin * innerR, 0.0f);
        GXColor1u32(color);
    }
    GXEnd();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

void qa_draw_solid_disc(f32 cx, f32 cy, f32 radius, JUtility::TColor color, int segs) {
    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, segs + 2);
    GXPosition3f32(cx, cy, 0.0f);
    GXColor1u32(color);
    for (int s = 0; s <= segs; s++) {
        f32 angle = static_cast<f32>(s) * (6.2831853f / static_cast<f32>(segs));
        GXPosition3f32(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius, 0.0f);
        GXColor1u32(color);
    }
    GXEnd();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

void qa_draw_solid_rect(f32 x, f32 y, f32 w, f32 h, JUtility::TColor color) {
    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x, y, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(x + w, y, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(x + w, y + h, 0.0f);
    GXColor1u32(color);
    GXPosition3f32(x, y + h, 0.0f);
    GXColor1u32(color);
    GXEnd();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

f32 qa_draw_text(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                 JUtility::TColor top, JUtility::TColor bottom, u8 alpha) {
    JUTFont* font = mDoExt_getMesgFont();
    if (!font) font = mDoExt_getSubFont();
    if (!font) return x;

    font->setGX();

    const f32 c = 1.5f;
    const f32 d = 1.0f;
    const f32 kOff[8][2] = {
        { c, 0.0f}, {-c, 0.0f}, {0.0f,  c}, {0.0f, -c},
        { d, d}, {d, -d}, {-d, d}, {-d, -d},
    };
    font->setCharColor(JUtility::TColor(0, 0, 0, alpha));
    for (const auto& o : kOff) {
        font->drawString_scale(x + o[0], y + o[1], charW, charH, text, true);
    }

    top.a = alpha;
    bottom.a = alpha;
    font->setGradColor(top, bottom);
    const f32 endX = font->drawString_scale(x, y, charW, charH, text, true);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
    return endX;
}

f32 qa_get_text_width(const char* text, f32 charW) {
    JUTFont* font = mDoExt_getMesgFont();
    if (!font) font = mDoExt_getSubFont();
    if (font) {
        f32 totalWidth = 0.0f;
        f32 baseWidth = static_cast<f32>(font->getWidth());
        if (baseWidth <= 0.0f) baseWidth = 1.0f;
        for (size_t i = 0; text[i] != '\0'; i++) {
            f32 w = static_cast<f32>(font->getWidth(text[i]));
            if (w <= 0.0f) w = baseWidth;
            totalWidth += w * (charW / baseWidth);
        }
        return totalWidth;
    }
    return static_cast<f32>(std::strlen(text)) * charW;
}

static void draw_corner_fan(f32 cx, f32 cy, f32 r, f32 a0, f32 a1, JUtility::TColor c, int segs) {
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, segs + 2);
    GXPosition3f32(cx, cy, 0.0f);
    GXColor1u32(c);
    for (int s = 0; s <= segs; s++) {
        f32 a = a0 + (a1 - a0) * (static_cast<f32>(s) / static_cast<f32>(segs));
        GXPosition3f32(cx + std::cos(a) * r, cy + std::sin(a) * r, 0.0f);
        GXColor1u32(c);
    }
    GXEnd();
}

void qa_draw_rounded_rect(f32 x, f32 y, f32 w, f32 h, f32 r, JUtility::TColor c, int segs) {
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;

    qa_draw_solid_rect(x + r, y, w - r * 2.0f, h, c);
    qa_draw_solid_rect(x, y + r, w, h - r * 2.0f, c);

    const f32 kPi = 6.2831853f * 0.5f;
    draw_corner_fan(x + r, y + r, r, kPi * 1.0f, kPi * 1.5f, c, segs);
    draw_corner_fan(x + w - r, y + r, r, kPi * 1.5f, kPi * 2.0f, c, segs);
    draw_corner_fan(x + w - r, y + h - r, r, 0.0f, kPi * 0.5f, c, segs);
    draw_corner_fan(x + r, y + h - r, r, kPi * 0.5f, kPi * 1.0f, c, segs);
}

void qa_draw_framed_plate(f32 x, f32 y, f32 w, f32 h, f32 r,
                          JUtility::TColor frame, JUtility::TColor fill) {
    qa_draw_rounded_rect(x, y, w, h, r, frame);
    qa_draw_rounded_rect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f, r > 2.0f ? r - 2.0f : 1.0f, fill);
}

void qa_draw_top_sheen(f32 x, f32 y, f32 w, f32 h, JUtility::TColor top) {
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    JUtility::TColor bottom = top;
    bottom.a = 0;
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x, y, 0.0f);
    GXColor1u32(top);
    GXPosition3f32(x + w, y, 0.0f);
    GXColor1u32(top);
    GXPosition3f32(x + w, y + h, 0.0f);
    GXColor1u32(bottom);
    GXPosition3f32(x, y + h, 0.0f);
    GXColor1u32(bottom);
    GXEnd();
}

static bool isTitleOrMainMenu() {
    daPy_py_c* player = daPy_getLinkPlayerActorClass();
    if (player == nullptr) {
        return true;
    }
    const char* stageName = dComIfGp_getStartStageName();
    if (stageName != nullptr) {
        if (std::strcmp(stageName, "F_SP102") == 0 || std::strcmp(stageName, "title") == 0) {
            return true;
        }
    }
    return false;
}

static bool isWolfPlayer() {
    daAlink_c* player = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    return (player != nullptr && player->checkWolf());
}

bool qa_is_rod_item(u8 itemNo) {
    switch (itemNo) {
    case dItemNo_FISHING_ROD_1_e:
    case dItemNo_LURE_ROD_e:
    case dItemNo_BEE_ROD_e:
    case dItemNo_JEWEL_ROD_e:
    case dItemNo_WORM_ROD_e:
    case dItemNo_JEWEL_BEE_ROD_e:
    case dItemNo_JEWEL_WORM_ROD_e:
        return true;
    default:
        return false;
    }
}

static u8 get_fishing_rod_item() {
    for (u8 slot = 0; slot < 24; slot++) {
        u8 item = dComIfGs_getItem(slot, false);
        if (qa_is_rod_item(item)) {
            return item;
        }
    }
    return dItemNo_NONE_e;
}

static bool items_same_family(u8 a, u8 b) {
    if (a == b) return true;
    const bool aLantern = (a == dItemNo_KANTERA_e || a == dItemNo_KANTERA2_e);
    const bool bLantern = (b == dItemNo_KANTERA_e || b == dItemNo_KANTERA2_e);
    if (aLantern && bLantern) return true;
    if (qa_is_rod_item(a) && qa_is_rod_item(b)) return true;
    return false;
}

bool qa_custom_contains_family(u8 itemNo) {
    const int count = qa_custom_count();
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        if (s_customItems[i] != QA_ITEM_NONE && items_same_family(s_customItems[i], itemNo)) {
            return true;
        }
    }
    return false;
}

u8 qa_custom_find_family_item(u8 itemNo) {
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        if (s_customItems[i] != QA_ITEM_NONE && items_same_family(s_customItems[i], itemNo)) {
            return s_customItems[i];
        }
    }
    return QA_ITEM_NONE;
}

bool qa_is_item_available(u8 itemNo) {
    if (itemNo == QA_ITEM_NONE) return false;
    for (u8 slot = 0; slot < 24; slot++) {
        if (items_same_family(itemNo, dComIfGs_getItem(slot, false))) {
            return true;
        }
    }
    return false;
}

bool qa_is_lantern_active() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr) return false;
    return link->checkUseKandelaar(0) || link->mEquipItem == dItemNo_KANTERA_e;
}

bool qa_load_boots_worn() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    return (link != nullptr && link->checkEquipHeavyBoots() != 0);
}

int qa_get_active_items(u8 outItems[QA_QUICK_SLOTS]) {
    int count = 0;
    for (int i = 0; i < QA_QUICK_SLOTS; i++) {
        u8 itemNo = s_customItems[i];
        if (itemNo != QA_ITEM_NONE && qa_is_item_available(itemNo)) {
            outItems[count++] = itemNo;
        }
    }
    return count;
}

bool quick_access_is_active() {
    return g_configQuickAccessEnabled && s_menuOpen;
}

bool quick_access_keep_boots_equipped(daAlink_c* link) {
    if (!g_configQuickAccessEnabled || link == nullptr) {
        return false;
    }
    return link->checkEquipHeavyBoots() != 0;
}

bool quick_access_boots_on_quick_access() {
    return g_configQuickAccessEnabled && s_assignedItem == dItemNo_HVY_BOOTS_e;
}

static u32 qa_bti_image_size(const ResTIMG* t) {
    const u32 w = t->width;
    const u32 h = t->height;
    switch (t->format) {
    case 0x00:
    case 0x08:
        return ((w + 7) / 8) * ((h + 7) / 8) * 32;
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x0A:
        return ((w + 3) / 4) * ((h + 3) / 4) * 32;
    case 0x06:
        return ((w + 3) / 4) * ((h + 3) / 4) * 64;
    case 0x01:
    case 0x02:
    case 0x09:
    case 0x0E:
    default:
        return ((w + 7) / 8) * ((h + 3) / 4) * 32;
    }
}

ResTIMG* qa_copy_texture(const ResTIMG* src) {
    if (src == nullptr || src->width == 0 || src->height == 0) {
        return nullptr;
    }
    const u32 imgSize = qa_bti_image_size(src);
    const u32 pltSize = (src->paletteOffset != 0) ? (src->numColors * 2) : 0;
    const u32 total = sizeof(ResTIMG) + imgSize + pltSize;

    JKRHeap* heap = mDoExt_getGameHeap();
    if (heap == nullptr) {
        return nullptr;
    }
    u8* buf = static_cast<u8*>(heap->alloc(total, 32));
    if (buf == nullptr) {
        return nullptr;
    }
    memcpy(buf, src, sizeof(ResTIMG));
    memcpy(buf + sizeof(ResTIMG), reinterpret_cast<const u8*>(src) + src->imageOffset, imgSize);
    ResTIMG* out = reinterpret_cast<ResTIMG*>(buf);
    out->imageOffset = sizeof(ResTIMG);
    if (pltSize > 0) {
        memcpy(buf + sizeof(ResTIMG) + imgSize,
               reinterpret_cast<const u8*>(src) + src->paletteOffset, pltSize);
        out->paletteOffset = sizeof(ResTIMG) + imgSize;
    }
    out->alphaEnabled = 1;
    return out;
}

ResTIMG* qa_extract_pane_texture(J2DPane* pane) {
    J2DPicture* pic = static_cast<J2DPicture*>(pane);
    if (pane == nullptr || pic->getTexture(0) == nullptr) {
        return nullptr;
    }
    return qa_copy_texture(const_cast<const ResTIMG*>(pic->getTexture(0)->getTexInfo()));
}

struct QaIconEntry {
    u8 itemNo;
    bool loaded;
    bool hasSecond;
    J2DPicture* pic;
    J2DPicture* pic2;
    alignas(32) u8 mainBuf[0x1000];
    alignas(32) u8 shineBuf[0x1000];
};

static const int QA_ICON_CACHE_SIZE = 32;
static QaIconEntry s_iconCache[QA_ICON_CACHE_SIZE];

void qa_reset_icon_caches() {
    for (int i = 0; i < QA_ICON_CACHE_SIZE; i++) {
        JKR_DELETE(s_iconCache[i].pic);
        JKR_DELETE(s_iconCache[i].pic2);
        s_iconCache[i].pic = nullptr;
        s_iconCache[i].pic2 = nullptr;
        s_iconCache[i].loaded = false;
        s_iconCache[i].hasSecond = false;
        s_iconCache[i].itemNo = QA_ITEM_NONE;
    }
}

static bool load_icon_entry(QaIconEntry& entry) {
    if (entry.loaded && entry.pic != nullptr) {
        return true;
    }
    int res = dMeter2Info_readItemTexture(
        entry.itemNo,
        reinterpret_cast<ResTIMG*>(entry.mainBuf),
        nullptr,
        reinterpret_cast<ResTIMG*>(entry.shineBuf),
        nullptr,
        nullptr, nullptr, nullptr, nullptr, -1
    );
    if (res <= 0) {
        return false;
    }
    reinterpret_cast<ResTIMG*>(entry.mainBuf)->alphaEnabled = 1;
    reinterpret_cast<ResTIMG*>(entry.shineBuf)->alphaEnabled = 1;

    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
    if (entry.pic == nullptr) {
        entry.pic = JKR_NEW J2DPicture(reinterpret_cast<ResTIMG*>(entry.mainBuf));
    } else {
        entry.pic->changeTexture(reinterpret_cast<ResTIMG*>(entry.mainBuf), 0);
    }
    entry.hasSecond = (res > 1);
    if (entry.hasSecond) {
        if (entry.pic2 == nullptr) {
            entry.pic2 = JKR_NEW J2DPicture(reinterpret_cast<ResTIMG*>(entry.shineBuf));
        } else {
            entry.pic2->changeTexture(reinterpret_cast<ResTIMG*>(entry.shineBuf), 0);
        }
    } else {
        JKR_DELETE(entry.pic2);
        entry.pic2 = nullptr;
    }
    if (oldHeap != nullptr) {
        mDoExt_setCurrentHeap(oldHeap);
    }

    if (entry.pic == nullptr) {
        entry.loaded = false;
        return false;
    }
    if (entry.hasSecond && entry.pic2 != nullptr) {
        g_meter2_info.setItemColor(entry.itemNo, entry.pic, entry.pic2, nullptr, nullptr);
    }
    entry.loaded = true;
    return true;
}

bool qa_get_item_icon(u8 itemNo, J2DPicture** outPic, ResTIMG** outImg, J2DPicture** outPic2) {
    if (outPic2 != nullptr) {
        *outPic2 = nullptr;
    }
    if (itemNo == QA_ITEM_NONE) {
        return false;
    }
    for (int i = 0; i < QA_ICON_CACHE_SIZE; i++) {
        QaIconEntry& entry = s_iconCache[i];
        if (entry.loaded && entry.itemNo == itemNo) {
            *outPic = entry.pic;
            *outImg = reinterpret_cast<ResTIMG*>(entry.mainBuf);
            if (outPic2 != nullptr) {
                *outPic2 = entry.hasSecond ? entry.pic2 : nullptr;
            }
            return true;
        }
    }
    for (int i = 0; i < QA_ICON_CACHE_SIZE; i++) {
        QaIconEntry& entry = s_iconCache[i];
        if (!entry.loaded && entry.pic == nullptr) {
            entry.itemNo = itemNo;
            if (load_icon_entry(entry)) {
                *outPic = entry.pic;
                *outImg = reinterpret_cast<ResTIMG*>(entry.mainBuf);
                if (outPic2 != nullptr) {
                    *outPic2 = entry.hasSecond ? entry.pic2 : nullptr;
                }
                return true;
            }
            entry.itemNo = QA_ITEM_NONE;
            return false;
        }
    }
    return false;
}

static J2DPicture* s_qaDigitPic[3] = { nullptr, nullptr, nullptr };
static dKantera_icon_c* s_qaKanteraIcon = nullptr;

static bool qa_item_ammo_count(u8 itemNo, int& count, int& maxCount) {
    count = -1;
    maxCount = -1;

    if (itemNo == dItemNo_NORMAL_BOMB_e || itemNo == dItemNo_WATER_BOMB_e ||
        itemNo == dItemNo_POKE_BOMB_e || itemNo == dItemNo_BOMB_ARROW_e) {
        const u8 wanted = (itemNo == dItemNo_BOMB_ARROW_e) ? dItemNo_NORMAL_BOMB_e : itemNo;
        int bagIdx = -1;
        for (int b = 0; b < 3 && bagIdx < 0; b++) {
            if (dComIfGs_getItem((u8)(b + 15), false) == wanted) {
                bagIdx = b;
            }
        }
        if (bagIdx < 0) {
            for (int b = 0; b < 3 && bagIdx < 0; b++) {
                if (dComIfGs_getItem((u8)(b + 15), false) != dItemNo_NONE_e &&
                    dComIfGs_getBombNum((u8)b) > 0) {
                    bagIdx = b;
                }
            }
        }
        if (bagIdx < 0) {
            bagIdx = 0;
        }
        count = dComIfGs_getBombNum((u8)bagIdx);
        maxCount = dComIfGs_getBombMax(dComIfGs_getItem((u8)(bagIdx + 15), false));
    } else if (itemNo == dItemNo_BOW_e || itemNo == dItemNo_HAWK_ARROW_e ||
               itemNo == dItemNo_LIGHT_ARROW_e || itemNo == dItemNo_ARROW_LV1_e ||
               itemNo == dItemNo_ARROW_LV2_e || itemNo == dItemNo_ARROW_LV3_e) {
        count = dComIfGs_getArrowNum();
        maxCount = dComIfGs_getArrowMax();
    } else if (itemNo == dItemNo_PACHINKO_e) {
        count = dComIfGs_getPachinkoNum();
        maxCount = dComIfGs_getPachinkoMax();
    } else if (itemNo == dItemNo_BEE_CHILD_e) {
        for (u8 b = 0; b < 4 && count < 0; b++) {
            if (dComIfGs_getItem((u8)(b + 11), false) == dItemNo_BEE_CHILD_e) {
                count = dComIfGs_getBottleNum(b);
                maxCount = 10;
            }
        }
        if (count < 0) {
            count = 0;
            maxCount = 10;
        }
    }

    return count >= 0;
}

static void qa_draw_ammo_digits(int num, int maxNum, f32 baseX, f32 baseY, f32 iconW, f32 iconH,
                                f32 alphaRate) {
    if (num < 0 || alphaRate <= 0.0f) return;
    if (num > 999) num = 999;
    if (maxNum > 0 && num > maxNum) num = maxNum;

    JKRArchive* arc = dComIfGp_getMain2DArchive();
    if (arc == nullptr) return;

    auto get_timg = [arc](int d) -> ResTIMG* {
        if (d < 0 || d > 9) d = 0;
        return (ResTIMG*)arc->getResource('TIMG', dMeter2Info_getNumberTextureName(d));
    };

    ResTIMG* defaultDigitTex = get_timg(0);
    if (defaultDigitTex == nullptr) return;

    if (s_qaDigitPic[0] == nullptr) {
        JKRHeap* rootHeap = JKRHeap::getRootHeap();
        JKRHeap* oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
        for (int i = 0; i < 3; i++) {
            s_qaDigitPic[i] = JKR_NEW J2DPicture(defaultDigitTex);
        }
        if (oldHeap != nullptr) {
            mDoExt_setCurrentHeap(oldHeap);
        }
    }
    if (s_qaDigitPic[0] == nullptr || s_qaDigitPic[1] == nullptr || s_qaDigitPic[2] == nullptr) {
        return;
    }

    JUtility::TColor black(0, 0, 0, 0);
    JUtility::TColor white(255, 255, 255, 255);
    if (maxNum > 0 && num == maxNum) {
        black.set(30, 30, 30, 0);
        white.set(255, 200, 50, 255);
    } else if (num == 0) {
        black.set(30, 30, 30, 0);
        white.set(180, 180, 180, 255);
    }

    const f32 digitW = 12.0f;
    const f32 digitH = 12.0f;
    const f32 startX = baseX + (iconW * 0.25f);
    const f32 startY = baseY + (iconH * 0.60f);

    const u8 a = (u8)(alphaRate * 255.0f);
    for (int i = 0; i < 3; i++) {
        s_qaDigitPic[i]->setBlackWhite(black, white);
        s_qaDigitPic[i]->setAlpha(a);
    }

    if (num < 100) {
        ResTIMG* t1 = get_timg(num / 10);
        ResTIMG* t2 = get_timg(num % 10);
        if (t1 != nullptr && t2 != nullptr) {
            s_qaDigitPic[0]->changeTexture(t1, 0);
            s_qaDigitPic[0]->draw(startX + iconW - (digitW * 1.8f), startY, digitW, digitH, false,
                                  false, false);
            s_qaDigitPic[1]->changeTexture(t2, 0);
            s_qaDigitPic[1]->draw(startX + iconW - (digitW * 0.9f), startY, digitW, digitH, false,
                                  false, false);
        }
    } else {
        ResTIMG* t1 = get_timg(num / 100);
        ResTIMG* t2 = get_timg((num / 10) % 10);
        ResTIMG* t3 = get_timg(num % 10);
        if (t1 != nullptr && t2 != nullptr && t3 != nullptr) {
            s_qaDigitPic[0]->changeTexture(t1, 0);
            s_qaDigitPic[0]->draw(startX + iconW - (digitW * 2.7f), startY, digitW, digitH, false,
                                  false, false);
            s_qaDigitPic[1]->changeTexture(t2, 0);
            s_qaDigitPic[1]->draw(startX + iconW - (digitW * 1.8f), startY, digitW, digitH, false,
                                  false, false);
            s_qaDigitPic[2]->changeTexture(t3, 0);
            s_qaDigitPic[2]->draw(startX + iconW - (digitW * 0.9f), startY, digitW, digitH, false,
                                  false, false);
        }
    }
}

static void qa_draw_lantern_oil_gauge(f32 baseX, f32 baseY, f32 iconW, f32 iconH, f32 alphaRate) {
    if (s_qaKanteraIcon == nullptr) {
        JKRArchive* arc2D = dComIfGp_getMain2DArchive();
        JKRHeap* rootHeap = JKRHeap::getRootHeap();
        if (arc2D != nullptr && rootHeap != nullptr) {
            JKRHeap* oldHeap = mDoExt_setCurrentHeap(rootHeap);
            s_qaKanteraIcon = JKR_NEW dKantera_icon_c();
            mDoExt_setCurrentHeap(oldHeap);

            if (s_qaKanteraIcon != nullptr &&
                (s_qaKanteraIcon->mpParent == nullptr ||
                 s_qaKanteraIcon->mpParent->getPanePtr() == nullptr ||
                 s_qaKanteraIcon->mpGauge == nullptr ||
                 s_qaKanteraIcon->mpGauge->getPanePtr() == nullptr)) {
                JKR_DELETE(s_qaKanteraIcon);
            }
        }
    }
    if (s_qaKanteraIcon == nullptr || s_qaKanteraIcon->mpParent == nullptr ||
        s_qaKanteraIcon->mpGauge == nullptr) {
        return;
    }

    const f32 gaugeX = baseX + iconW * 0.5f + 7.0f;
    const f32 gaugeY = baseY + iconH;
    s_qaKanteraIcon->setPos(gaugeX, gaugeY);
    s_qaKanteraIcon->setScale(0.4f, 0.6f);
    s_qaKanteraIcon->setNowGauge(dComIfGs_getMaxOil(), dComIfGs_getOil());
    s_qaKanteraIcon->setAlphaRate(alphaRate);
    s_qaKanteraIcon->drawSelf();
}

void qa_draw_item_ammo(u8 itemNo, f32 baseX, f32 baseY, f32 iconW, f32 iconH, u8 alpha) {
    if (itemNo == QA_ITEM_NONE || alpha == 0) {
        return;
    }
    const f32 alphaRate = static_cast<f32>(alpha) / 255.0f;
    if (itemNo == dItemNo_KANTERA_e || itemNo == dItemNo_KANTERA2_e) {
        qa_draw_lantern_oil_gauge(baseX, baseY, iconW, iconH, alphaRate);
        return;
    }
    int count = -1;
    int maxCount = -1;
    if (qa_item_ammo_count(itemNo, count, maxCount)) {
        qa_draw_ammo_digits(count, maxCount, baseX, baseY, iconW, iconH, alphaRate);
    }
}

void qa_shutdown_item_ammo() {
    for (int i = 0; i < 3; i++) {
        JKR_DELETE(s_qaDigitPic[i]);
        s_qaDigitPic[i] = nullptr;
    }
    JKR_DELETE(s_qaKanteraIcon);
    s_qaKanteraIcon = nullptr;
}

static dSelect_cursor_c* s_selCursors[2] = { nullptr, nullptr };

dSelect_cursor_c* qa_sel_cursor(int idx) {
    if (idx < 0 || idx > 1) {
        idx = 0;
    }
    dSelect_cursor_c*& cursor = s_selCursors[idx];
    if (cursor == nullptr) {
        cursor = new dSelect_cursor_c(2, 0.85f, nullptr);
        if (cursor != nullptr) {
            cursor->mpSelectIcon = nullptr;
            cursor->setAlphaRate(1.0f);

            const f32 base = 26.0f;
            for (int k = 0; k < 4; k++) {
                cursor->field_0x94[k] = (k < 2) ? -base : base;
                cursor->field_0xa4[k] = (k % 2 == 0) ? -base : base;
            }

            if (cursor->mpPaneMgr != nullptr) {
                J2DPane* root = cursor->mpPaneMgr->getPanePtr();
                if (root != nullptr) {
                    J2DPane* keep[8];
                    int keepCount = 0;
                    for (int k = 0; k < 4 && keepCount < 8; k++) {
                        J2DPane* p = cursor->field_0x1C[k] != nullptr
                                         ? cursor->field_0x1C[k]->getPanePtr()
                                         : nullptr;
                        for (; p != nullptr && p != root && keepCount < 8;
                             p = p->getParentPane()) {
                            keep[keepCount++] = p;
                        }
                    }
                    for (J2DPane* child = root->getFirstChildPane(); child != nullptr;
                         child = child->getNextChildPane()) {
                        bool isKeep = false;
                        for (int k = 0; k < keepCount; k++) {
                            if (child == keep[k]) {
                                isKeep = true;
                                break;
                            }
                        }
                        if (!isKeep) {
                            child->hide();
                        }
                    }
                }
            }
        }
    }
    return cursor;
}

void qa_sel_cursor_destroy() {
    for (int i = 0; i < 2; i++) {
        if (s_selCursors[i] != nullptr) {
            delete s_selCursors[i];
            s_selCursors[i] = nullptr;
        }
    }
}

static void play_error_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void play_ok_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void play_cursor_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static u8 s_deferredUseItem = QA_ITEM_NONE;

// Using a quick access item other than the lantern snuffs a burning lantern for
// real (clear FLG2_UNK_1), so it does not come back lit when switching back to it.
static void qa_extinguish_lantern_for_item() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr || link->checkWolf()) {
        return;
    }
    if (link->mEquipItem == dItemNo_KANTERA_e ||
        !link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1)) {
        return;
    }
    link->offNoResetFlg2(daPy_py_c::FLG2_UNK_1);
    link->mZ2Link.setKanteraState(0);
}

static bool s_qaBootsDesired = false;
static int s_qaBootsGraceFrames = 0;
static int s_qaBootsCooldown = 0;
static bool s_qaBootsEquipAllowed = false;

DEFINE_HOOK(&daAlink_c::procBootsEquipInit, QaBootsEquipInitHook);

HookAction on_qa_boots_equip_init_pre(ModContext*, void* args, void* retval, void*) {
    daAlink_c* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr) {
        return HOOK_CONTINUE;
    }

    if (g_configQuickAccessEnabled && s_assignedItem == dItemNo_HVY_BOOTS_e) {
        if (s_qaBootsEquipAllowed) {
            s_qaBootsEquipAllowed = false;
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

static bool qa_boots_in_water(daAlink_c* link) {
    if (link->checkModeFlg(daAlink_c::MODE_SWIMMING) ||
        link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE))
    {
        return true;
    }
    switch (link->mProcID) {
    case daAlink_c::PROC_SWIM_UP:
    case daAlink_c::PROC_SWIM_WAIT:
    case daAlink_c::PROC_SWIM_MOVE:
    case daAlink_c::PROC_SWIM_DIVE:
        return true;
    default:
        return false;
    }
}

static void qa_toggle_boots_underwater(daAlink_c* link, bool equipped) {
    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_HVY_BOOTS_e);
    const int procType = link->checkNewItemChange(2);
    s_qaBootsEquipAllowed = true;
    if (procType != 0) {
        z_mobile_hb_lock(link, true);
        link->changeItemTriggerKeepProc(2, procType);
    } else {
        link->setHeavyBoots(equipped ? 0 : 1);
    }
    s_qaBootsEquipAllowed = false;
    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
    s_qaBootsDesired = !equipped;
    s_qaBootsGraceFrames = 45;
    play_ok_se();
}

static void execute_iron_boots() {
    if (s_qaBootsCooldown > 0) {
        return;
    }
    s_qaBootsCooldown = 25;

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr) return;

    if (!qa_is_item_available(dItemNo_HVY_BOOTS_e)) {
        play_error_se();
        return;
    }

    if (link->checkNotHeavyBootsStage() || link->checkReinRide() || link->checkCanoeRide()) {
        play_error_se();
        return;
    }

    const bool equipped = link->checkEquipHeavyBoots() != 0;
    if (qa_boots_in_water(link)) {
        qa_toggle_boots_underwater(link, equipped);
        return;
    }

    if (link->checkEquipHeavyBoots()) {
        s_qaBootsDesired = false;
        s_qaBootsGraceFrames = 45;
        s_qaBootsEquipAllowed = true;
        link->procBootsEquipInit();
        play_ok_se();
        return;
    }

    s_qaBootsDesired = true;
    s_qaBootsGraceFrames = 45;
    s_qaBootsEquipAllowed = true;
    link->procBootsEquipInit();
    play_ok_se();
}

static void execute_horse_call() {
    daAlink_c* alink = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (alink == nullptr) return;

    if (!qa_is_item_available(dItemNo_HORSE_FLUTE_e)) {
        play_error_se();
        return;
    }

    if (boss_rush_is_fighting_here()) {
        play_error_se();
        return;
    }

    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_HORSE_FLUTE_e);

    int proc_type = alink->checkNewItemChange(2);
    if (proc_type != 0) {
        alink->changeItemTriggerKeepProc(2, proc_type);
        play_ok_se();
    } else {
        play_error_se();
    }

    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
}

static bool s_qaLanternLit = false;

static void execute_lantern() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr) return;

    if (!qa_is_item_available(dItemNo_KANTERA_e) && !qa_is_item_available(dItemNo_KANTERA2_e)) {
        play_error_se();
        return;
    }

    if (link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE) || link->checkModeFlg(0x40000) || link->checkReinRide() || link->checkCanoeRide()) {
        play_error_se();
        return;
    }

    if (link->mEquipItem == dItemNo_KANTERA_e) {
        s_deferredUseItem = dItemNo_KANTERA_e;
        return;
    }

    if (qa_is_lantern_active()) {
        s_qaLanternLit = false;
    }

    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, dItemNo_KANTERA_e);

    int proc_type = link->checkNewItemChange(2);
    if (proc_type != 0) {
        link->changeItemTriggerKeepProc(2, proc_type);
        play_ok_se();
    } else {
        play_error_se();
    }

    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
}

static void execute_fishing_rod() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr) return;

    u8 rodItem = get_fishing_rod_item();
    if (rodItem == dItemNo_NONE_e) {
        play_error_se();
        return;
    }

    if (link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE) || link->checkModeFlg(0x40000) || link->checkReinRide() || link->checkCanoeRide()) {
        play_error_se();
        return;
    }

    if (qa_is_rod_item(link->mEquipItem)) {
        s_deferredUseItem = link->mEquipItem;
        return;
    }

    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, rodItem);

    int proc_type = link->checkNewItemChange(2);
    if (proc_type != 0) {
        link->changeItemTriggerKeepProc(2, proc_type);
        play_ok_se();
    } else {
        play_error_se();
    }

    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
}

static u8 s_qaBombItem = QA_ITEM_NONE;
static u8 s_qaBombBag = 0xFF;
static bool s_qaBombGrabbed = false;
static int s_qaBombFrames = 0;
static bool s_qaBombSlotHeld = false;
static u8 s_qaBombPrevIdx = 0xFF;
static u8 s_qaBombPrevMix = 0xFF;
static u8 s_qaBombPrevPlay = 0xFF;

static void qa_restore_down_slot() {
    if (!s_qaBombSlotHeld) {
        return;
    }
    dComIfGs_setMixItemIndex(2, s_qaBombPrevMix);
    dComIfGs_setSelectItemIndex(2, s_qaBombPrevIdx);
    g_dComIfG_gameInfo.play.setSelectItem(2, s_qaBombPrevPlay);
    s_qaBombSlotHeld = false;
}

static void execute_bomb_item(daAlink_c* link, u8 itemNo) {
    if (s_qaBombItem != QA_ITEM_NONE) {
        return;
    }

    u8 bagSlot = 0xFF;
    for (u8 s = SLOT_15; s <= SLOT_17; s++) {
        if (dComIfGs_getItem(s, false) == itemNo) {
            bagSlot = s;
            break;
        }
    }
    if (bagSlot == 0xFF) {
        play_error_se();
        return;
    }

    s_qaBombPrevMix = dComIfGs_getMixItemIndex(2);
    s_qaBombPrevIdx = dComIfGs_getSelectItemIndex(2);
    s_qaBombPrevPlay = dComIfGp_getSelectItem(2);

    dComIfGs_setMixItemIndex(2, 0xFF);
    dComIfGs_setSelectItemIndex(2, bagSlot);
    g_dComIfG_gameInfo.play.setSelectItem(2, itemNo);
    s_qaBombSlotHeld = true;

    int proc_type = link->checkNewItemChange(2);
    if (proc_type == 0) {
        play_error_se();
        qa_restore_down_slot();
        return;
    }

    link->changeItemTriggerKeepProc(2, proc_type);
    play_ok_se();

    s_qaBombItem = itemNo;
    s_qaBombBag = bagSlot - SLOT_15;
    s_qaBombGrabbed = false;
    s_qaBombFrames = 0;
}

DEFINE_HOOK(&daAlink_c::checkReadyItem, QaCheckReadyItemHook);
static void on_check_ready_item_qa_post(ModContext*, void*, void* ret, void*) {
    if (s_qaBombItem == QA_ITEM_NONE) {
        return;
    }
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr || !quick_access_keep_bomb_equipped(link)) {
        return;
    }
    *static_cast<bool*>(ret) = true;
}

static void qa_tick_bomb_tracking() {
    if (s_qaBombItem == QA_ITEM_NONE) {
        return;
    }
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr) {
        return;
    }
    s_qaBombFrames++;

    const bool grabbed = link->mGrabItemAcKeep.getActor() != nullptr;
    if (grabbed && !s_qaBombGrabbed) {
        if (dComIfGp_getSelectItem(2) != s_qaBombItem && s_qaBombBag <= 2 &&
            dComIfGs_getBombNum(s_qaBombBag) > 0) {
            dComIfGs_setBombNum(s_qaBombBag, dComIfGs_getBombNum(s_qaBombBag) - 1);
        }
        qa_restore_down_slot();
    }
    s_qaBombGrabbed = grabbed;

    if (s_qaBombSlotHeld && s_qaBombFrames >= 30) {
        qa_restore_down_slot();
    }

    if (!quick_access_keep_bomb_equipped(link) && !s_qaBombSlotHeld) {
        s_qaBombItem = QA_ITEM_NONE;
        s_qaBombBag = 0xFF;
        s_qaBombGrabbed = false;
    }
}

bool quick_access_keep_bomb_equipped(daAlink_c* link) {
    if (!g_configQuickAccessEnabled || link == nullptr || s_qaBombItem == QA_ITEM_NONE) {
        return false;
    }
    return (link->mEquipItem == s_qaBombItem) ||
           (link->mGrabItemAcKeep.getActor() != nullptr);
}

bool quick_access_keep_lantern_equipped(daAlink_c* link) {
    if (!g_configQuickAccessEnabled || link == nullptr) {
        return false;
    }
    if (s_assignedItem != dItemNo_KANTERA_e && s_assignedItem != dItemNo_KANTERA2_e) {
        return false;
    }
    return link->mEquipItem == dItemNo_KANTERA_e ||
           link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1);
}

DEFINE_HOOK(&daAlink_c::execute, QaAlinkExecuteHook);
DEFINE_HOOK(&daAlink_c::setHeavyBoots, QaSetHeavyBootsHook);

HookAction on_qa_set_heavy_boots_pre(ModContext*, void* args, void* retval, void*) {
    if (!g_configQuickAccessEnabled) {
        return HOOK_CONTINUE;
    }

    const int enable = mods::arg<int>(args, 1);
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || enable != 0 || s_assignedItem != dItemNo_HVY_BOOTS_e) {
        return HOOK_CONTINUE;
    }

    const bool forced =
        link->checkWolf() || link->checkEventRun() || link->checkDeadHP() ||
        link->checkNotHeavyBootsStage() || link->checkCanoeRide() ||
        link->checkHorseRide() || link->checkBoardRide() || link->checkSpinnerRide() ||
        link->checkModeFlg(0x40000) ||
        link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE) ||
        link->mProcID == daAlink_c::PROC_DIVE_JUMP ||
        link->mProcID == daAlink_c::PROC_SMALL_JUMP;
    if (forced) {
        return HOOK_CONTINUE;
    }

    if (retval != nullptr) {
        *static_cast<int*>(retval) = 0;
    }
    return HOOK_SKIP_ORIGINAL;
}

static u8 s_qaPreEquipItem = 0xFF;

HookAction on_qa_alink_execute_pre(ModContext*, void*, void*, void*) {
    if (!g_configQuickAccessEnabled || isTitleOrMainMenu()) {
        return HOOK_CONTINUE;
    }
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link != nullptr) {
        s_qaPreEquipItem = link->mEquipItem;
    }
    return HOOK_CONTINUE;
}

static void on_qa_alink_execute_post(ModContext*, void*, void*, void*) {
    if (s_qaBootsCooldown > 0) {
        s_qaBootsCooldown--;
    }
    if (!g_configQuickAccessEnabled || isTitleOrMainMenu()) {
        s_qaLanternLit = false;
        return;
    }

    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr || link->checkWolf()) {
        s_qaLanternLit = false;
        s_qaBootsDesired = false;
        return;
    }

    if (g_configQuickAccessEnabled && s_assignedItem == dItemNo_HVY_BOOTS_e) {
        if (s_qaBootsGraceFrames > 0) {
            s_qaBootsGraceFrames--;
        }

        const bool actualWorn = link->checkEquipHeavyBoots() != 0;
        if (s_qaBootsDesired && !actualWorn && s_qaBootsGraceFrames == 0 &&
            link->mProcID != daAlink_c::PROC_BOOTS_EQUIP) {
            const bool forced =
                link->checkWolf() || link->checkEventRun() || link->checkDeadHP() ||
                link->checkNotHeavyBootsStage() || link->checkCanoeRide() ||
                link->checkHorseRide() || link->checkBoardRide() || link->checkSpinnerRide() ||
                link->checkModeFlg(0x40000) || link->checkMagneBootsOn() ||
                link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE) ||
                link->mProcID == daAlink_c::PROC_DIVE_JUMP ||
                link->mProcID == daAlink_c::PROC_SMALL_JUMP;
            if (forced) {
                s_qaBootsDesired = false;
            } else {
                link->setHeavyBoots(1);
            }
        }
    } else {
        s_qaBootsDesired = false;
        s_qaBootsGraceFrames = 0;
    }

    const bool lanternAssigned =
        (s_assignedItem == dItemNo_KANTERA_e || s_assignedItem == dItemNo_KANTERA2_e);
    const bool litNow = link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_1);

    if (litNow) {
        s_qaLanternLit = lanternAssigned;
        return;
    }

    if (!s_qaLanternLit || !lanternAssigned) {
        return;
    }

    if (s_qaPreEquipItem == dItemNo_KANTERA_e) {
        s_qaLanternLit = false;
        return;
    }
    if (link->mEquipItem == dItemNo_NONE_e && link->field_0x2fde == dItemNo_NONE_e) {
        s_qaLanternLit = false;
        return;
    }
    if (dComIfGs_getOil() == 0) {
        s_qaLanternLit = false;
        return;
    }
    if (link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE)) {
        s_qaLanternLit = false;
        return;
    }

    if (link->doTrigger()) {
        s_qaLanternLit = false;
        return;
    }

    link->onNoResetFlg2(daPy_py_c::FLG2_UNK_1);
    link->mZ2Link.setKanteraState(2);
}

static void execute_generic_item(u8 itemNo) {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    if (link == nullptr) return;

    if (!qa_is_item_available(itemNo)) {
        play_error_se();
        return;
    }

    if (link->mEquipItem == itemNo) {
        s_deferredUseItem = itemNo;
        return;
    }

    if (itemNo == dItemNo_NORMAL_BOMB_e || itemNo == dItemNo_WATER_BOMB_e ||
        itemNo == dItemNo_POKE_BOMB_e) {
        execute_bomb_item(link, itemNo);
        return;
    }

    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, itemNo);

    int proc_type = link->checkNewItemChange(2);
    if (proc_type != 0) {
        link->changeItemTriggerKeepProc(2, proc_type);
        play_ok_se();
    } else {
        play_error_se();
    }

    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
}

void qa_execute_item(u8 itemNo) {
    if (itemNo == QA_ITEM_NONE) {
        return;
    }
    if (itemNo == dItemNo_HVY_BOOTS_e) {
        qa_extinguish_lantern_for_item();
        execute_iron_boots();
    } else if (itemNo == dItemNo_HORSE_FLUTE_e) {
        qa_extinguish_lantern_for_item();
        execute_horse_call();
    } else if (itemNo == dItemNo_KANTERA_e || itemNo == dItemNo_KANTERA2_e) {
        execute_lantern();
    } else if (qa_is_rod_item(itemNo)) {
        qa_extinguish_lantern_for_item();
        execute_fishing_rod();
    } else {
        qa_extinguish_lantern_for_item();
        execute_generic_item(itemNo);
    }
}

static void qa_bmg_item_name(u8 itemNo, char* out, size_t outSize) {
    out[0] = '\0';
    char msg[64];
    g_meter2_info.getString(static_cast<u32>(itemNo) + 0x165, msg, nullptr);
    size_t w = 0;
    for (size_t r = 0; msg[r] != '\0' && w + 1 < outSize; r++) {
        u8 ch = static_cast<u8>(msg[r]);
        out[w++] = (ch < 0x20) ? ' ' : static_cast<char>(ch);
    }
    while (w > 0 && out[w - 1] == ' ') {
        w--;
    }
    out[w] = '\0';
}

void qa_item_label(u8 itemNo, char* buf, size_t bufSize) {
    buf[0] = '\0';
    if (itemNo == QA_ITEM_NONE) {
        return;
    }

    if (itemNo == dItemNo_HORSE_FLUTE_e) {
        std::snprintf(buf, bufSize, "Horse Call");
        return;
    }
    if (itemNo == dItemNo_HVY_BOOTS_e) {
        if (qa_load_boots_worn()) {
            std::snprintf(buf, bufSize, "Iron Boots [Equipped]");
        } else {
            std::snprintf(buf, bufSize, "Iron Boots");
        }
        return;
    }
    if (itemNo == dItemNo_KANTERA_e || itemNo == dItemNo_KANTERA2_e) {
        if (qa_is_lantern_active()) {
            std::snprintf(buf, bufSize, "Lantern [Active]");
        } else if (dComIfGs_getOil() == 0) {
            std::snprintf(buf, bufSize, "Lantern (No Oil)");
        } else {
            std::snprintf(buf, bufSize, "Lantern");
        }
        return;
    }
    if (qa_is_rod_item(itemNo)) {
        std::snprintf(buf, bufSize, "Fishing Rod");
        return;
    }

    static u8 s_nameCacheItem = QA_ITEM_NONE;
    static char s_nameCache[64];
    if (s_nameCacheItem != itemNo) {
        qa_bmg_item_name(itemNo, s_nameCache, sizeof(s_nameCache));
        s_nameCacheItem = itemNo;
    }
    std::snprintf(buf, bufSize, "%s", s_nameCache);
}

DEFINE_HOOK(&mDoCPd_c::read, PadReadRadialMenuHook);
DEFINE_HOOK(&dMeter2Draw_c::draw, Meter2DrawRadialMenuHook);
DEFINE_HOOK(&daAlink_c::setStickData, QaSetStickDataHook);

static int s_holdFrames = 0;
static bool s_dpadCancelLatch = false;
static bool s_editDpadDownLatch = false;
static int s_repeatDir = 0;
static int s_repeatTimer = 0;
static int s_repeatCount = 0;

struct QaDpadRepeat {
    int dir;
    int timer;
    int count;
};
static QaDpadRepeat s_dpadRepeatX;
static QaDpadRepeat s_dpadRepeatY;
static QaDpadRepeat s_shoulderRepeat;

static dMeter2Draw_c* s_lastDraw = nullptr;
static J2DScreen* s_lastScreen = nullptr;

static void suppress_menu_buttons(interface_of_controller_pad& pad) {
    const u16 qaBit = controls_binding_bit(CTRL_BIND_QUICK_ACCESS);
    pad.mButtonFlags &= ~qaBit;
    pad.mPressedButtonFlags &= ~qaBit;

    pad.mCStickPosX = 0.0f;
    pad.mCStickPosY = 0.0f;
    pad.mCStickValue = 0.0f;

    if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
        pad.mButtonFlags &= ~PAD_BUTTON_A;
    }
}

static void reset_repeat_state() {
    s_repeatDir = 0;
    s_repeatTimer = 0;
    s_repeatCount = 0;
    s_dpadRepeatX = QaDpadRepeat{};
    s_dpadRepeatY = QaDpadRepeat{};
    s_shoulderRepeat = QaDpadRepeat{};
}

static u8 s_aimItem = QA_ITEM_NONE;

static bool qa_item_has_aim_mode(u8 itemNo) {
    switch (itemNo) {
    case dItemNo_IRONBALL_e:
    case dItemNo_BOW_e:
    case dItemNo_HAWK_EYE_e:
    case dItemNo_HOOKSHOT_e:
    case dItemNo_W_HOOKSHOT_e:
    case dItemNo_BOMB_ARROW_e:
    case dItemNo_HAWK_ARROW_e:
        return true;
    default:
        return false;
    }
}

static void qa_cancel_item_aim(daAlink_c* link) {
    if (link != nullptr) {
        link->mItemButton &= ~0x04;
    }
    s_aimItem = QA_ITEM_NONE;
}

static void qa_enter_item_aim(u8 itemNo) {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr || !qa_is_item_available(itemNo)) {
        play_error_se();
        return;
    }
    g_dComIfG_gameInfo.play.setSelectItem(2, itemNo);
    const int proc_type = link->checkNewItemChange(2);
    if (proc_type != 0) {
        link->changeItemTriggerKeepProc(2, proc_type);
    }
    s_aimItem = itemNo;
    play_ok_se();
}

static void qa_run_item_repress(daAlink_c* link, u8 itemNo) {
    if (link->checkEquipAnime() || link->checkKandelaarSwingAnime() ||
        link->checkCopyRodThrowAnime() || link->checkBoomerangThrowAnime()) {
        return;
    }

    u8 oldGpItem = dComIfGp_getSelectItem(2);
    g_dComIfG_gameInfo.play.setSelectItem(2, itemNo);
    link->mSelectItemId = 2;
    const int action = link->checkItemActionInitStart();
    g_dComIfG_gameInfo.play.setSelectItem(2, oldGpItem);
    if (action == -1) {
        play_error_se();
    } else {
        play_ok_se();
    }
}

static void on_set_stick_data_qa_post(ModContext*, void* args, void*, void*) {
    if (!g_configQuickAccessEnabled || args == nullptr) {
        return;
    }
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) {
        s_aimItem = QA_ITEM_NONE;
        s_deferredUseItem = QA_ITEM_NONE;
        return;
    }

    if (s_aimItem != QA_ITEM_NONE) {
        if (link->checkWolf() || !qa_is_item_available(s_aimItem)) {
            s_aimItem = QA_ITEM_NONE;
        } else {
            g_dComIfG_gameInfo.play.setSelectItem(2, s_aimItem);
            link->mSelectItemId = 2;
            link->mItemButton |= 0x04;
        }
    }

    if (s_deferredUseItem != QA_ITEM_NONE) {
        const u8 itemNo = s_deferredUseItem;
        s_deferredUseItem = QA_ITEM_NONE;
        if (!link->checkWolf() && qa_is_item_available(itemNo)) {
            qa_run_item_repress(link, itemNo);
        }
    }
}

static void swallow_shoulder_triggers(interface_of_controller_pad& pad) {
    pad.mTriggerLeft = 0.0f;
    pad.mTriggerRight = 0.0f;
    pad.mTrigLockL = false;
    pad.mTrigLockR = false;
    pad.mHoldLockL = false;
    pad.mHoldLockR = false;
    pad.mPressedButtonFlags &= ~(PAD_TRIGGER_L | PAD_TRIGGER_R);
    pad.mButtonFlags &= ~(PAD_TRIGGER_L | PAD_TRIGGER_R);
}

void qa_strip_tap_action() {
    if (s_assignedItem == QA_ITEM_NONE) {
        play_error_se();
        return;
    }
    if (s_aimItem != QA_ITEM_NONE) {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link != nullptr) {
            link->mItemButton &= ~0x04;
        }
        s_aimItem = QA_ITEM_NONE;
    } else if (qa_item_has_aim_mode(s_assignedItem)) {
        qa_enter_item_aim(s_assignedItem);
    } else {
        qa_execute_item(s_assignedItem);
    }
}

static void close_menu() {
    if (s_editMode) {
        s_editMode = false;
        quick_access_edit_exit();
    }
    s_editDpadDownLatch = false;
    s_menuOpen = false;
    s_holdFrames = 0;
    s_deferredUseItem = QA_ITEM_NONE;
    reset_repeat_state();
}

void qa_enter_edit_mode() {
    s_editMode = true;
    s_editDpadDownLatch = true;
    quick_access_strip_cursor_reset();
    quick_access_edit_enter();
    s_selectedSlot = SLOT_NONE;
    reset_repeat_state();
    play_ok_se();
}

static void leave_edit_mode(bool menuStillWanted) {
    s_editMode = false;
    s_editDpadDownLatch = false;
    quick_access_edit_exit();
    if (menuStillWanted) {
        reset_repeat_state();
    } else {
        close_menu();
    }
}

static const int QA_REPEAT_START_INTERVAL = 8;
static const int QA_REPEAT_ACCEL_STEP = 1;
static const int QA_REPEAT_MIN_INTERVAL = 2;

static int qa_repeat_threshold(int count) {
    int interval = QA_REPEAT_START_INTERVAL - count * QA_REPEAT_ACCEL_STEP;
    if (interval < QA_REPEAT_MIN_INTERVAL) {
        interval = QA_REPEAT_MIN_INTERVAL;
    }
    return interval;
}

static void strip_cycle_step(int dir) {
    quick_access_strip_cycle(dir);
}

static void edit_step_x(int dir) {
    quick_access_edit_move(dir, 0);
}

static void edit_step_y(int dir) {
    quick_access_edit_move(0, dir);
}

static void edit_rotate_step(int dir) {
    quick_access_edit_rotate_slot(dir);
}

static void update_axis_repeat(int dir, int& stateDir, int& stateTimer, int& stateCount,
                               void (*step)(int)) {
    if (dir != 0) {
        if (dir != stateDir) {
            stateDir = dir;
            stateTimer = 0;
            stateCount = 0;
            step(dir);
        } else {
            stateTimer++;
            if (stateTimer >= qa_repeat_threshold(stateCount)) {
                stateTimer = 0;
                stateCount++;
                step(dir);
            }
        }
    } else {
        stateDir = 0;
        stateTimer = 0;
        stateCount = 0;
    }
}

static void update_stick_repeat(f32 stickX, void (*step)(int)) {
    const f32 kDeadzone = 0.35f;
    int dir = 0;
    if (stickX >= kDeadzone) {
        dir = 1;
    } else if (stickX <= -kDeadzone) {
        dir = -1;
    }

    update_axis_repeat(dir, s_repeatDir, s_repeatTimer, s_repeatCount, step);
}

static void qa_wolf_sun_song() {
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr) {
        play_error_se();
        return;
    }
    link->handleWolfHowl();
}

static void wolf_quick_access_input(interface_of_controller_pad& pad) {
    bool held = (pad.mButtonFlags & controls_binding_bit(CTRL_BIND_QUICK_ACCESS)) != 0;
    if (s_dpadCancelLatch) {
        if (!held) {
            s_dpadCancelLatch = false;
        }
        held = false;
    }

    if (!held) {
        if (s_menuOpen) {
            close_menu();
        } else if (s_holdFrames > 0) {
            s_holdFrames = 0;
            qa_wolf_sun_song();
        }
        return;
    }

    if (!s_menuOpen) {
        s_holdFrames++;
        if (s_holdFrames >= QA_TAP_FRAMES) {
            s_menuOpen = true;
            qa_invalidate_msg_window();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 0.9f, 1.2f, -1.0f, -1.0f, 0);
        }
        suppress_menu_buttons(pad);
        return;
    }

    if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
        pad.mButtonFlags &= ~PAD_BUTTON_A;
        s_dpadCancelLatch = true;
        close_menu();
        qa_wolf_sun_song();
        suppress_menu_buttons(pad);
        return;
    }

    if ((pad.mPressedButtonFlags & PAD_BUTTON_B) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
        pad.mButtonFlags &= ~PAD_BUTTON_B;
        s_dpadCancelLatch = true;
        close_menu();
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        suppress_menu_buttons(pad);
        return;
    }

    pad.mPressedButtonFlags &= ~(PAD_BUTTON_X | PAD_BUTTON_Y);
    pad.mButtonFlags &= ~(PAD_BUTTON_X | PAD_BUTTON_Y);
    pad.mPressedButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);
    pad.mButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);
    swallow_shoulder_triggers(pad);

    suppress_menu_buttons(pad);
}

static void on_pad_read_quick_access_post(ModContext*, void*, void*, void*) {
    if (!g_configQuickAccessEnabled || isTitleOrMainMenu()) {
        close_menu();
        s_menuAlpha = 0.0f;
        return;
    }

    sync_wheel_down_assignment();

    qa_tick_bomb_tracking();

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);

    u8 windowStatus = dMeter2Info_getWindowStatus();
    bool isMenuOrPause = (windowStatus != 0) || dComIfGp_isPauseFlag() || dScnPly_c::isPause()
                         || dComIfGp_event_runCheck() || dMeter2Info_isShopTalkFlag()
                         || dMsgObject_isTalkNowCheck();

    if (isMenuOrPause) {
        if (s_aimItem != QA_ITEM_NONE) {
            qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
        }
        close_menu();
        return;
    }

    if (s_aimItem != QA_ITEM_NONE) {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link == nullptr || !qa_is_item_available(s_aimItem)) {
            s_aimItem = QA_ITEM_NONE;
        }
    }

    if (s_aimItem == QA_ITEM_NONE && !s_editMode && !s_menuOpen &&
        !quick_access_bottles_hotkey_active() &&
        (pad.mPressedButtonFlags & PAD_BUTTON_B) != 0)
    {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
        const u8 assigned = s_assignedItem;
        if (link != nullptr && assigned != QA_ITEM_NONE &&
            items_same_family(link->mEquipItem, assigned) &&
            !link->checkEquipAnime() && !link->checkKandelaarSwingAnime() &&
            !link->checkCopyRodThrowAnime() && !link->checkBoomerangThrowAnime() &&
            !link->checkNoResetFlg2(daPy_py_c::FLG2_FISHING_CAST_WAIT) &&
            !link->checkNoResetFlg0(daPy_py_c::FLG0_WATER_IN_MOVE) &&
            !link->checkModeFlg(0x40000) && !link->checkReinRide() && !link->checkCanoeRide())
        {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
            pad.mButtonFlags &= ~PAD_BUTTON_B;
            link->allUnequip(TRUE);
            return;
        }
    }

    if (isWolfPlayer()) {
        if (s_aimItem != QA_ITEM_NONE) {
            qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
        }
        if (s_editMode) {
            s_dpadCancelLatch = true;
            close_menu();
        }
        if (!quick_access_bottles_hotkey_active()) {
            wolf_quick_access_input(pad);
        }
        return;
    }

    bool dpadDownHeld = (pad.mButtonFlags & controls_binding_bit(CTRL_BIND_QUICK_ACCESS)) != 0;
    if (s_dpadCancelLatch) {
        if (!dpadDownHeld) {
            s_dpadCancelLatch = false;
        }
        dpadDownHeld = false;
    }
    if (quick_access_bottles_hotkey_active()) {
        dpadDownHeld = false;
    }
    if (s_editMode && s_editDpadDownLatch &&
        (pad.mButtonFlags & controls_binding_bit(CTRL_BIND_QUICK_ACCESS)) == 0) {
        s_editDpadDownLatch = false;
    }

    if (s_editMode) {
        f32 stickX = pad.mCStickPosX;

        if ((pad.mPressedButtonFlags & PAD_BUTTON_B) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
            pad.mButtonFlags &= ~PAD_BUTTON_B;
            s_dpadCancelLatch = true;
            close_menu();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            suppress_menu_buttons(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_X) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_X;
            pad.mButtonFlags &= ~PAD_BUTTON_X;
            s_editDpadDownLatch = false;
            dpadDownHeld = (pad.mButtonFlags & controls_binding_bit(CTRL_BIND_QUICK_ACCESS)) != 0;
            leave_edit_mode(dpadDownHeld);
            play_ok_se();
            suppress_menu_buttons(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
            pad.mButtonFlags &= ~PAD_BUTTON_A;
            if (!quick_access_edit_toggle_current()) {
                play_error_se();
            }
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_Y) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_Y;
            pad.mButtonFlags &= ~PAD_BUTTON_Y;
            quick_access_edit_clear_slot();
        }

        update_stick_repeat(stickX, quick_access_edit_cycle);

        const int shoulderDir = (pad.mButtonFlags & PAD_TRIGGER_R) ? 1
                              : (pad.mButtonFlags & PAD_TRIGGER_L) ? -1 : 0;
        update_axis_repeat(shoulderDir, s_shoulderRepeat.dir, s_shoulderRepeat.timer,
                           s_shoulderRepeat.count, edit_rotate_step);

        swallow_shoulder_triggers(pad);

        const int xDir = (pad.mButtonFlags & PAD_BUTTON_LEFT) ? -1
                       : (pad.mButtonFlags & PAD_BUTTON_RIGHT) ? 1 : 0;
        const bool downHeldForGrid =
            ((pad.mButtonFlags & PAD_BUTTON_DOWN) != 0) && !s_editDpadDownLatch;
        const int yDir = (pad.mButtonFlags & PAD_BUTTON_UP) ? -1 : (downHeldForGrid ? 1 : 0);
        update_axis_repeat(xDir, s_dpadRepeatX.dir, s_dpadRepeatX.timer, s_dpadRepeatX.count,
                           edit_step_x);
        update_axis_repeat(yDir, s_dpadRepeatY.dir, s_dpadRepeatY.timer, s_dpadRepeatY.count,
                           edit_step_y);

        const u32 dpadAll = PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT;
        pad.mPressedButtonFlags &= ~dpadAll;
        pad.mButtonFlags &= ~dpadAll;

        suppress_menu_buttons(pad);
        return;
    }

    if (g_configQuickAccessAppearance == QA_APPEARANCE_RADIAL) {
        if (!dpadDownHeld) {
            if (s_menuOpen) {
                s_menuOpen = false;
                if (s_aimItem != QA_ITEM_NONE) {
                    qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
                }
                if (s_selectedSlot != SLOT_NONE) {
                    u8 active[QA_QUICK_SLOTS];
                    const int count = qa_get_active_items(active);
                    if (s_selectedSlot >= 0 && s_selectedSlot < count) {
                        s_assignedItem = active[s_selectedSlot];
                        qa_custom_store();
                        play_ok_se();
                    }
                }
                close_menu();
            } else if (s_holdFrames > 0) {
                s_holdFrames = 0;
                qa_strip_tap_action();
            }
            return;
        }

        if (!s_menuOpen) {
            s_holdFrames++;
            if (s_holdFrames < QA_TAP_FRAMES) {
                reset_repeat_state();
                suppress_menu_buttons(pad);
                return;
            }
            s_menuOpen = true;
            s_selectedSlot = SLOT_NONE;
            qa_invalidate_msg_window();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 0.9f, 1.2f, -1.0f, -1.0f, 0);
        }

        f32 stickX = pad.mCStickPosX;
        f32 stickY = pad.mCStickPosY;
        f32 stickMag = std::sqrt(stickX * stickX + stickY * stickY);

        if ((pad.mPressedButtonFlags & PAD_BUTTON_X) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_X;
            pad.mButtonFlags &= ~PAD_BUTTON_X;
            qa_enter_edit_mode();
            suppress_menu_buttons(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_B) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
            pad.mButtonFlags &= ~PAD_BUTTON_B;
            s_dpadCancelLatch = true;
            close_menu();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            suppress_menu_buttons(pad);
            return;
        }

        if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
            pad.mButtonFlags &= ~PAD_BUTTON_A;
            u8 active[QA_QUICK_SLOTS];
            const int count = qa_get_active_items(active);
            if (s_selectedSlot != SLOT_NONE && s_selectedSlot >= 0 && s_selectedSlot < count) {
                if (s_aimItem != QA_ITEM_NONE) {
                    qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
                }
                s_assignedItem = active[s_selectedSlot];
                qa_custom_store();
                play_ok_se();
                s_dpadCancelLatch = true;
                close_menu();
                suppress_menu_buttons(pad);
                return;
            }
        }

        pad.mButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);
        pad.mPressedButtonFlags &= ~(PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT);
        swallow_shoulder_triggers(pad);

        suppress_menu_buttons(pad);
        quick_access_radial_select(stickX, stickY, stickMag);
        return;
    }

    if (!dpadDownHeld) {
        if (s_menuOpen) {
            s_menuOpen = false;
            if (s_aimItem != QA_ITEM_NONE) {
                qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
            }
            if (s_selectedSlot != SLOT_NONE) {
                const u8 sel = qa_custom_item(s_selectedSlot);
                if (sel != QA_ITEM_NONE && qa_is_item_available(sel)) {
                    s_assignedItem = sel;
                    qa_custom_store();
                    play_ok_se();
                } else if (s_assignedItem != QA_ITEM_NONE) {
                    s_assignedItem = QA_ITEM_NONE;
                    qa_custom_store();
                    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f,
                                             -1.0f, -1.0f, 0);
                }
            }
            close_menu();
        } else if (s_holdFrames > 0) {
            s_holdFrames = 0;
            qa_strip_tap_action();
        }
        return;
    }

    f32 stickX = pad.mCStickPosX;

    if (!s_menuOpen) {
        s_holdFrames++;
        if (s_holdFrames >= QA_TAP_FRAMES) {
            s_menuOpen = true;
            if (s_selectedSlot == SLOT_NONE || s_selectedSlot < 0 ||
                s_selectedSlot >= QA_QUICK_SLOTS) {
                quick_access_strip_reset_selection();
            }
            qa_invalidate_msg_window();
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 0.9f, 1.2f, -1.0f, -1.0f, 0);
        }
        reset_repeat_state();
        suppress_menu_buttons(pad);
        return;
    }

    if ((pad.mPressedButtonFlags & PAD_BUTTON_X) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_X;
        pad.mButtonFlags &= ~PAD_BUTTON_X;
        qa_enter_edit_mode();
        suppress_menu_buttons(pad);
        return;
    }

    if ((pad.mPressedButtonFlags & PAD_BUTTON_B) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_B;
        pad.mButtonFlags &= ~PAD_BUTTON_B;
        s_dpadCancelLatch = true;
        close_menu();
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        suppress_menu_buttons(pad);
        return;
    }

    if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
        pad.mButtonFlags &= ~PAD_BUTTON_A;
        const u8 sel = (s_selectedSlot != SLOT_NONE && s_selectedSlot >= 0 &&
                        s_selectedSlot < QA_QUICK_SLOTS)
                           ? qa_custom_item(s_selectedSlot)
                           : QA_ITEM_NONE;
        if (sel != QA_ITEM_NONE && qa_is_item_available(sel)) {
            if (s_aimItem != QA_ITEM_NONE) {
                qa_cancel_item_aim(static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass()));
            }
            s_assignedItem = sel;
            qa_custom_store();
            play_ok_se();
            s_dpadCancelLatch = true;
            close_menu();
            suppress_menu_buttons(pad);
            return;
        }
    }

    update_stick_repeat(stickX, strip_cycle_step);

    if ((pad.mPressedButtonFlags & PAD_BUTTON_LEFT) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_LEFT;
        pad.mButtonFlags &= ~PAD_BUTTON_LEFT;
        quick_access_strip_cycle(-1);
    }
    if ((pad.mPressedButtonFlags & PAD_BUTTON_RIGHT) != 0) {
        pad.mPressedButtonFlags &= ~PAD_BUTTON_RIGHT;
        pad.mButtonFlags &= ~PAD_BUTTON_RIGHT;
        quick_access_strip_cycle(1);
    }

    const int shoulderDir = (pad.mButtonFlags & PAD_TRIGGER_R) ? 1
                          : (pad.mButtonFlags & PAD_TRIGGER_L) ? -1 : 0;
    update_axis_repeat(shoulderDir, s_shoulderRepeat.dir, s_shoulderRepeat.timer,
                       s_shoulderRepeat.count, strip_cycle_step);

    swallow_shoulder_triggers(pad);

    suppress_menu_buttons(pad);
}

static dusk::config::ConfigVar<f32>* s_qaHudScaleVar = nullptr;

f32 qa_user_hud_scale() {
    if (s_qaHudScaleVar != nullptr) {
        f32 scale = s_qaHudScaleVar->getValue();
        if (scale < 0.5f) scale = 0.5f;
        if (scale > 2.0f) scale = 2.0f;
        return scale;
    }
    return 1.0f;
}

static bool s_scaledHudActive = false;
static JGeometry::TBox2<f32> s_savedHudOrtho;

void qa_hud_scale_begin(f32 anchorX, f32 anchorY) {
    const f32 s = qa_user_hud_scale();
    if (s == 1.0f || s_scaledHudActive) return;
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port == nullptr) return;
    auto* ortho = reinterpret_cast<J2DOrthoGraph*>(port);
    s_savedHudOrtho = *ortho->getOrtho();
    ortho->setOrtho(anchorX + (s_savedHudOrtho.i.x - anchorX) / s,
                    anchorY + (s_savedHudOrtho.i.y - anchorY) / s,
                    s_savedHudOrtho.getWidth() / s, s_savedHudOrtho.getHeight() / s, -1.0f, 1.0f);
    ortho->setPort();
    s_scaledHudActive = true;
}

void qa_hud_scale_end() {
    if (!s_scaledHudActive) return;
    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port != nullptr) {
        auto* ortho = reinterpret_cast<J2DOrthoGraph*>(port);
        ortho->setOrtho(mDoGph_gInf_c::getMinXF(), mDoGph_gInf_c::getMinYF(),
                        mDoGph_gInf_c::getWidthF(), mDoGph_gInf_c::getHeightF(), -1.0f, 1.0f);
        ortho->setPort();
    }
    s_scaledHudActive = false;
}

static void draw_strip_hud_icon(J2DScreen* screen) {
    u8 assigned = s_assignedItem;
    if (assigned == QA_ITEM_NONE || !qa_is_item_available(assigned)) {
        return;
    }

    J2DPane* juji = screen->search(MULTI_CHAR('juji_n'));
    if (juji == nullptr || !juji->isVisible() || juji->getAlpha() == 0) {
        return;
    }

    J2DPicture* pic = nullptr;
    ResTIMG* img = nullptr;
    J2DPicture* pic2 = nullptr;
    if (!qa_get_item_icon(assigned, &pic, &img, &pic2)) {
        return;
    }

    const f32 hudScale = qa_user_hud_scale();
    bool isLantern = (assigned == dItemNo_KANTERA_e || assigned == dItemNo_KANTERA2_e);
    f32 targetH = (isLantern ? 29.5f : 26.0f);
    f32 targetW = 18.0f;
    if (img != nullptr && img->width > 0 && img->height > 0) {
        targetW = targetH * (static_cast<f32>(img->width) / static_cast<f32>(img->height));
    }

    const JGeometry::TBox2<f32>& bounds = juji->getGlbBounds();
    const f32 growW = bounds.getWidth() * (1.0f - hudScale) * 0.5f;
    const f32 shrinkH = bounds.getHeight() * (1.0f - hudScale) * 0.5f;
    f32 drawX = bounds.i.x + (bounds.getWidth() - targetW) * 0.30f * hudScale + growW;
    f32 drawY = bounds.i.y + (isLantern ? 31.5f : 33.0f) * hudScale + shrinkH;
    targetW *= hudScale;
    targetH *= hudScale;

    u8 alpha = juji->getAlpha();
    J2DPane* midnaPane = screen->search(MULTI_CHAR('midona_n'));
    if (midnaPane != nullptr && midnaPane->isVisible()) {
        alpha = midnaPane->getAlpha();
    }

    if (daAlink_c::checkRoom()) {
        alpha = static_cast<u8>(alpha * (g_drawHIO.mButtonXYItemDimAlpha / 255.0f));
    }

    pic->setAlpha(alpha);
    pic->draw(drawX, drawY, targetW, targetH, false, false, false);
    if (pic2 != nullptr) {
        pic2->setAlpha(alpha);
        pic2->draw(drawX, drawY, targetW, targetH, false, false, false);
    }

    qa_draw_item_ammo(assigned, drawX, drawY, targetW, targetH, alpha);
}

static void on_meter2_draw_quick_access_post(ModContext*, void* args, void*, void*) {
    if (!args || !g_configQuickAccessEnabled || isTitleOrMainMenu()) {
        s_menuAlpha = 0.0f;
        quick_access_strip_cursor_reset();
        return;
    }

    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    if (!draw || !draw->getMainScreenPtr()) {
        return;
    }

    J2DScreen* screen = draw->getMainScreenPtr();
    if (draw != s_lastDraw || screen != s_lastScreen) {
        s_lastDraw = draw;
        s_lastScreen = screen;
        qa_reset_icon_caches();
        quick_access_radial_reset();
        quick_access_strip_reset();
    }

    if (!isWolfPlayer()) {
        J2DGrafContext* hudCtx = dComIfGp_getCurrentGrafPort();
        if (hudCtx) hudCtx->setup2D();
        draw_strip_hud_icon(screen);
    }

    if (s_menuOpen) {
        s_menuAlpha += (1.0f - s_menuAlpha) * 0.35f;
        if (s_menuAlpha >= 0.99f) {
            s_menuAlpha = 1.0f;
        }
    } else {
        s_menuAlpha += (0.0f - s_menuAlpha) * 0.12f;
        if (s_menuAlpha < 0.005f) {
            s_menuAlpha = 0.0f;
        }
    }

    if (s_menuAlpha < 0.01f) {
        quick_access_strip_cursor_present();
        return;
    }

    s_glowTimer += 0.045f;
    if (s_glowTimer >= 6.2831853f) s_glowTimer -= 6.2831853f;

    f32 glow = 0.5f + 0.5f * std::sin(s_glowTimer);

    u8 alpha = static_cast<u8>(s_menuAlpha * 255.0f);

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }

    const f32 screenW = screen->getWidth();
    const f32 screenH = screen->getHeight();

    if (s_editMode) {
        quick_access_edit_draw(screenW, screenH, alpha, glow);
        return;
    }

    if (isWolfPlayer()) {
        quick_access_wolf_draw(screenW, screenH, alpha, glow);
        return;
    }

    if (g_configQuickAccessAppearance == QA_APPEARANCE_RADIAL) {
        quick_access_radial_draw(screenW * 0.5f, screenH * 0.5f, alpha, glow);
        return;
    }

    quick_access_strip_draw(screenW, screenH, alpha, glow);
}

ModResult init_quick_access(const HookService* hook_svc, const SaveService* save_svc,
                            ModContext* mod_ctx, ModError*) {
    if (hook_svc) {
        mods::hook::add_post<PadReadRadialMenuHook>(hook_svc, on_pad_read_quick_access_post);
        mods::hook::add_post<Meter2DrawRadialMenuHook>(hook_svc, on_meter2_draw_quick_access_post);
        mods::hook::add_post<QaSetStickDataHook>(hook_svc, on_set_stick_data_qa_post);
        mods::hook::add_post<QaCheckReadyItemHook>(hook_svc, on_check_ready_item_qa_post);
        mods::hook::add_pre<QaAlinkExecuteHook>(hook_svc, on_qa_alink_execute_pre);
        mods::hook::add_post<QaAlinkExecuteHook>(hook_svc, on_qa_alink_execute_post);
        mods::hook::add_pre<QaSetHeavyBootsHook>(hook_svc, on_qa_set_heavy_boots_pre);
        mods::hook::add_pre<QaBootsEquipInitHook>(hook_svc, on_qa_boots_equip_init_pre);
    }

    s_saveSvc = save_svc;
    s_modCtx = mod_ctx;
    if (hook_svc != nullptr && s_modCtx != nullptr) {
        void* addr = nullptr;
        using QaGetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
        if (hook_svc->resolve(s_modCtx, "dusk::config::GetConfigVar", &addr, nullptr) == MOD_OK) {
            s_qaHudScaleVar =
                static_cast<dusk::config::ConfigVar<f32>*>(
                    reinterpret_cast<QaGetConfigVarFn>(addr)("game.hudScale"));
        }
    }
    if (save_svc != nullptr && mod_ctx != nullptr) {
        save_svc->observe_saves(mod_ctx, on_new_save_reset_items,
                                on_save_loaded_restore_items, nullptr, nullptr,
                                &s_saveObserver);
        on_save_loaded_restore_items(mod_ctx, 0, nullptr);
    }
    return MOD_OK;
}

void update_quick_access(const LogService*, ModContext* mod_ctx) {
    s_modCtx = mod_ctx;
    update_quick_access_itemwheel();
}

ModContext* qa_mod_ctx() {
    return s_modCtx;
}

void shutdown_quick_access() {
    close_menu();
    s_menuAlpha = 0.0f;
    qa_reset_icon_caches();
    qa_shutdown_item_ammo();
    quick_access_radial_reset();
    quick_access_strip_reset();
    quick_access_edit_shutdown();
    quick_access_wolf_shutdown();
    qa_sel_cursor_destroy();
    s_lastDraw = nullptr;
    s_lastScreen = nullptr;
    shutdown_quick_access_itemwheel();
}
