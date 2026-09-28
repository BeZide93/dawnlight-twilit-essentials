#include "boss_rush_darklink.hpp"

#include "../util.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_resorce.h"
#include "d/d_stage.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_b_tn.h"
#include "mods/hook.hpp"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphLoader/J3DAnmLoader.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "mods/svc/log.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

extern const LogService* svc_log;

namespace {

constexpr const char* kDarkLinkModId = "hopex.dark_link";
constexpr const char* kKmdlPath = "/res/Object/Kmdl.arc";
constexpr const char* kAlinkPath = "/res/Object/Alink.arc";
constexpr const char* kShieldPath = "/res/Object/HyShd.arc";
constexpr u32 kTagBmwr = 'BMWR';
constexpr u32 kTagBmwe = 'BMWE';
constexpr u32 kMasterSwordIndex = 0x38;
constexpr u32 kMasterSheathIndex = 0x37;
constexpr u32 kShieldIndex = 0x3;
constexpr u32 kHeapSize = 0x600000;
constexpr u32 kRootHeapReserve = 0xC00000;
constexpr u32 kGameHeapReserve = 0x400000;
constexpr u32 kBattleWaitAnmIndex = 0x256;
constexpr u32 kIdleWaitAnmIndex = 0x26A;
constexpr u32 kLinkAnmBufferSize = 0x10800;
constexpr u16 kHeadJoint = 4;
constexpr u16 kSheathJoint = 5;
constexpr u16 kLeftHandJoint = 9;
constexpr u16 kSwordJoint = 10;
constexpr u16 kRightHandJoint = 0xE;
constexpr u16 kShieldJoint = 15;
constexpr u16 kLeftHandMaterial = 1;
constexpr u16 kRightHandMaterial = 6;

constexpr f32 kGreyBase = 30.0f;
constexpr f32 kGreyRange = 45.0f;
constexpr f32 kEyeBase = 175.0f;
constexpr f32 kEyeRange = 70.0f;
constexpr f32 kIntensityScale = 0.35f;
constexpr u8 kGhostAlpha = 175;
constexpr u8 kShadowGrey = 18;

enum class State { Idle, Mounting, Ready, Failed };
enum class Recolor { Dark, Eye, Keep };

State s_state = State::Idle;
JKRExpHeap* s_heap = nullptr;
mDoDvdThd_mountArchive_c* s_mountCmd = nullptr;
JKRArchive* s_archive = nullptr;
JKRArchive* s_alinkArchive = nullptr;
JKRArchive* s_shieldArchive = nullptr;
J3DModel* s_body = nullptr;
J3DModel* s_head = nullptr;
J3DModel* s_face = nullptr;
J3DModel* s_hands = nullptr;
J3DModel* s_sword = nullptr;
J3DModel* s_sheath = nullptr;
J3DModel* s_shield = nullptr;
mDoExt_bckAnm* s_bck = nullptr;
dKy_tevstr_c s_darkTev;
std::vector<const void*> s_recolored;

void dl_log(const char* fmt, ...) {
    if (svc_log == nullptr || svc_log->info == nullptr) return;
    char msg[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    svc_log->info(mod_ctx, msg);
}

JKRHeap* pick_parent_heap() {
    JKRHeap* root = JKRHeap::getRootHeap();
    const u32 rootFree = root != nullptr ? static_cast<u32>(root->getFreeSize()) : 0;
    if (rootFree > kHeapSize + kRootHeapReserve) return root;
    JKRHeap* game = mDoExt_getGameHeap();
    const u32 gameFree = game != nullptr ? static_cast<u32>(game->getFreeSize()) : 0;
    if (gameFree > kHeapSize + kGameHeapReserve) return game;
    dl_log("[darklink] not enough memory (root free %u, game free %u)", rootFree, gameFree);
    return nullptr;
}

u8 clamp_u8(f32 v) {
    return static_cast<u8>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
}

void recolor_rgb(u8& r, u8& g, u8& b, Recolor mode) {
    const f32 l = (0.30f * r + 0.59f * g + 0.11f * b) / 255.0f;
    if (mode == Recolor::Eye) {
        r = g = b = clamp_u8(kEyeBase + l * kEyeRange);
        return;
    }
    const u8 grey = clamp_u8(kGreyBase + l * kGreyRange);
    r = grey;
    g = grey;
    b = static_cast<u8>(grey + 2 > 255 ? 255 : grey + 2);
}

u8 recolor_intensity(u8 i, Recolor mode) {
    if (mode == Recolor::Eye) return clamp_u8(kEyeBase + i / 255.0f * kEyeRange);
    return clamp_u8(i * kIntensityScale);
}

u16 read_be16(const u8* p) { return static_cast<u16>((p[0] << 8) | p[1]); }

void write_be16(u8* p, u16 v) {
    p[0] = static_cast<u8>(v >> 8);
    p[1] = static_cast<u8>(v & 0xFF);
}

u16 recolor_565(u16 c, Recolor mode) {
    u8 r = static_cast<u8>(((c >> 11) & 0x1F) * 255 / 31);
    u8 g = static_cast<u8>(((c >> 5) & 0x3F) * 255 / 63);
    u8 b = static_cast<u8>((c & 0x1F) * 255 / 31);
    recolor_rgb(r, g, b, mode);
    return static_cast<u16>(((r * 31 / 255) << 11) | ((g * 63 / 255) << 5) | (b * 31 / 255));
}

u16 recolor_5a3(u16 c, Recolor mode) {
    if (c & 0x8000) {
        u8 r = static_cast<u8>(((c >> 10) & 0x1F) * 255 / 31);
        u8 g = static_cast<u8>(((c >> 5) & 0x1F) * 255 / 31);
        u8 b = static_cast<u8>((c & 0x1F) * 255 / 31);
        recolor_rgb(r, g, b, mode);
        return static_cast<u16>(0x8000 | ((r * 31 / 255) << 10) | ((g * 31 / 255) << 5) | (b * 31 / 255));
    }
    u8 r = static_cast<u8>(((c >> 8) & 0xF) * 17);
    u8 g = static_cast<u8>(((c >> 4) & 0xF) * 17);
    u8 b = static_cast<u8>((c & 0xF) * 17);
    recolor_rgb(r, g, b, mode);
    return static_cast<u16>((c & 0x7000) | ((r / 17) << 8) | ((g / 17) << 4) | (b / 17));
}

u16 recolor_ia8(u16 c, Recolor mode) {
    return static_cast<u16>((c & 0xFF00) | recolor_intensity(static_cast<u8>(c & 0xFF), mode));
}

void recolor_cmpr_block(u8* block, Recolor mode) {
    const u16 c0 = read_be16(block);
    const u16 c1 = read_be16(block + 2);
    u16 n0 = recolor_565(c0, mode);
    u16 n1 = recolor_565(c1, mode);
    const bool opaque = c0 > c1;
    if (opaque) {
        if (n0 < n1) {
            std::swap(n0, n1);
            for (int i = 4; i < 8; i++) block[i] ^= 0x55;
        } else if (n0 == n1) {
            if (n0 < 0xFFFF) {
                n0++;
            } else {
                n1--;
            }
        }
    } else if (n0 > n1) {
        std::swap(n0, n1);
        for (int i = 4; i < 8; i++) {
            u8 v = block[i];
            u8 out = 0;
            for (int s = 0; s < 8; s += 2) {
                u8 idx = (v >> s) & 3;
                if (idx < 2) idx ^= 1;
                out |= static_cast<u8>(idx << s);
            }
            block[i] = out;
        }
    }
    write_be16(block, n0);
    write_be16(block + 2, n1);
}

u32 level_size(u8 format, u32 w, u32 h) {
    u32 bw = 4, bh = 4, bpp = 16;
    switch (format) {
    case GX_TF_I4: case GX_TF_C4: case GX_TF_CMPR: bw = 8; bh = 8; bpp = 4; break;
    case GX_TF_I8: case GX_TF_IA4: case GX_TF_C8: bw = 8; bh = 4; bpp = 8; break;
    case GX_TF_RGBA8: bpp = 32; break;
    default: break;
    }
    const u32 tilesX = (w + bw - 1) / bw;
    const u32 tilesY = (h + bh - 1) / bh;
    return tilesX * tilesY * bw * bh * bpp / 8;
}

void recolor_pixels(u8* data, u8 format, u32 size, Recolor mode) {
    switch (format) {
    case GX_TF_CMPR:
        for (u32 i = 0; i + 8 <= size; i += 8) recolor_cmpr_block(data + i, mode);
        break;
    case GX_TF_RGB565:
        for (u32 i = 0; i + 2 <= size; i += 2) write_be16(data + i, recolor_565(read_be16(data + i), mode));
        break;
    case GX_TF_RGB5A3:
        for (u32 i = 0; i + 2 <= size; i += 2) write_be16(data + i, recolor_5a3(read_be16(data + i), mode));
        break;
    case GX_TF_IA8:
        for (u32 i = 0; i + 2 <= size; i += 2) write_be16(data + i, recolor_ia8(read_be16(data + i), mode));
        break;
    case GX_TF_I8:
        for (u32 i = 0; i < size; i++) data[i] = recolor_intensity(data[i], mode);
        break;
    case GX_TF_IA4:
        for (u32 i = 0; i < size; i++) {
            const u8 intensity = static_cast<u8>((data[i] & 0xF) * 17);
            data[i] = static_cast<u8>((data[i] & 0xF0) | (recolor_intensity(intensity, mode) / 17));
        }
        break;
    case GX_TF_I4:
        for (u32 i = 0; i < size; i++) {
            const u8 hi = recolor_intensity(static_cast<u8>((data[i] >> 4) * 17), mode) / 17;
            const u8 lo = recolor_intensity(static_cast<u8>((data[i] & 0xF) * 17), mode) / 17;
            data[i] = static_cast<u8>((hi << 4) | lo);
        }
        break;
    case GX_TF_RGBA8:
        for (u32 t = 0; t + 64 <= size; t += 64) {
            u8* tile = data + t;
            for (int p = 0; p < 16; p++) {
                u8 r = tile[p * 2 + 1];
                u8 g = tile[32 + p * 2];
                u8 b = tile[32 + p * 2 + 1];
                recolor_rgb(r, g, b, mode);
                tile[p * 2 + 1] = r;
                tile[32 + p * 2] = g;
                tile[32 + p * 2 + 1] = b;
            }
        }
        break;
    default:
        break;
    }
}

void recolor_palette(u8* palette, u8 tlutFormat, u16 count, Recolor mode) {
    for (u16 i = 0; i < count; i++) {
        u8* p = palette + i * 2;
        const u16 c = read_be16(p);
        if (tlutFormat == GX_TL_RGB565) {
            write_be16(p, recolor_565(c, mode));
        } else if (tlutFormat == GX_TL_RGB5A3) {
            write_be16(p, recolor_5a3(c, mode));
        } else {
            write_be16(p, recolor_ia8(c, mode));
        }
    }
}

bool already_recolored(const void* ptr) {
    if (std::find(s_recolored.begin(), s_recolored.end(), ptr) != s_recolored.end()) return true;
    s_recolored.push_back(ptr);
    return false;
}

Recolor texture_mode(const char* name) {
    if (name == nullptr) return Recolor::Dark;
    if (std::strcmp(name, "al_eyeball") == 0) return Recolor::Eye;
    if (std::strcmp(name, "highlight02") == 0) return Recolor::Keep;
    return Recolor::Dark;
}

int s_recolorDone = 0;
int s_recolorSkipped = 0;

u32 read_be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);
}

