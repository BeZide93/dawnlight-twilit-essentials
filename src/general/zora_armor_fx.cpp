// ai helped a lot with this one
#include "zora_armor_fx.hpp"
#include "auto_zora_armor.hpp"

#include <collection_lib/collection_lib.hpp>

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_resorce.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_counter.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DMatBlock.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DTevs.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern const LogService* svc_log;
extern const ResourceService* svc_resource;
extern ModContext* mod_ctx;

DEFINE_HOOK(&daAlink_c::execute, ZoraFxExecuteHook);
DEFINE_HOOK(&daAlink_c::draw, ZoraFxDrawHook);
DEFINE_HOOK(&daAlink_c::modelDraw, ZoraFxModelDrawHook);

namespace {

constexpr int kNativeCount = ZORA_FX_CUSTOM_BASE;
constexpr int kMaxCustom = 12;
constexpr int kTypeCount = kNativeCount + kMaxCustom;
constexpr const char* kArcPath[kNativeCount] = {
    "/res/Object/Kmdl.arc", "/res/Object/Zmdl.arc", "/res/Object/Mmdl.arc", "/res/Object/Bmdl.arc"};
constexpr const char* kBodyName[kNativeCount] = {"al.bmd", "zl.bmd", "ml.bmd", "bl.bmd"};
constexpr const char* kHatName[kNativeCount] = {"al_head.bmd", "zl_head.bmd", "ml_head.bmd", "bl_head.bmd"};
constexpr const char* kFaceName[kNativeCount] = {"al_face.bmd", "zl_face.bmd", "al_face.bmd", "al_face.bmd"};
constexpr const char* kHandsName[kNativeCount] = {"al_hands.bmd", "al_hands.bmd", "al_hands.bmd", "bl_hands.bmd"};
constexpr const char* kNativeName[kNativeCount] = {"hero", "zora", "magic", "ordon"};

constexpr u32 kTagBmwr = 'BMWR';
constexpr u32 kDissolveDiffFlags = 0x11000084 | 0x2000400;
constexpr u32 kPlainDiffFlags = 0x11000084;
constexpr u32 kRootHeapReserve = 0x800000;
constexpr u32 kHeapHeadroom = 0x100000;
constexpr u16 kMaxJoints = 48;
constexpr u16 kMaxMaterials = 32;
constexpr u16 kHeadJoint = 4;
constexpr int kDuration = 30;
constexpr int kMaxWaitTicks = 90;
constexpr int kMaxSwapWaitTicks = 360;
constexpr u32 kMaxPoseAge = 3;
constexpr f32 kScrollSpeed = 0.0075f;
constexpr f32 kLineBelowFeet = 14.0f;
constexpr f32 kLineAboveHead = 26.0f;
constexpr int kTexDim = 128;
constexpr int kDropCount = 20;
constexpr int kDropsAbove = 12;
constexpr f32 kTau = 6.2831853f;

enum Mode { kModeBelow, kModeAbove, kModePlain };

const J3DTevStageInfo kBelowStage = {
    0x05, 0x08, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x07, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00,
};

const J3DTevStageInfo kAboveStage = {
    0x05, 0x08, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x07, 0x04, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00,
};

const Mtx kEffectBase = {
    {0.5f, 0.0f, 0.0f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.5f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

enum class LoadState : u8 { Idle, Mounting, Ready, Failed };

struct GhostSet {
    LoadState state = LoadState::Idle;
    bool wanted = false;
    mDoDvdThd_mountArchive_c* cmd = nullptr;
    JKRExpHeap* heap = nullptr;
    J3DModel* body = nullptr;
    J3DModel* hat = nullptr;
    J3DModel* face = nullptr;
    J3DModel* hands = nullptr;
};

struct Pose {
    bool valid = false;
    fpc_ProcID linkId = fpcM_ERROR_PROCESS_ID_e;
    u32 stamp = 0;
    Mtx base;
    Mtx body[kMaxJoints];
    Mtx hat[kMaxJoints];
    Mtx face[kMaxJoints];
    Mtx hands[kMaxJoints];
    u16 bodyNum = 0;
    u16 hatNum = 0;
    u16 faceNum = 0;
    u16 handsNum = 0;
};

struct Effect {
    bool active = false;
    fpc_ProcID linkId = fpcM_ERROR_PROCESS_ID_e;
    int fromType = ZORA_FX_NONE;
    int toType = ZORA_FX_NONE;
    int tick = 0;
    int waitTicks = 0;
    bool rising = true;
    bool waitingSwap = false;
    f32 scroll = 0.0f;
};

struct Drop {
    f32 s;
    f32 off;
    f32 r;
    f32 center;
};

GhostSet s_sets[kTypeCount];
Pose s_pose;
Effect s_fx;
bool s_drawGhosts = false;
u8* s_texBuf = nullptr;
ResTIMG* s_waterTex = nullptr;
bool s_texFailed = false;
J3DModel* s_cbModel = nullptr;
Mtx* s_cbSrc = nullptr;
u16 s_cbCount = 0;

void fx_log(const char* fmt, ...) {
    if (svc_log == nullptr || mod_ctx == nullptr) return;
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    svc_log->info(mod_ctx, msg);
}

bool valid_type(int type) {
    return type >= 0 && type < kTypeCount;
}

f32 smooth01(f32 e0, f32 e1, f32 x) {
    f32 t = (x - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

u8 to_u8(f32 v) {
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    return static_cast<u8>(v * 255.0f + 0.5f);
}

f32 water_edge(f32 s) {
    return 0.5f + 0.05f * std::sin(kTau * 2.0f * s + 0.3f) + 0.024f * std::sin(kTau * 3.0f * s + 1.7f) +
           0.007f * std::sin(kTau * 7.0f * s + 4.1f);
}

void put_texel(u8* dst, int x, int y, f32 r, f32 g, f32 b, f32 a) {
    const int tile = (y >> 2) * (kTexDim >> 2) + (x >> 2);
    const int p = ((y & 3) << 2) | (x & 3);
    u8* base = dst + tile * 64;
    base[p * 2] = to_u8(a);
    base[p * 2 + 1] = to_u8(r);
    base[32 + p * 2] = to_u8(g);
    base[32 + p * 2 + 1] = to_u8(b);
}

void build_water_pixels(u8* dst) {
    Drop drops[kDropCount];
    u32 seed = 0x5A17C3u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<f32>(seed >> 8) * (1.0f / 16777216.0f);
    };
    for (int k = 0; k < kDropCount; k++) {
        const bool above = k < kDropsAbove;
        drops[k].s = rnd();
        drops[k].off = above ? -(0.035f + 0.09f * rnd()) : (0.03f + 0.06f * rnd());
        drops[k].r = above ? (0.016f + 0.02f * rnd()) : (0.014f + 0.016f * rnd());
        drops[k].center = water_edge(drops[k].s) + drops[k].off;
    }

    for (int y = 0; y < kTexDim; y++) {
        const f32 t = (y + 0.5f) / kTexDim;
        for (int x = 0; x < kTexDim; x++) {
            const f32 s = (x + 0.5f) / kTexDim;
            const f32 d = t - water_edge(s);
            f32 field = d;
            for (const Drop& drop : drops) {
                f32 ds = s - drop.s;
                ds -= std::floor(ds + 0.5f);
                const f32 dt = t - drop.center;
                const f32 blob = drop.r - std::sqrt(ds * ds + dt * dt);
                field = drop.off < 0.0f ? std::max(field, blob) : std::min(field, -blob);
            }
            const f32 alpha = smooth01(-0.004f, 0.004f, field);
            const f32 core = std::exp(-(field * field) / (0.0105f * 0.0105f));
            const f32 halo = std::exp(-(field * field) / (0.03f * 0.03f));
            const f32 band = smooth01(0.006f, 0.025f, d) * (1.0f - smooth01(0.06f, 0.17f, d));
            const f32 u = kTau * s;
            const f32 v = kTau * t;
            const f32 n1 = std::sin(4.0f * u + 2.6f * std::sin(9.0f * v + 2.0f * u));
            const f32 n2 = std::sin(-3.0f * u + 13.0f * v + 1.8f * std::sin(5.0f * u - 7.0f * v));
            const f32 caustic = std::pow(std::max(0.0f, 1.0f - 0.5f * std::fabs(n1 + n2)), 7.0f) * band;
            const f32 r = 0.70f * core + 0.10f * halo + 0.20f * caustic;
            const f32 g = 0.90f * core + 0.38f * halo + 0.55f * caustic;
            const f32 b = 1.00f * core + 0.62f * halo + 0.80f * caustic;
            put_texel(dst, x, y, r, g, b, alpha);
        }
    }
}

bool build_water_texture() {
    if (s_waterTex != nullptr) return true;
    if (s_texFailed) return false;
    JKRHeap* root = JKRHeap::getRootHeap();
    const u32 texelBytes = kTexDim * kTexDim * 4;
    u8* buf = root != nullptr ? static_cast<u8*>(root->alloc(sizeof(ResTIMG) + texelBytes, 32)) : nullptr;
    if (buf == nullptr) {
        s_texFailed = true;
        fx_log("zora_fx: water texture alloc failed");
        return false;
    }
    std::memset(buf, 0, sizeof(ResTIMG));
    build_water_pixels(buf + sizeof(ResTIMG));
    ResTIMG* timg = reinterpret_cast<ResTIMG*>(buf);
    timg->format = GX_TF_RGBA8;
    timg->alphaEnabled = 1;
    timg->width = static_cast<u16>(kTexDim);
    timg->height = static_cast<u16>(kTexDim);
    timg->wrapS = GX_REPEAT;
    timg->wrapT = GX_CLAMP;
    timg->minFilter = GX_LINEAR;
    timg->magFilter = GX_LINEAR;
    timg->mipmapCount = 1;
    timg->imageOffset = static_cast<s32>(sizeof(ResTIMG));
    s_texBuf = buf;
    s_waterTex = timg;
    return true;
}

u32 res_size(JKRArchive* arc, void* raw) {
    u32 size = arc->getExpandedResSize(raw);
    const u8* bytes = static_cast<const u8*>(raw);
    const u32 header = (static_cast<u32>(bytes[8]) << 24) | (static_cast<u32>(bytes[9]) << 16) |
                       (static_cast<u32>(bytes[10]) << 8) | static_cast<u32>(bytes[11]);
    if (std::memcmp(bytes, "J3D2", 4) == 0 && header > 0 && header < 0x800000 &&
        (size == 0xFFFFFFFFu || header > size)) {
        size = header;
    }
    return size == 0xFFFFFFFFu ? 0 : size;
}

bool setup_dissolve(J3DModelData* data, bool below) {
    J3DTexture* tex = data->getTexture();
    if (tex == nullptr || tex->getNum() == 0 || s_waterTex == nullptr) return false;
    u16 warpIdx = 0xFFFF;
    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DMaterial* material = data->getMaterialNodePointer(i);
        J3DTevBlock* tev = material != nullptr ? material->getTevBlock() : nullptr;
        J3DTexGenBlock* texGen = material != nullptr ? material->getTexGenBlock() : nullptr;
        if (tev == nullptr || texGen == nullptr || texGen->getTexGenNum() == 0) return false;
        const u8 stageNum = tev->getTevStageNum();
        if (stageNum == 0) return false;
        J3DTevOrder* order = tev->getTevOrder(stageNum - 1);
        if (order == nullptr || order->getTexMap() != 3) return false;
        if (texGen->getTexMtx(texGen->getTexGenNum() - 1) == nullptr) return false;
        const u16 texNo = tev->getTexNo(3);
        if (warpIdx == 0xFFFF) warpIdx = texNo;
        if (texNo != warpIdx) return false;
        tev->setTevStage(stageNum - 1, J3DTevStage(below ? kBelowStage : kAboveStage));
    }
    if (warpIdx >= tex->getNum()) return false;
    tex->setResTIMG(warpIdx, *s_waterTex);
    return true;
}

J3DModel* build_model(JKRHeap* heap, void* raw, u32 size, Mode mode) {
    u8* copy = static_cast<u8*>(heap->alloc((size + 0x1F) & ~0x1Fu, 0x20));
    if (copy == nullptr) return nullptr;
    std::memcpy(copy, raw, size);
    J3DModelData* data = dRes_info_c::loaderBasicBmd(kTagBmwr, copy);
    if (data == nullptr || data->getMaterialNum() == 0 || data->getJointNum() == 0 ||
        data->getMaterialNum() > kMaxMaterials || data->getJointNum() > kMaxJoints) {
        return nullptr;
    }
    if (mode == kModePlain) {
        dRes_info_c::offWarpMaterial(data);
    } else if (!setup_dissolve(data, mode == kModeBelow)) {
        return nullptr;
    }
    data->simpleCalcMaterial(const_cast<MtxP>(j3dDefaultMtx));
    data->makeSharedDL();
    return mDoExt_J3DModel__create(data, 0x80000, mode == kModePlain ? kPlainDiffFlags : kDissolveDiffFlags);
}

void clear_models(GhostSet& set) {
    set.body = nullptr;
    set.hat = nullptr;
    set.face = nullptr;
    set.hands = nullptr;
}

bool always_hidden_name(const char* name) {
    return name != nullptr && (std::strstr(name, "skirt") != nullptr || std::strstr(name, "mask") != nullptr ||
                               std::strstr(name, "bootsB") != nullptr);
}

void apply_initial_visibility(GhostSet& set) {
    if (set.hands != nullptr) {
        J3DModelData* hands = set.hands->getModelData();
        for (u16 i = 0; i < hands->getMaterialNum(); i++) {
            J3DShape* shape = hands->getMaterialNodePointer(i)->getShape();
            if (shape != nullptr) shape->hide();
        }
    }
    J3DModelData* body = set.body->getModelData();
    JUTNameTab* names = body->getMaterialName();
    if (names == nullptr) return;
    for (u16 i = 0; i < body->getMaterialNum(); i++) {
        J3DShape* shape = body->getMaterialNodePointer(i)->getShape();
        if (shape != nullptr && always_hidden_name(names->getName(i))) shape->hide();
    }
}

const CustomEquipDef* custom_def(int type) {
    if (type < kNativeCount || type >= kTypeCount) return nullptr;
    const CustomEquipDef* def = custom_equip_get(type - kNativeCount);
    if (def == nullptr || def->kind != CE_TUNIC || def->modelArc == nullptr || def->modelArc[0] == '\0') {
        return nullptr;
    }
    return def;
}

const char* type_name(int type) {
    if (type >= 0 && type < kNativeCount) return kNativeName[type];
    const CustomEquipDef* def = custom_def(type);
    return def != nullptr && def->name != nullptr ? def->name : "custom";
}

int native_type_for_item(u8 item) {
    switch (item) {
    case dItemNo_WEAR_KOKIRI_e: return ZORA_FX_KOKIRI;
    case dItemNo_WEAR_ZORA_e: return ZORA_FX_ZORA;
    case dItemNo_ARMOR_e: return ZORA_FX_MAGIC;
    case dItemNo_WEAR_CASUAL_e: return ZORA_FX_CASUAL;
    default: return ZORA_FX_NONE;
    }
}

int base_type(int type) {
    const CustomEquipDef* def = custom_def(type);
    if (def == nullptr) return type;
    const int base = native_type_for_item(def->baseItem);
    return base != ZORA_FX_NONE ? base : ZORA_FX_KOKIRI;
}

bool contains_ci(const char* text, const char* pattern) {
    for (; *text != '\0'; text++) {
        const char* a = text;
        const char* b = pattern;
        while (*a != '\0' && *b != '\0' &&
               std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b))) {
            a++;
            b++;
        }
        if (*b == '\0') return true;
    }
    return false;
}

void* find_arc_bmd(JKRArchive* arc, const char* pattern) {
    if (arc == nullptr || arc->mFiles == nullptr || arc->mStringTable == nullptr) return nullptr;
    static const char* const kNotBody[] = {"head", "face", "hand", "kantera", "glow", "boot", "swb"};
    const u32 num = arc->countFile();
    for (u32 i = 0; i < num; i++) {
        if (arc->mFiles[i].isDirectory()) continue;
        const char* name = arc->mStringTable + arc->mFiles[i].getNameOffset();
        if (name == nullptr || !contains_ci(name, ".bmd")) continue;
        if (pattern != nullptr) {
            if (!contains_ci(name, pattern)) continue;
        } else {
            bool other = false;
            for (const char* part : kNotBody) other = other || contains_ci(name, part);
            if (other) continue;
        }
        return arc->fetchResource(&arc->mFiles[i], nullptr);
    }
    return nullptr;
}

bool find_parts(int type, JKRArchive* arc, void* raw[4]) {
    if (type < kNativeCount) {
        const char* names[4] = {kBodyName[type], kHatName[type], kFaceName[type], kHandsName[type]};
        for (int i = 0; i < 4; i++) {
            raw[i] = arc->getResource(kTagBmwr, names[i]);
            if (raw[i] == nullptr) {
                fx_log("zora_fx: %s missing %s", type_name(type), names[i]);
                return false;
            }
        }
        return true;
    }
    const CustomEquipDef* def = custom_def(type);
    if (def == nullptr) return false;
    if (def->modelFileId != 0xFFFF) {
        raw[0] = arc->getResource(static_cast<u16>(def->modelFileId));
        if (raw[0] == nullptr) raw[0] = arc->getIdxResource(def->modelFileId);
    }
    if (raw[0] == nullptr) raw[0] = find_arc_bmd(arc, nullptr);
    raw[1] = find_arc_bmd(arc, "head");
    if (raw[1] == nullptr) raw[1] = find_arc_bmd(arc, "hat");
    raw[2] = find_arc_bmd(arc, "face");
    raw[3] = find_arc_bmd(arc, "hand");
    if (raw[0] == nullptr || raw[1] == nullptr) {
        fx_log("zora_fx: %s has no body/head model", type_name(type));
        return false;
    }
    return true;
}

bool build_set(int type, JKRArchive* arc) {
    GhostSet& set = s_sets[type];
    void* raw[4] = {};
    if (!find_parts(type, arc, raw)) return false;
    u32 size[4] = {};
    u32 total = 0;
    for (int i = 0; i < 4; i++) {
        if (raw[i] == nullptr) continue;
        size[i] = res_size(arc, raw[i]);
        if (size[i] < 0x20) {
            if (i < 2) return false;
            raw[i] = nullptr;
            continue;
        }
        total += (size[i] + 0x1F) & ~0x1Fu;
    }

    const u32 heapSize = total * 3 + kHeapHeadroom;
    JKRHeap* root = JKRHeap::getRootHeap();
    if (root == nullptr || static_cast<u32>(root->getFreeSize()) < heapSize + kRootHeapReserve) {
        fx_log("zora_fx: not enough memory for %s (%u)", type_name(type), heapSize);
        return false;
    }
    JKRExpHeap* heap = JKRExpHeap::create(heapSize, root, false);
    if (heap == nullptr) return false;

    const Mode mode = type == ZORA_FX_ZORA ? kModeBelow : kModeAbove;
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(heap);
    set.body = build_model(heap, raw[0], size[0], mode);
    set.hat = set.body != nullptr ? build_model(heap, raw[1], size[1], mode) : nullptr;
    set.face = set.hat != nullptr && raw[2] != nullptr ? build_model(heap, raw[2], size[2], kModePlain) : nullptr;
    set.hands = set.hat != nullptr && raw[3] != nullptr ? build_model(heap, raw[3], size[3], kModePlain) : nullptr;
    mDoExt_setCurrentHeap(oldHeap);

    const bool ok = set.body != nullptr && set.hat != nullptr && (raw[2] == nullptr || set.face != nullptr) &&
                    (raw[3] == nullptr || set.hands != nullptr);
    if (!ok) {
        fx_log("zora_fx: build failed for %s (body=%d hat=%d face=%d hands=%d)", type_name(type),
               set.body != nullptr, set.hat != nullptr, set.face != nullptr, set.hands != nullptr);
        clear_models(set);
        mDoExt_destroyExpHeap(heap);
        return false;
    }
    set.heap = heap;
    apply_initial_visibility(set);
    fx_log("zora_fx: loaded %s ghosts, heap %d/%u", type_name(type), static_cast<int>(heap->getTotalUsedSize()),
           heapSize);
    return true;
}

void load_resource_set(int type, const char* path) {
    GhostSet& set = s_sets[type];
    set.state = LoadState::Failed;
    if (svc_resource == nullptr || mod_ctx == nullptr) return;
    ResourceBuffer buf = RESOURCE_BUFFER_INIT;
    if (svc_resource->load(mod_ctx, path, &buf) != MOD_OK || buf.data == nullptr) {
        fx_log("zora_fx: could not load %s", path);
        return;
    }
    JKRArchive* arc = JKRArchive::mount(buf.data, JKRHeap::getRootHeap(), JKRArchive::MOUNT_DIRECTION_HEAD);
    const bool ok = arc != nullptr && build_set(type, arc);
    if (arc != nullptr) arc->unmount();
    svc_resource->free(mod_ctx, &buf);
    set.state = ok ? LoadState::Ready : LoadState::Failed;
}

void start_load(int type) {
    GhostSet& set = s_sets[type];
    JKRHeap* root = JKRHeap::getRootHeap();
    if (!build_water_texture() || root == nullptr) {
        set.state = LoadState::Failed;
        return;
    }
    const char* path = type < kNativeCount ? kArcPath[type] : nullptr;
    if (const CustomEquipDef* def = custom_def(type)) {
        if (def->modelArc[0] != '/') {
            load_resource_set(type, def->modelArc);
            return;
        }
        path = def->modelArc;
    }
    set.cmd = path != nullptr ? mDoDvdThd_mountArchive_c::create(path, 0, root) : nullptr;
    set.state = set.cmd != nullptr ? LoadState::Mounting : LoadState::Failed;
}

void poll_load(int type) {
    GhostSet& set = s_sets[type];
    if (set.cmd == nullptr) {
        set.state = LoadState::Failed;
        return;
    }
    if (!set.cmd->sync()) return;
    JKRArchive* arc = set.cmd->getArchive();
    set.cmd->destroy();
    set.cmd = nullptr;
    const bool ok = arc != nullptr && build_set(type, arc);
    if (arc != nullptr) arc->unmount();
    set.state = ok ? LoadState::Ready : LoadState::Failed;
    if (arc == nullptr) fx_log("zora_fx: mount failed for %s", type_name(type));
}

J3DModel* face_model(int type) {
    if (s_sets[type].face != nullptr) return s_sets[type].face;
    const int base = base_type(type);
    return base != type && s_sets[base].state == LoadState::Ready ? s_sets[base].face : nullptr;
}

J3DModel* hands_model(int type) {
    if (s_sets[type].hands != nullptr) return s_sets[type].hands;
    const int base = base_type(type);
    return base != type && s_sets[base].state == LoadState::Ready ? s_sets[base].hands : nullptr;
}

void pump_loading() {
    for (int t = 0; t < kTypeCount; t++) {
        if (s_sets[t].state == LoadState::Mounting) {
            poll_load(t);
            return;
        }
    }
    for (int t = 0; t < kTypeCount; t++) {
        if (s_sets[t].wanted && s_sets[t].state == LoadState::Idle) {
            start_load(t);
            return;
        }
    }
}

bool set_ready(int type) {
    return valid_type(type) && s_sets[type].state == LoadState::Ready;
}

int real_models_type(daAlink_c* link) {
    if (link->getClothesChangeWaitTimer() != 0) return ZORA_FX_NONE;
    if (link->mProcID == daAlink_c::PROC_METAMORPHOSE || link->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY) {
        return ZORA_FX_NONE;
    }
    return zora_armor_fx_current_type(link);
}

u16 copy_pose(J3DModel* model, Mtx* dst) {
    J3DModelData* data = model->getModelData();
    if (data == nullptr) return 0;
    const u16 num = std::min<u16>(data->getJointNum(), kMaxJoints);
    for (u16 i = 0; i < num; i++) {
        mDoMtx_copy(model->getAnmMtx(i), dst[i]);
    }
    return num;
}

void snapshot_pose(daAlink_c* link) {
    if (link->mpLinkModel == nullptr || link->mpLinkHatModel == nullptr || link->mpLinkFaceModel == nullptr ||
        link->mpLinkHandModel == nullptr) {
        s_pose.valid = false;
        return;
    }
    mDoMtx_copy(link->mpLinkModel->getBaseTRMtx(), s_pose.base);
    s_pose.bodyNum = copy_pose(link->mpLinkModel, s_pose.body);
    s_pose.hatNum = copy_pose(link->mpLinkHatModel, s_pose.hat);
    s_pose.faceNum = copy_pose(link->mpLinkFaceModel, s_pose.face);
    s_pose.handsNum = copy_pose(link->mpLinkHandModel, s_pose.hands);
    s_pose.valid = s_pose.bodyNum > kHeadJoint;
    s_pose.linkId = fopAcM_GetID(link);
    s_pose.stamp = g_Counter.mCounter0;
}

bool is_hidden(J3DMaterial* material) {
    J3DShape* shape = material != nullptr ? material->getShape() : nullptr;
    return shape != nullptr && shape->checkFlag(J3DShpFlag_Visible);
}

void set_hidden(J3DMaterial* material, bool hidden) {
    J3DShape* shape = material != nullptr ? material->getShape() : nullptr;
    if (shape == nullptr) return;
    if (hidden) {
        shape->hide();
    } else {
        shape->show();
    }
}

void copy_tev_colors(J3DMaterial* dst, J3DMaterial* src) {
    J3DTevBlock* d = dst != nullptr ? dst->getTevBlock() : nullptr;
    J3DTevBlock* s = src != nullptr ? src->getTevBlock() : nullptr;
    if (d == nullptr || s == nullptr) return;
    for (u32 i = 0; i < 4; i++) {
        J3DGXColorS10* color = s->getTevColor(i);
        if (color != nullptr && d->getTevColor(i) != nullptr) d->setTevColor(i, color);
        J3DGXColor* konst = s->getTevKColor(i);
        if (konst != nullptr && d->getTevKColor(i) != nullptr) d->setTevKColor(i, konst);
    }
}

bool suffix_equal(const char* a, const char* b) {
    const char* sa = std::strchr(a, '_');
    const char* sb = std::strchr(b, '_');
    return sa != nullptr && sb != nullptr && std::strcmp(sa, sb) == 0;
}

bool unmatched_hidden(const char* name, daAlink_c* link) {
    if (name == nullptr) return false;
    if (always_hidden_name(name)) return true;
    if (std::strstr(name, "boots") != nullptr) return link->checkEquipHeavyBoots() != 0;
    return false;
}

void sync_model(J3DModel* ghost, J3DModel* real, bool sameArc, daAlink_c* link) {
    if (ghost == nullptr || real == nullptr) return;
    J3DModelData* g = ghost->getModelData();
    J3DModelData* r = real->getModelData();
    if (g == nullptr || r == nullptr) return;
    const u16 gNum = g->getMaterialNum();
    const u16 rNum = r->getMaterialNum();
    if (sameArc && gNum == rNum) {
        for (u16 i = 0; i < gNum; i++) {
            J3DMaterial* gm = g->getMaterialNodePointer(i);
            J3DMaterial* rm = r->getMaterialNodePointer(i);
            set_hidden(gm, is_hidden(rm));
            copy_tev_colors(gm, rm);
        }
        return;
    }
    JUTNameTab* gNames = g->getMaterialName();
    JUTNameTab* rNames = r->getMaterialName();
    if (gNames == nullptr || rNames == nullptr) return;
    for (u16 i = 0; i < gNum; i++) {
        const char* name = gNames->getName(i);
        if (name == nullptr) continue;
        int match = -1;
        for (u16 j = 0; j < rNum && match < 0; j++) {
            const char* other = rNames->getName(j);
            if (other != nullptr && std::strcmp(name, other) == 0) match = j;
        }
        for (u16 j = 0; j < rNum && match < 0; j++) {
            const char* other = rNames->getName(j);
            if (other != nullptr && suffix_equal(name, other)) match = j;
        }
        J3DMaterial* gm = g->getMaterialNodePointer(i);
        set_hidden(gm, match >= 0 ? is_hidden(r->getMaterialNodePointer(match)) : unmatched_hidden(name, link));
    }
}

void sync_ghosts(daAlink_c* link, int realType) {
    for (int t = 0; t < kTypeCount; t++) {
        GhostSet& set = s_sets[t];
        if (set.state != LoadState::Ready) continue;
        const bool same = t == realType;
        sync_model(set.body, link->mpLinkModel, same, link);
        sync_model(set.hat, link->mpLinkHatModel, same, link);
        sync_model(set.hands, link->mpLinkHandModel, same, link);
    }
}

int ghost_joint_callback(J3DJoint* joint, int timing) {
    if (timing != 0 || s_cbModel == nullptr || s_cbSrc == nullptr) return 1;
    const u16 jnt = joint->getJntNo();
    if (jnt >= s_cbCount) return 1;
    s_cbModel->setAnmMtx(jnt, s_cbSrc[jnt]);
    mDoMtx_copy(s_cbSrc[jnt], J3DSys::mCurrentMtx);
    return 1;
}

void draw_ghost(daAlink_c* link, J3DModel* model, Mtx base, Mtx* pose, u16 poseNum) {
    if (model == nullptr) return;
    J3DModelData* data = model->getModelData();
    const u16 jointNum = data->getJointNum();
    model->setBaseTRMtx(base);
    s_cbModel = model;
    s_cbSrc = pose;
    s_cbCount = std::min(jointNum, poseNum);
    for (u16 j = 0; j < jointNum; j++) {
        data->getJointNodePointer(j)->setCallBack(ghost_joint_callback);
    }
    model->calc();
    for (u16 j = 0; j < jointNum; j++) {
        data->getJointNodePointer(j)->setCallBack(nullptr);
    }
    s_cbModel = nullptr;
    s_cbSrc = nullptr;
    s_cbCount = 0;
    g_env_light.setLightTevColorType_MAJI(model, &link->tevStr);
    mDoExt_modelEntryDL(model);
}

void apply_dissolve(J3DModel* model, MtxP stack, f32 tx, f32 ty) {
    if (model == nullptr) return;
    J3DMaterial* material = model->getModelData()->getMaterialNodePointer(0);
    J3DTexGenBlock* texGen = material != nullptr ? material->getTexGenBlock() : nullptr;
    if (texGen == nullptr || texGen->getTexGenNum() == 0) return;
    J3DTexMtx* texMtx = texGen->getTexMtx(texGen->getTexGenNum() - 1);
    if (texMtx == nullptr) return;
    J3DTexMtxInfo& info = texMtx->getTexMtxInfo();
    info.mSRT.mTranslationX = tx;
    info.mSRT.mTranslationY = ty;
    cMtx_concat(kEffectBase, stack, info.mEffectMtx);
}

void line_range(f32& lo, f32& hi) {
    lo = 1.0e9f;
    hi = -1.0e9f;
    for (u16 i = 0; i < s_pose.bodyNum; i++) {
        lo = std::min(lo, s_pose.body[i][1][3]);
        hi = std::max(hi, s_pose.body[i][1][3]);
    }
    for (u16 i = 0; i < s_pose.hatNum; i++) {
        hi = std::max(hi, s_pose.hat[i][1][3]);
    }
    if (lo > hi) {
        lo = hi = s_pose.base[1][3];
    }
    lo -= kLineBelowFeet;
    hi += kLineAboveHead;
}

f32 waterline_height() {
    f32 lo = 0.0f;
    f32 hi = 0.0f;
    line_range(lo, hi);
    f32 p = static_cast<f32>(s_fx.tick) / static_cast<f32>(kDuration);
    p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    const f32 e = 0.5f * p + 0.25f * (1.0f - std::cos(3.14159265f * p));
    return s_fx.rising ? lo + (hi - lo) * e : hi + (lo - hi) * e;
}

void draw_equipment(daAlink_c* link) {
    if (link->checkSwordDraw() && !daPy_py_c::checkWoodSwordEquip()) {
        if (link->mSwordModel != nullptr && !link->checkNoResetFlg3(daPy_py_c::FLG3_UNK_80000000)) {
            link->modelDraw(link->mSwordModel, 0);
        }
        if (link->mSheathModel != nullptr) link->modelDraw(link->mSheathModel, 0);
    }
    if (link->checkShieldDraw() && link->mShieldModel != nullptr) {
        link->modelDraw(link->mShieldModel, 0);
    }
}

void draw_effect(daAlink_c* link) {
    GhostSet& from = s_sets[s_fx.fromType];
    GhostSet& to = s_sets[s_fx.toType];

    const cXyz& pos = link->current.pos;
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    const s16 yaw = camera != nullptr ? fopCamM_GetAngleY(camera) : link->shape_angle.y;
    mDoMtx_stack_c::YrotS(static_cast<s16>(-yaw));
    mDoMtx_stack_c::transM(-pos.x, -pos.y, -pos.z);
    const f32 ty = 0.025f * (1.0f + (waterline_height() - pos.y));
    for (J3DModel* model : {from.body, from.hat, to.body, to.hat}) {
        apply_dissolve(model, mDoMtx_stack_c::get(), s_fx.scroll, ty);
    }

    Mtx head;
    mDoMtx_copy(s_pose.body[kHeadJoint], head);

    draw_ghost(link, from.body, s_pose.base, s_pose.body, s_pose.bodyNum);
    draw_ghost(link, from.hat, head, s_pose.hat, s_pose.hatNum);
    draw_ghost(link, to.body, s_pose.base, s_pose.body, s_pose.bodyNum);
    draw_ghost(link, to.hat, head, s_pose.hat, s_pose.hatNum);

    if (link->getClothesChangeWaitTimer() != 0) {
        draw_ghost(link, face_model(s_fx.fromType), head, s_pose.face, s_pose.faceNum);
        draw_ghost(link, hands_model(s_fx.fromType), s_pose.base, s_pose.hands, s_pose.handsNum);
        draw_equipment(link);
    }
}

bool pose_fits(int type) {
    return set_ready(type) && s_sets[type].body->getModelData()->getJointNum() == s_pose.bodyNum;
}

bool effect_drawable(daAlink_c* link) {
    const fpc_ProcID id = fopAcM_GetID(link);
    if (!s_fx.active || s_fx.waitingSwap || id != s_fx.linkId || !s_pose.valid || s_pose.linkId != id) return false;
    if (!pose_fits(s_fx.fromType) || !pose_fits(s_fx.toType)) return false;
    if (dComIfGp_checkCameraAttentionStatus(link->field_0x317c, 0x20)) return false;
    return link->checkPlayerNoDraw() == 0;
}

void end_effect(const char* reason) {
    if (s_fx.active) fx_log("zora_fx: %s after %d ticks", reason, s_fx.tick);
    s_fx = Effect{};
    s_drawGhosts = false;
}

void tick_effect(daAlink_c* link) {
    if (!s_fx.active) return;
    if (fopAcM_GetID(link) != s_fx.linkId || link->checkWolf()) {
        end_effect("cancel");
        return;
    }
    if (s_fx.waitingSwap) {
        const int realType = real_models_type(link);
        if (realType == s_fx.toType || link->getClothesChangeWaitTimer() != 0) {
            s_fx.waitingSwap = false;
            s_fx.tick = 0;
            s_fx.waitTicks = 0;
            return;
        }
        if (realType != s_fx.fromType || ++s_fx.waitTicks > kMaxSwapWaitTicks) end_effect("cancel");
        return;
    }
    s_fx.tick++;
    s_fx.scroll += kScrollSpeed;
    if (s_fx.scroll >= 1.0f) s_fx.scroll -= 1.0f;
    if (link->getClothesChangeWaitTimer() != 0) {
        if (++s_fx.waitTicks > kMaxWaitTicks) end_effect("timeout");
        return;
    }
    if (real_models_type(link) != s_fx.toType) {
        end_effect("cancel");
        return;
    }
    if (s_fx.tick >= kDuration) end_effect("done");
}

daAlink_c* player_arg(void* args) {
    if (args == nullptr) return nullptr;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    return link != nullptr && link == daAlink_getAlinkActorClass() ? link : nullptr;
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    daAlink_c* link = player_arg(args);
    if (link == nullptr) return;
    pump_loading();
    tick_effect(link);
}

HookAction on_draw_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = player_arg(args);
    s_drawGhosts = link != nullptr && effect_drawable(link);
    return HOOK_CONTINUE;
}

