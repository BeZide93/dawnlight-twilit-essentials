#include "collection_internal.hpp"

#include "JSystem/JKernel/JKRArchive.h"

#include <new>

ModContext* g_modCtx = nullptr;
const SaveService* g_saveSvc = nullptr;
const LogService* g_logSvc = nullptr;

dMenu_Collect2D_c* s_currentCollect2D = nullptr;
bool s_needReloadCollect = false;

void log_collect_info(const char* fmt, ...) {
    if (g_logSvc == nullptr || g_modCtx == nullptr) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_logSvc->info(g_modCtx, buf);
}

void log_collect_warn(const char* fmt, ...) {
    if (g_logSvc == nullptr || g_modCtx == nullptr) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_logSvc->warn(g_modCtx, buf);
}

// ---------------------------------------------------------------------------
// Panes
// ---------------------------------------------------------------------------

u64 cl_make_tag(char p0, char p1, char p2, char p3, u8 a, u8 b) {
    const u8 chars[6] = {static_cast<u8>(p0), static_cast<u8>(p1), static_cast<u8>(p2),
                         static_cast<u8>(p3), static_cast<u8>('0' + a), static_cast<u8>('0' + b)};
    u64 tag = 0;
    for (u8 c : chars) tag = (tag << 8) | c;
    return tag;
}

static bool is_picture(J2DPane* pane) {
    return pane->getTypeID() == 18 || pane->getTypeID() == 19 ||
           pane->getKind() == 'PIC1' || pane->getKind() == 'PIC2';
}

const ResTIMG* cl_pane_texture(J2DPane* pane) {
    if (pane == nullptr) return nullptr;
    if (is_picture(pane)) {
        JUTTexture* tex = static_cast<J2DPicture*>(pane)->getTexture(0);
        if (tex != nullptr && tex->getTexInfo() != nullptr) return tex->getTexInfo();
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr; child = child->getNextChildPane()) {
        if (is_picture(child)) {
            JUTTexture* tex = static_cast<J2DPicture*>(child)->getTexture(0);
            if (tex != nullptr && tex->getTexInfo() != nullptr) return tex->getTexInfo();
        }
    }
    return nullptr;
}

void cl_set_pane_pos(J2DPane* pane, f32 x, f32 y) {
    if (pane != nullptr) pane->translate(x, y);
}

Vec cl_pane_global_center(J2DPane* pane) {
    CPaneMgr probe;
    return probe.getGlobalVtxCenter(pane, false, 0);
}

// ---------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------

namespace {

struct IconCacheEntry {
    const char* path;
    u16 fileId;
    ResTIMG* tex;       // owned copy (or texture replacement), 32-byte aligned
};

constexpr int kMaxIcons = 64;
IconCacheEntry s_icons[kMaxIcons] = {};
int s_iconCount = 0;

// Archive paths that are not in the mod's res/ (every failed load logs an error).
constexpr int kMaxMissing = 8;
const char* s_missingPaths[kMaxMissing] = {};
int s_missingCount = 0;

bool known_missing(const char* path) {
    for (int i = 0; i < s_missingCount; i++) {
        if (std::strcmp(s_missingPaths[i], path) == 0) return true;
    }
    return false;
}

bool same_path(const char* a, const char* b) {
    if (a == b) return true;
    if (a == nullptr || b == nullptr) return false;
    return std::strcmp(a, b) == 0;
}

ResTIMG* copy_texture(const void* data, u32 size) {
    if (data == nullptr || size < 0x20) return nullptr;
    void* mem = operator new[](size, std::align_val_t(32), std::nothrow);
    if (mem == nullptr) return nullptr;
    std::memcpy(mem, data, size);
    ResTIMG* img = static_cast<ResTIMG*>(mem);
    img->alphaEnabled = 1;
    return img;
}

ResTIMG* copy_from_archive(JKRArchive* arc, u16 fileId) {
    if (arc == nullptr) return nullptr;
    void* res = arc->getIdxResource(fileId);
    if (res == nullptr) res = arc->getResource(fileId);
    if (res == nullptr) return nullptr;
    return copy_texture(res, arc->getResSize(res));
}

ResTIMG* load_icon_uncached(const char* path, u16 fileId) {
    const ResourceService* res = cl_resource_service();

    if (fileId == 0xFFFF) {
        if (path == nullptr || res == nullptr || g_modCtx == nullptr) return nullptr;
        ResourceBuffer buf = RESOURCE_BUFFER_INIT;
        if (res->load(g_modCtx, path, &buf) != MOD_OK || buf.data == nullptr) return nullptr;
        ResTIMG* img = copy_texture(buf.data, static_cast<u32>(buf.size));
        res->free(g_modCtx, &buf);
        if (img == nullptr) return nullptr;
        ResTIMG* replaced = tex_replacements_apply(path, img);
        if (replaced != img) operator delete[](static_cast<void*>(img), std::align_val_t(32));
        return replaced;
    }

    // Archive shipped in the mod's res/.
    if (path != nullptr && res != nullptr && g_modCtx != nullptr && !known_missing(path)) {
        ResourceBuffer buf = RESOURCE_BUFFER_INIT;
        if (res->load(g_modCtx, path, &buf) != MOD_OK || buf.data == nullptr) {
            if (s_missingCount < kMaxMissing) s_missingPaths[s_missingCount++] = path;
        } else {
            JKRHeap* heap = JKRHeap::getRootHeap();
            JKRArchive* arc = JKRArchive::mount(buf.data, heap, JKRArchive::MOUNT_DIRECTION_HEAD);
            ResTIMG* img = copy_from_archive(arc, fileId);
            if (arc != nullptr) JKRUnmountArchive(arc);
            res->free(g_modCtx, &buf);
            if (img != nullptr) return img;
        }
    }

    // The game's collection archive (Layout/clctres.arc, including overlay patches).
    return copy_from_archive(dComIfGp_getCollectResArchive(), fileId);
}

}  // namespace

ResTIMG* cl_load_icon(const char* path, IconArcRef iconArc) {
    if (path == nullptr && iconArc.fileId == 0xFFFF) return nullptr;

    for (int i = 0; i < s_iconCount; i++) {
        IconCacheEntry& e = s_icons[i];
        if (e.fileId == iconArc.fileId && same_path(e.path, path)) {
            if (e.tex != nullptr) return e.tex;
            // Failed before - retry (the game archive may not have been mounted yet).
            e.tex = load_icon_uncached(path, iconArc.fileId);
            return e.tex;
        }
    }

    ResTIMG* tex = load_icon_uncached(path, iconArc.fileId);
    if (s_iconCount < kMaxIcons) {
        s_icons[s_iconCount++] = {path, iconArc.fileId, tex};
    }
    if (tex == nullptr) {
        log_collect_info("collection-lib: icon '%s' (file 0x%04x) not found", path ? path : "",
                         iconArc.fileId);
    }
    return tex;
}

void cl_free_icons() {
    for (int i = 0; i < s_iconCount; i++) {
        if (s_icons[i].tex != nullptr) {
            operator delete[](static_cast<void*>(s_icons[i].tex), std::align_val_t(32));
        }
        s_icons[i] = {};
    }
    s_iconCount = 0;
    s_missingCount = 0;
}