bool is_eye_texture(const char* name) {
    return name != nullptr &&
           (std::strcmp(name, "al_eyeball") == 0 || std::strcmp(name, "highlight02") == 0 ||
            std::strcmp(name, "eye_kage01") == 0);
}

const char* tex1_name(const u8* names, const u8* end, u16 index) {
    if (names == nullptr || names + 4 > end) return nullptr;
    if (index >= read_be16(names)) return nullptr;
    const u8* entry = names + 4 + 4 * index;
    if (entry + 4 > end) return nullptr;
    const u8* name = names + read_be16(entry + 2);
    if (name >= end) return nullptr;
    for (const u8* c = name; c < end; c++) {
        if (*c == 0) return reinterpret_cast<const char*>(name);
    }
    return nullptr;
}

void recolor_tex1(u8* file, const u8* end, u32 tex1) {
    const u8* section = file + tex1;
    if (section + 20 > end) return;
    const u16 count = read_be16(section + 8);
    const u32 headerOffset = read_be32(section + 12);
    const u32 nameOffset = read_be32(section + 16);
    const u8* names = nameOffset != 0 ? section + nameOffset : nullptr;
    for (u16 i = 0; i < count; i++) {
        u8* header = file + tex1 + headerOffset + 32u * i;
        if (header + 32 > end) break;
        const char* name = tex1_name(names, end, i);
        if (is_eye_texture(name)) header[0x17] = 0;
        const Recolor mode = texture_mode(name);
        if (mode == Recolor::Keep) continue;

        const u8 format = header[0];
        const u16 width = read_be16(header + 2);
        const u16 height = read_be16(header + 4);
        const u8 indexed = header[8];
        const u8 tlutFormat = header[9];
        const u16 numColors = read_be16(header + 0xA);
        const s32 paletteOffset = static_cast<s32>(read_be32(header + 0xC));
        const u8 mipCount = header[0x18];
        const s32 imageOffset = static_cast<s32>(read_be32(header + 0x1C));

        if (indexed != 0 && numColors != 0) {
            u8* palette = header + paletteOffset;
            if (palette < file || palette + numColors * 2u > end) {
                s_recolorSkipped++;
                continue;
            }
            if (!already_recolored(palette)) recolor_palette(palette, tlutFormat, numColors, mode);
            s_recolorDone++;
            continue;
        }

        u8* pixels = header + imageOffset;
        if (pixels < file || pixels >= end) {
            s_recolorSkipped++;
            continue;
        }
        if (already_recolored(pixels)) continue;
        u32 w = width;
        u32 h = height;
        u32 size = 0;
        const u32 levels = mipCount > 0 ? mipCount : 1;
        for (u32 level = 0; level < levels && w > 0 && h > 0; level++) {
            size += level_size(format, w, h);
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
        }
        if (pixels + size > end) size = static_cast<u32>(end - pixels);
        recolor_pixels(pixels, format, size, mode);
        s_recolorDone++;
    }
}