void on_draw_post(ModContext*, void* args, void*, void*) {
    const bool drawGhosts = s_drawGhosts;
    s_drawGhosts = false;
    daAlink_c* link = player_arg(args);
    if (link == nullptr || (!g_configAutoZoraArmor && !s_fx.active)) return;
    const int realType = real_models_type(link);
    if (realType != ZORA_FX_NONE) {
        snapshot_pose(link);
        sync_ghosts(link, realType);
    }
    if (drawGhosts && s_fx.active) draw_effect(link);
}

HookAction on_model_draw_pre(ModContext*, void* args, void*, void*) {
    if (!s_drawGhosts || args == nullptr) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    J3DModel* model = mods::arg<J3DModel*>(args, 1);
    if (link == nullptr || model == nullptr || link != daAlink_getAlinkActorClass()) return HOOK_CONTINUE;
    if (model == link->mpLinkModel || model == link->mpLinkHatModel) return HOOK_SKIP_ORIGINAL;
    return HOOK_CONTINUE;
}

}

ModResult init_zora_armor_fx(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    ModResult result = mods::hook::add_post<ZoraFxExecuteHook>(hook_svc, on_execute_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<ZoraFxDrawHook>(hook_svc, on_draw_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<ZoraFxDrawHook>(hook_svc, on_draw_post);
    if (result != MOD_OK) return result;
    return mods::hook::add_pre<ZoraFxModelDrawHook>(hook_svc, on_model_draw_pre);
}

int zora_armor_fx_current_type(daAlink_c* link) {
    if (link == nullptr || link->checkWolf()) return ZORA_FX_NONE;
    if (custom_equip_active(CE_TUNIC)) {
        const int type = kNativeCount + custom_equip_active_id(CE_TUNIC);
        return custom_def(type) != nullptr ? type : ZORA_FX_NONE;
    }
    if (link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000)) return ZORA_FX_NONE;
    return native_type_for_item(dComIfGs_getSelectEquipClothes());
}

void zora_armor_fx_preload(int type) {
    if (!valid_type(type)) return;
    s_sets[type].wanted = true;
    s_sets[base_type(type)].wanted = true;
}

bool zora_armor_fx_begin(daAlink_c* link, int fromType, int toType) {
    if (link == nullptr || !valid_type(fromType) || !valid_type(toType) || fromType == toType) return false;
    if ((fromType == ZORA_FX_ZORA) == (toType == ZORA_FX_ZORA)) return false;
    if (!set_ready(fromType) || !set_ready(toType)) {
        fx_log("zora_fx: skip %s -> %s, ghosts not loaded", type_name(fromType), type_name(toType));
        return false;
    }
    if (!s_pose.valid || s_pose.linkId != fopAcM_GetID(link) || g_Counter.mCounter0 - s_pose.stamp > kMaxPoseAge ||
        !pose_fits(fromType) || !pose_fits(toType)) {
        fx_log("zora_fx: skip %s -> %s, no usable pose", type_name(fromType), type_name(toType));
        return false;
    }
    s_fx = Effect{};
    s_fx.active = true;
    s_fx.linkId = fopAcM_GetID(link);
    s_fx.fromType = fromType;
    s_fx.toType = toType;
    s_fx.rising = toType == ZORA_FX_ZORA;
    s_fx.waitingSwap = true;
    fx_log("zora_fx: begin %s -> %s", type_name(fromType), type_name(toType));
    return true;
}

void update_zora_armor_fx() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    const fpc_ProcID id = link != nullptr ? fopAcM_GetID(link) : fpcM_ERROR_PROCESS_ID_e;
    if (s_fx.active && id != s_fx.linkId) end_effect("cancel");
    if (s_pose.valid && id != s_pose.linkId) s_pose.valid = false;
}

void shutdown_zora_armor_fx() {
    s_fx = Effect{};
    s_pose.valid = false;
    s_drawGhosts = false;
    for (GhostSet& set : s_sets) {
        if (set.cmd != nullptr && set.cmd->sync()) {
            JKRArchive* arc = set.cmd->getArchive();
            set.cmd->destroy();
            if (arc != nullptr) arc->unmount();
        }
        if (set.heap != nullptr) mDoExt_destroyExpHeap(set.heap);
        set = GhostSet{};
    }
    if (s_texBuf != nullptr) {
        JKRHeap* root = JKRHeap::getRootHeap();
        if (root != nullptr) root->free(s_texBuf);
    }
    s_texBuf = nullptr;
    s_waterTex = nullptr;
    s_texFailed = false;
}