int s_ghostPatched = 0;
int s_eyeRepointed = 0;

bool name_has_eye(const char* name) {
    if (name == nullptr) return false;
    for (const char* c = name; c[0] != 0 && c[1] != 0 && c[2] != 0; c++) {
        if ((c[0] == 'e' || c[0] == 'E') && (c[1] == 'y' || c[1] == 'Y') && (c[2] == 'e' || c[2] == 'E')) {
            return true;
        }
    }
    return false;
}

void patch_mat3_ghost(u8* file, const u8* end, u32 mat3) {
    u8* section = file + mat3;
    if (section + 12 + 30 * 4 > end) return;
    const u16 count = read_be16(section + 8);
    u32 table[30];
    for (int i = 0; i < 30; i++) table[i] = read_be32(section + 12 + 4 * i);
    if (table[0] == 0 || table[1] == 0 || table[7] == 0) return;
    u8* initData = section + table[0];
    const u8* remap = section + table[1];
    const u8* names = table[2] != 0 ? section + table[2] : nullptr;
    u8* chanTable = section + table[7];

    for (u16 m = 0; m < count; m++) {
        if (remap + 2 * m + 2 > end) break;
        u8* material = initData + 0x14C * read_be16(remap + 2 * m);
        if (material + 0x14C > end) continue;
        const char* name = tex1_name(names, end, m);
        const u16 chanAlpha0 = read_be16(material + 0x0E);
        if (!name_has_eye(name)) continue;
        if (chanAlpha0 != 0xFFFF && chanTable + 8u * chanAlpha0 + 8 <= end &&
            chanTable[8u * chanAlpha0] == 0) {
            material[0x0C] = static_cast<u8>(chanAlpha0 >> 8);
            material[0x0D] = static_cast<u8>(chanAlpha0 & 0xFF);
            s_eyeRepointed++;
        }
    }

    for (u16 m = 0; m < count; m++) {
        if (remap + 2 * m + 2 > end) break;
        u8* material = initData + 0x14C * read_be16(remap + 2 * m);
        if (material + 0x14C > end) continue;
        material[0] = 4;
        const u16 blendIdx = read_be16(material + 0x148);
        if (table[25] != 0 && blendIdx != 0xFFFF) {
            u8* blend = section + table[25] + 4u * blendIdx;
            if (blend + 4 <= end) {
                blend[0] = 1;
                blend[1] = 4;
                blend[2] = 5;
            }
        }
        const u8 kasel = material[0xAC];
        if (kasel >= 0x1D && kasel <= 0x1F && table[18] != 0) {
            const u16 konstIdx = read_be16(material + 0x94 + 2 * (kasel - 0x1C));
            u8* konst = section + table[18] + 4u * konstIdx;
            if (konstIdx != 0xFFFF && konst + 4 <= end) konst[3] = kGhostAlpha;
        }
        s_ghostPatched++;
    }
}

void recolor_bmd_file(u8* file, u32 size) {
    s_recolorDone = 0;
    s_recolorSkipped = 0;
    s_ghostPatched = 0;
    s_eyeRepointed = 0;
    if (size < 0x20 || std::memcmp(file, "J3D2", 4) != 0) return;
    const u8* end = file + size;
    u32 offset = 0x20;
    while (offset + 8 <= size) {
        const u32 sectionSize = read_be32(file + offset + 4);
        if (std::memcmp(file + offset, "MAT3", 4) == 0) {
            patch_mat3_ghost(file, end, offset);
        } else if (std::memcmp(file + offset, "TEX1", 4) == 0) {
            recolor_tex1(file, end, offset);
        }
        if (sectionSize == 0) return;
        offset += sectionSize;
    }
}

J3DModel* create_model_from_raw(JKRArchive* archive, void* raw, u32 tag, const char* label) {
    if (archive == nullptr || raw == nullptr) return nullptr;
    u32 size = archive->getExpandedResSize(raw);
    const u8* rawBytes = static_cast<const u8*>(raw);
    const u32 headerSize = (static_cast<u32>(rawBytes[8]) << 24) | (static_cast<u32>(rawBytes[9]) << 16) |
                           (static_cast<u32>(rawBytes[10]) << 8) | static_cast<u32>(rawBytes[11]);
    if (std::memcmp(rawBytes, "J3D2", 4) == 0 && headerSize > 0 && headerSize < 0x800000 &&
        (size == 0xFFFFFFFFu || headerSize > size)) {
        size = headerSize;
    }
    if (size == 0 || size == 0xFFFFFFFFu) {
        dl_log("[darklink] %s: bad resource size %u", label, size);
        return nullptr;
    }
    u8* copy = JKR_NEW_ARRAY_ARGS(u8, size, 0x20);
    if (copy == nullptr) {
        dl_log("[darklink] %s: no memory for %u bytes", label, size);
        return nullptr;
    }
    std::memcpy(copy, raw, size);
    recolor_bmd_file(copy, size);
    J3DModelData* data = dRes_info_c::loaderBasicBmd(tag, copy);
    if (data == nullptr || data->getMaterialNum() == 0 || data->getJointNum() == 0) {
        dl_log("[darklink] %s: model data invalid", label);
        return nullptr;
    }
    J3DModel* model = tag == kTagBmwe ? mDoExt_J3DModel__create(data, 0, 0x11000084)
                                      : mDoExt_J3DModel__create(data, 0x80000, 0x11000284);
    if (model == nullptr) {
        dl_log("[darklink] %s: model create failed", label);
        return nullptr;
    }
    dl_log("[darklink] %s: %u bytes, %d joints, %d materials, %d textures, recolored %d, skipped %d, ghost %d, eyes %d",
           label, size, static_cast<int>(data->getJointNum()),
           static_cast<int>(data->getMaterialNum()),
           data->getTexture() != nullptr ? static_cast<int>(data->getTexture()->getNum()) : 0,
           s_recolorDone, s_recolorSkipped, s_ghostPatched, s_eyeRepointed);
    return model;
}

J3DModel* create_kmdl_model(const char* bmdName) {
    return create_model_from_raw(s_archive, s_archive->getResource(kTagBmwr, bmdName), kTagBmwr, bmdName);
}

JKRArchive* mount_dvd_archive(const char* path) {
    return JKRArchive::mount(path, JKRArchive::MOUNT_DVD, s_heap, JKRArchive::MOUNT_DIRECTION_HEAD);
}

void setup_hand_shapes(J3DModel* hands) {
    if (hands == nullptr) return;
    J3DModelData* data = hands->getModelData();
    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DShape* shape = data->getMaterialNodePointer(i)->getShape();
        if (shape == nullptr) continue;
        if (i == kLeftHandMaterial || i == kRightHandMaterial) {
            shape->show();
        } else {
            shape->hide();
        }
    }
}

mDoExt_bckAnm* create_link_bck(u32 index) {
    JKRArchive* anmArchive = dComIfGp_getAnmArchive();
    if (anmArchive == nullptr) return nullptr;
    if (anmArchive->findIdxResource(index) == nullptr) return nullptr;
    u8* buffer = JKR_NEW_ARRAY_ARGS(u8, kLinkAnmBufferSize, 0x20);
    if (buffer == nullptr) return nullptr;
    const u32 read = JKRReadIdxResource(buffer, kLinkAnmBufferSize, index, anmArchive);
    if (read == 0 || read > kLinkAnmBufferSize || std::memcmp(buffer, "J3D1bck1", 8) != 0) {
        dl_log("[darklink] anim %x unusable (read %u)", index, read);
        return nullptr;
    }
    void* anm = J3DAnmLoaderDataBase::load(buffer);
    if (anm == nullptr) return nullptr;
    mDoExt_bckAnm* bck = JKR_NEW mDoExt_bckAnm();
    if (bck == nullptr) return nullptr;
    if (!bck->init(static_cast<J3DAnmTransform*>(anm), TRUE, 2, 1.0f, 0, -1, false)) {
        JKR_DELETE(bck);
        return nullptr;
    }
    return bck;
}

void release_all() {
    if (s_mountCmd != nullptr) return;
    if (s_bck != nullptr) {
        JKR_DELETE(s_bck);
        s_bck = nullptr;
    }
    for (J3DModel** model : {&s_shield, &s_sheath, &s_sword, &s_hands, &s_face, &s_head, &s_body}) {
        if (*model != nullptr) {
            JKR_DELETE(*model);
            *model = nullptr;
        }
    }
    for (JKRArchive** archive : {&s_shieldArchive, &s_alinkArchive, &s_archive}) {
        if (*archive != nullptr) {
            (*archive)->unmount();
            *archive = nullptr;
        }
    }
    if (s_heap != nullptr) {
        mDoExt_destroyExpHeap(s_heap);
        s_heap = nullptr;
    }
    s_recolored.clear();
}

bool build_models() {
    dl_log("[darklink] Kmdl mounted, heap free %u", static_cast<u32>(s_heap->getFreeSize()));
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(s_heap);
    s_body = create_kmdl_model("al.bmd");
    s_head = create_kmdl_model("al_head.bmd");
    s_face = create_kmdl_model("al_face.bmd");
    s_hands = create_kmdl_model("al_hands.bmd");
    s_alinkArchive = mount_dvd_archive(kAlinkPath);
    if (s_alinkArchive != nullptr) {
        s_sword = create_model_from_raw(s_alinkArchive, s_alinkArchive->getIdxResource(kMasterSwordIndex),
                                        kTagBmwe, "sword");
        s_sheath = create_model_from_raw(s_alinkArchive, s_alinkArchive->getIdxResource(kMasterSheathIndex),
                                         kTagBmwe, "sheath");
    }
    s_shieldArchive = mount_dvd_archive(kShieldPath);
    if (s_shieldArchive != nullptr) {
        s_shield = create_model_from_raw(s_shieldArchive, s_shieldArchive->getIdxResource(kShieldIndex),
                                         kTagBmwr, "shield");
    }
    s_bck = create_link_bck(kBattleWaitAnmIndex);
    if (s_bck == nullptr) s_bck = create_link_bck(kIdleWaitAnmIndex);
    mDoExt_setCurrentHeap(oldHeap);
    dl_log("[darklink] body=%d head=%d face=%d hands=%d alink=%d sword=%d sheath=%d shieldArc=%d shield=%d bck=%d heap free %u",
           s_body != nullptr, s_head != nullptr, s_face != nullptr, s_hands != nullptr,
           s_alinkArchive != nullptr, s_sword != nullptr, s_sheath != nullptr,
           s_shieldArchive != nullptr, s_shield != nullptr, s_bck != nullptr,
           static_cast<u32>(s_heap->getFreeSize()));

    if (s_body == nullptr || s_body->getModelData()->getJointNum() <= kShieldJoint) {
        return false;
    }
    setup_hand_shapes(s_hands);
    dl_log("[darklink] hand shapes set");
    return true;
}

bool update_loading() {
    switch (s_state) {
    case State::Ready:
        return true;
    case State::Failed:
        return false;
    case State::Idle: {
        JKRHeap* parent = pick_parent_heap();
        s_heap = parent != nullptr ? JKRExpHeap::create(kHeapSize, parent, false) : nullptr;
        if (s_heap == nullptr) {
            s_state = State::Failed;
            return false;
        }
        s_mountCmd = mDoDvdThd_mountArchive_c::create(kKmdlPath, 0, s_heap);
        dl_log("[darklink] heap created, mounting Kmdl (cmd=%d)", s_mountCmd != nullptr);
        if (s_mountCmd == nullptr) {
            release_all();
            s_state = State::Failed;
            return false;
        }
        s_state = State::Mounting;
        return false;
    }
    case State::Mounting:
        if (!s_mountCmd->sync()) return false;
        s_archive = s_mountCmd->getArchive();
        s_mountCmd->destroy();
        s_mountCmd = nullptr;
        s_state = State::Failed;
        if (s_archive == nullptr || !build_models()) {
            dl_log("[darklink] build failed (archive=%d)", s_archive != nullptr);
            release_all();
            s_state = State::Failed;
            return false;
        }
        s_state = State::Ready;
        return true;
    }
    return false;
}

void apply_dark_light(J3DModel* model) {
    daAlink_c* alink = daAlink_getAlinkActorClass();
    if (alink == nullptr) return;
    g_env_light.settingTevStruct_colget_player(&alink->tevStr);
    std::memcpy(static_cast<void*>(&s_darkTev), static_cast<const void*>(&alink->tevStr),
                sizeof(dKy_tevstr_c));
    s_darkTev.TevKColor.r = kShadowGrey;
    s_darkTev.TevKColor.g = kShadowGrey;
    s_darkTev.TevKColor.b = kShadowGrey;
    s_darkTev.TevKColor.a = kGhostAlpha;
    g_env_light.setLightTevColorType_MAJI(model, &s_darkTev);
}

void draw_at(J3DModel* model, MtxP mtx) {
    if (model == nullptr) return;
    model->setBaseTRMtx(mtx);
    apply_dark_light(model);
    mDoExt_modelUpdateDL(model);
}

}

bool g_configBossRushDarkLink = true;

bool boss_rush_darklink_mod_installed() {
    static int s_checkCountdown = 0;
    static bool s_installed = false;
    if (--s_checkCountdown <= 0) {
        s_checkCountdown = 120;
        s_installed = is_mod_enabled(kDarkLinkModId);
    }
    return s_installed;
}

bool boss_rush_darklink_enabled() {
#if USE_DARK_LINK
    return g_configBossRushDarkLink && boss_rush_darklink_mod_installed();
#else
    return false;
#endif
}

void boss_rush_darklink_draw(const cXyz& pos, const csXyz& angle) {
    static int s_loggedStep = 0;
    auto step = [](int id, const char* name) {
        if (id > s_loggedStep) {
            s_loggedStep = id;
            dl_log("[darklink] draw step %d: %s", id, name);
        }
    };

    step(1, "enter");
    if (!update_loading()) return;
    step(2, "loaded");

    mDoMtx_stack_c::transS(pos.x, pos.y, pos.z);
    mDoMtx_stack_c::ZXYrotM(angle.x, angle.y, angle.z);
    s_body->setBaseScale(cXyz(1.0f, 1.0f, 1.0f));
    s_body->setBaseTRMtx(mDoMtx_stack_c::get());
    step(3, "base matrix set");

    if (s_bck != nullptr) {
        s_bck->play();
        s_bck->entry(s_body->getModelData());
    }
    step(4, "anim entered");

    s_body->calc();
    step(5, "body calc");
    apply_dark_light(s_body);
    step(6, "body light");
    mDoExt_modelUpdateDL(s_body);
    step(7, "body drawn");

    draw_at(s_face, s_body->getAnmMtx(kHeadJoint));
    step(8, "face drawn");
    draw_at(s_head, s_body->getAnmMtx(kHeadJoint));
    step(9, "head drawn");

    if (s_hands != nullptr && s_hands->getModelData()->getJointNum() > 2) {
        s_hands->setBaseTRMtx(s_body->getBaseTRMtx());
        s_hands->calc();
        s_hands->setAnmMtx(1, s_body->getAnmMtx(kLeftHandJoint));
        s_hands->setAnmMtx(2, s_body->getAnmMtx(kRightHandJoint));
        apply_dark_light(s_hands);
        mDoExt_modelEntryDL(s_hands);
    }
    step(10, "hands drawn");

    draw_at(s_shield, s_body->getAnmMtx(kShieldJoint));
    step(11, "shield drawn");
    draw_at(s_sheath, s_body->getAnmMtx(kSheathJoint));
    step(12, "sheath drawn");
    draw_at(s_sword, s_body->getAnmMtx(kSwordJoint));
    step(13, "sword drawn");

    static bool s_drawLogged = false;
    if (!s_drawLogged) {
        s_drawLogged = true;
        dl_log("[darklink] first draw complete at %d %d %d", static_cast<int>(pos.x),
               static_cast<int>(pos.y), static_cast<int>(pos.z));
    }
}

void boss_rush_darklink_unload() {
    if (s_state == State::Mounting) return;
    release_all();
    s_state = State::Idle;
}

DEFINE_HOOK(&daB_TN_c::execute, BossRushVanillaDarknutExecuteHook);
DEFINE_HOOK(&daB_TN_c::draw, BossRushVanillaDarknutDrawHook);

namespace {

constexpr int32_t kVanillaDarknutHookPriority = 1000;
constexpr const char* kDarknutStage = "D_MN06B";
constexpr const char* kDarkLinkActorName = "DarkLnk";
constexpr int kActorProfileRefreshFrames = 300;

using SearchNameFn = dStage_objectNameInf* (*)(const char*);
SearchNameFn s_searchName = nullptr;

HookAction on_vanilla_darknut_execute_pre(ModContext*, void* args, void* retval, void*) {
    if (!boss_rush_wants_vanilla_darknut()) return HOOK_CONTINUE;
    const int result = BossRushVanillaDarknutExecuteHook::g_orig(mods::arg<daB_TN_c*>(args, 0));
    if (retval != nullptr) *static_cast<int*>(retval) = result;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_vanilla_darknut_draw_pre(ModContext*, void* args, void* retval, void*) {
    if (!boss_rush_wants_vanilla_darknut()) return HOOK_CONTINUE;
    const int result = BossRushVanillaDarknutDrawHook::g_orig(mods::arg<daB_TN_c*>(args, 0));
    if (retval != nullptr) *static_cast<int*>(retval) = result;
    return HOOK_SKIP_ORIGINAL;
}

}

bool boss_rush_darklink_replaces_darknut(const fopAc_ac_c* darknut) {
    if (darknut == nullptr || boss_rush_wants_vanilla_darknut() ||
        !boss_rush_darklink_mod_installed()) {
        return false;
    }
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr && std::strcmp(stage, kDarknutStage) == 0 &&
           static_cast<const daB_TN_c*>(darknut)->mType == 0;
}

s16 boss_rush_darklink_actor_profile() {
    static s16 s_profile = -1;
    static int s_countdown = 0;
    if (s_searchName == nullptr) {
        return -1;
    }
    if (--s_countdown <= 0) {
        s_countdown = kActorProfileRefreshFrames;
        const dStage_objectNameInf* info = s_searchName(kDarkLinkActorName);
        s_profile = info != nullptr ? info->procname : static_cast<s16>(-1);
    }
    return s_profile;
}

ModResult init_boss_rush_darklink(const HookService* hook_svc) {
    void* searchName = nullptr;
    if (hook_svc->resolve(mod_ctx, "dStage_searchName", &searchName, nullptr) == MOD_OK) {
        s_searchName = reinterpret_cast<SearchNameFn>(searchName);
    }

    HookOptions options = HOOK_OPTIONS_INIT;
    options.priority = kVanillaDarknutHookPriority;
    ModResult result = mods::hook::add_pre<BossRushVanillaDarknutExecuteHook>(
        hook_svc, on_vanilla_darknut_execute_pre, &options);
    if (result != MOD_OK) return result;
    return mods::hook::add_pre<BossRushVanillaDarknutDrawHook>(
        hook_svc, on_vanilla_darknut_draw_pre, &options);
}
