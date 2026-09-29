#include "boss_rush_darklink.hpp"

#include "../util.hpp"
#include "boss_rush.hpp"
#include "../general/fast_forward_cutscenes.hpp"

#include "d/d_attention.h"
#include "d/d_meter2.h"
#include "d/d_msg_object.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_resorce.h"
#include "d/d_stage.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_b_tn.h"
#include "mods/hook.hpp"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_graphic.h"
#include "f_op/f_op_msg_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_counter.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphLoader/J3DAnmLoader.h"
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

constexpr u8 kGhostAlpha = 130;
constexpr f32 kSpecularIntensity = 0.35f;
constexpr f32 kShininess = 4.0f;
constexpr u8 kBaseTint = 0;

constexpr f32 kKeyLightDir[3] = {0.4f, -0.6f, -0.7f};
constexpr f32 kRimLightDir[3] = {-0.5f, -0.3f, 0.8f};
constexpr GXColor kKeyLightColor = {255, 255, 255, 255};
constexpr GXColor kRimLightColor = {190, 195, 205, 255};

constexpr const char* kEyeTexture = "al_eyeball";
constexpr GXColor kEyeColor = {255, 40, 30, 255};
constexpr f32 kEyeGlowGain = 2.2f;

constexpr u16 kCapFirstJoint = 6;
constexpr u16 kCapLastJoint = 9;
constexpr f32 kCapDroop[kCapLastJoint - kCapFirstJoint + 1] = {0.45f, 0.3f, 0.25f, 0.2f};
constexpr f32 kCapSwayDegrees[kCapLastJoint - kCapFirstJoint + 1] = {2.0f, 4.0f, 6.0f, 8.0f};
constexpr f32 kCapSwaySpeed = 0.06f;
constexpr f32 kCapSwayLag = 0.6f;
constexpr u8 kSpecularLightMask = 0x0C;
constexpr f32 kLightDistance = 1.0e18f;
constexpr u8 kTevScale = kSpecularIntensity > 2.0f   ? GX_CS_SCALE_4
                         : kSpecularIntensity > 1.0f ? GX_CS_SCALE_2
                                                     : GX_CS_SCALE_1;
constexpr f32 kTevScaleFactor = kSpecularIntensity > 2.0f ? 4.0f : kSpecularIntensity > 1.0f ? 2.0f : 1.0f;

constexpr int kMat3InitData = 0;
constexpr int kMat3Remap = 1;
constexpr int kMat3MatColor = 5;
constexpr int kMat3ColorChanNum = 6;
constexpr int kMat3ColorChan = 7;
constexpr int kMat3TexNo = 15;
constexpr int kMat3TevOrder = 16;
constexpr int kMat3TevStageNum = 19;
constexpr int kMat3TevStage = 20;
constexpr int kMat3AlphaComp = 24;
constexpr int kMat3Blend = 25;
constexpr u32 kMaterialInitSize = 0x14C;

enum class State { Idle, Mounting, Ready, Failed };

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

u16 read_be16(const u8* p) { return static_cast<u16>((p[0] << 8) | p[1]); }

u32 read_be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);
}

bool is_eye_texture(const char* name) {
    return name != nullptr &&
           (std::strcmp(name, "al_eyeball") == 0 || std::strcmp(name, "highlight02") == 0 ||
            std::strcmp(name, "eye_kage01") == 0);
}

void write_be16(u8* p, u16 v) {
    p[0] = static_cast<u8>(v >> 8);
    p[1] = static_cast<u8>(v & 0xFF);
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

struct Tex1Info {
    u8* section = nullptr;
    u16 count = 0;
    u32 headerOffset = 0;
    const u8* names = nullptr;
};

Tex1Info read_tex1(u8* file, const u8* end, u32 tex1) {
    Tex1Info info;
    u8* section = file + tex1;
    if (tex1 == 0 || section + 20 > end) return info;
    info.section = section;
    info.count = read_be16(section + 8);
    info.headerOffset = read_be32(section + 12);
    const u32 nameOffset = read_be32(section + 16);
    info.names = nameOffset != 0 ? section + nameOffset : nullptr;
    return info;
}

u16 recolor_eye_565(u16 c) {
    const f32 r = ((c >> 11) & 0x1F) * 255.0f / 31.0f;
    const f32 b = (c & 0x1F) * 255.0f / 31.0f;
    f32 glow = (b - r) * kEyeGlowGain / 255.0f;
    glow = glow < 0.0f ? 0.0f : (glow > 1.0f ? 1.0f : glow);
    const u32 nr = static_cast<u32>(kEyeColor.r * glow * 31.0f / 255.0f + 0.5f);
    const u32 ng = static_cast<u32>(kEyeColor.g * glow * 63.0f / 255.0f + 0.5f);
    const u32 nb = static_cast<u32>(kEyeColor.b * glow * 31.0f / 255.0f + 0.5f);
    return static_cast<u16>((nr << 11) | (ng << 5) | nb);
}

void recolor_eye_cmpr_block(u8* block) {
    const u16 c0 = read_be16(block);
    const u16 c1 = read_be16(block + 2);
    u16 n0 = recolor_eye_565(c0);
    u16 n1 = recolor_eye_565(c1);
    if (c0 > c1) {
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
            const u8 v = block[i];
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

int s_specularPatched = 0;
int s_eyeMaterials = 0;
int s_eyeTextures = 0;

void patch_tex1(const Tex1Info& tex, u8* file, const u8* end) {
    if (tex.section == nullptr) return;
    std::vector<const u8*> recolored;
    for (u16 i = 0; i < tex.count; i++) {
        u8* header = tex.section + tex.headerOffset + 32u * i;
        if (header + 32 > end) break;
        const char* name = tex1_name(tex.names, end, i);
        if (is_eye_texture(name)) header[0x17] = 0;
        if (name == nullptr || std::strcmp(name, kEyeTexture) != 0 || header[0] != GX_TF_CMPR) continue;

        u8* pixels = header + static_cast<s32>(read_be32(header + 0x1C));
        if (pixels < file || pixels >= end) continue;
        if (std::find(recolored.begin(), recolored.end(), pixels) != recolored.end()) continue;
        recolored.push_back(pixels);

        u32 w = read_be16(header + 2);
        u32 h = read_be16(header + 4);
        u32 size = 0;
        const u32 levels = header[0x18] > 0 ? header[0x18] : 1;
        for (u32 level = 0; level < levels; level++) {
            size += ((w + 7) / 8) * ((h + 7) / 8) * 32;
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
        }
        if (pixels + size > end) size = static_cast<u32>(end - pixels);
        for (u32 b = 0; b + 8 <= size; b += 8) recolor_eye_cmpr_block(pixels + b);
        s_eyeTextures++;
    }
}

u8 scale_alpha_ref(u8 ref) {
    return static_cast<u8>((ref * kGhostAlpha + 127) / 255);
}

void write_specular_stage(u8* stage, bool textured) {
    stage[1] = GX_CC_ZERO;
    stage[2] = textured ? GX_CC_TEXC : GX_CC_ZERO;
    stage[3] = textured ? GX_CC_KONST : GX_CC_ZERO;
    stage[4] = GX_CC_RASC;
    stage[5] = GX_TEV_ADD;
    stage[6] = GX_TB_ZERO;
    stage[7] = kTevScale;
    stage[8] = 1;
    stage[9] = GX_TEVPREV;
    stage[10] = GX_CA_ZERO;
    stage[11] = textured ? GX_CA_KONST : GX_CA_ZERO;
    stage[12] = textured ? GX_CA_TEXA : GX_CA_ZERO;
    stage[13] = textured ? GX_CA_ZERO : GX_CA_KONST;
    stage[14] = GX_TEV_ADD;
    stage[15] = GX_TB_ZERO;
    stage[16] = GX_CS_SCALE_1;
    stage[17] = 1;
    stage[18] = GX_TEVPREV;
}

void write_eye_stage(u8* stage) {
    stage[1] = GX_CC_TEXC;
    stage[2] = GX_CC_ZERO;
    stage[3] = GX_CC_ZERO;
    stage[4] = GX_CC_RASC;
    stage[5] = GX_TEV_ADD;
    stage[6] = GX_TB_ZERO;
    stage[7] = GX_CS_SCALE_1;
    stage[8] = 1;
    stage[9] = GX_TEVPREV;
    stage[10] = GX_CA_ZERO;
    stage[11] = GX_CA_ZERO;
    stage[12] = GX_CA_ZERO;
    stage[13] = GX_CA_KONST;
    stage[14] = GX_TEV_ADD;
    stage[15] = GX_TB_ZERO;
    stage[16] = GX_CS_SCALE_1;
    stage[17] = 1;
    stage[18] = GX_TEVPREV;
}

void patch_mat3_specular(u8* file, const u8* end, u32 mat3, const Tex1Info& tex) {
    u8* section = file + mat3;
    if (section + 12 + 30 * 4 > end) return;
    const u16 count = read_be16(section + 8);
    u32 table[30];
    for (int i = 0; i < 30; i++) table[i] = read_be32(section + 12 + 4 * i);
    if (table[kMat3InitData] == 0 || table[kMat3Remap] == 0) return;

    auto entry = [&](int t, u32 index, u32 stride) -> u8* {
        if (table[t] == 0 || index == 0xFFFF) return nullptr;
        u8* p = section + table[t] + stride * index;
        return p + stride <= end ? p : nullptr;
    };

    auto eye_slot = [&](const u8* material) -> int {
        for (int s = 0; s < 16; s++) {
            const u8* order = entry(kMat3TevOrder, read_be16(material + 0xBC + 2 * s), 4);
            if (order == nullptr || order[1] >= 8) continue;
            const u8* texNo = entry(kMat3TexNo, read_be16(material + 0x84 + 2 * order[1]), 2);
            if (texNo == nullptr) continue;
            const char* name = tex1_name(tex.names, end, read_be16(texNo));
            if (name != nullptr && std::strcmp(name, kEyeTexture) == 0) return s;
        }
        return -1;
    };

    std::vector<u16> scaledAlphaComps;
    const u8* remap = section + table[kMat3Remap];
    for (int pass = 0; pass < 2; pass++) {
        for (u16 m = 0; m < count; m++) {
            if (remap + 2 * m + 2 > end) break;
            u8* material = entry(kMat3InitData, read_be16(remap + 2 * m), kMaterialInitSize);
            if (material == nullptr) continue;
            const int eyeSlot = eye_slot(material);
            if ((eyeSlot >= 0) != (pass == 1)) continue;
            if (eyeSlot > 0) {
                std::memcpy(material + 0xBC, material + 0xBC + 2 * eyeSlot, 2);
                std::memcpy(material + 0xE4, material + 0xE4 + 2 * eyeSlot, 2);
                std::memcpy(material + 0x104, material + 0x104 + 2 * eyeSlot, 2);
            }
            u8* stage = entry(kMat3TevStage, read_be16(material + 0xE4), 20);
            if (stage == nullptr) continue;

            bool textured = false;
            if (u8* order = entry(kMat3TevOrder, read_be16(material + 0xBC), 4)) {
                order[2] = GX_COLOR0A0;
                textured = order[1] != 0xFF;
            }
            if (eyeSlot >= 0) {
                write_eye_stage(stage);
                s_eyeMaterials++;
            } else {
                write_specular_stage(stage, textured);
            }

            material[0x00] = 4;
            if (material[0x04] != 0xFF) {
                if (u8* stageNum = entry(kMat3TevStageNum, material[0x04], 1)) *stageNum = 1;
            }
            if (material[0x02] != 0xFF) {
                u8* chanNum = entry(kMat3ColorChanNum, material[0x02], 1);
                if (chanNum != nullptr && *chanNum == 0) *chanNum = 1;
            }
            material[0x9C] = GX_TEV_KCSEL_K0;
            material[0xAC] = GX_TEV_KASEL_K0_A;

            if (u8* color = entry(kMat3MatColor, read_be16(material + 0x08), 4)) {
                std::memset(color, 0xFF, 4);
            }
            if (u8* chan = entry(kMat3ColorChan, read_be16(material + 0x0C), 8)) {
                chan[0] = 1;
                chan[1] = GX_SRC_REG;
                chan[2] = kSpecularLightMask;
                chan[3] = GX_DF_NONE;
                chan[4] = GX_AF_SPEC;
                chan[5] = GX_SRC_REG;
            }

            const u16 alphaCompIdx = read_be16(material + 0x146);
            u8* alphaComp = entry(kMat3AlphaComp, alphaCompIdx, 8);
            if (alphaComp != nullptr &&
                std::find(scaledAlphaComps.begin(), scaledAlphaComps.end(), alphaCompIdx) == scaledAlphaComps.end()) {
                scaledAlphaComps.push_back(alphaCompIdx);
                alphaComp[1] = scale_alpha_ref(alphaComp[1]);
                alphaComp[4] = scale_alpha_ref(alphaComp[4]);
            }
            if (u8* blend = entry(kMat3Blend, read_be16(material + 0x148), 4)) {
                blend[0] = GX_BM_BLEND;
                blend[1] = GX_BL_SRCALPHA;
                blend[2] = GX_BL_INVSRCALPHA;
            }
            s_specularPatched++;
        }
    }
}

void patch_bmd_file(u8* file, u32 size) {
    s_specularPatched = 0;
    s_eyeMaterials = 0;
    s_eyeTextures = 0;
    if (size < 0x20 || std::memcmp(file, "J3D2", 4) != 0) return;
    const u8* end = file + size;
    u32 mat3 = 0;
    u32 tex1 = 0;
    u32 offset = 0x20;
    while (offset + 8 <= size) {
        const u32 sectionSize = read_be32(file + offset + 4);
        if (std::memcmp(file + offset, "MAT3", 4) == 0) {
            mat3 = offset;
        } else if (std::memcmp(file + offset, "TEX1", 4) == 0) {
            tex1 = offset;
        }
        if (sectionSize == 0) break;
        offset += sectionSize;
    }
    const Tex1Info tex = read_tex1(file, end, tex1);
    if (mat3 != 0) patch_mat3_specular(file, end, mat3, tex);
    patch_tex1(tex, file, end);
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
    patch_bmd_file(copy, size);
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
    dl_log("[darklink] %s: %u bytes, %d joints, %d materials, %d specular, %d eye materials, %d eye textures",
           label, size, static_cast<int>(data->getJointNum()),
           static_cast<int>(data->getMaterialNum()), s_specularPatched, s_eyeMaterials, s_eyeTextures);
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

void rotate_about_axis(MtxP m, f32 ax, f32 ay, f32 az, f32 angle) {
    const f32 c = std::cos(angle);
    const f32 s = std::sin(angle);
    const f32 t = 1.0f - c;
    const f32 r[3][3] = {
        {t * ax * ax + c, t * ax * ay - s * az, t * ax * az + s * ay},
        {t * ax * ay + s * az, t * ay * ay + c, t * ay * az - s * ax},
        {t * ax * az - s * ay, t * ay * az + s * ax, t * az * az + c},
    };
    f32 out[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out[i][j] = r[i][0] * m[0][j] + r[i][1] * m[1][j] + r[i][2] * m[2][j];
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) m[i][j] = out[i][j];
    }
}

int cap_joint_callback(J3DJoint* joint, int op) {
    if (op != 0) return 1;
    J3DModel* model = j3dSys.getModel();
    const int j = joint->getJntNo();
    if (model == nullptr || model != s_head || j < kCapFirstJoint || j > kCapLastJoint) return 1;
    MtxP cur = J3DSys::mCurrentMtx;
    const f32 len = std::sqrt(cur[0][0] * cur[0][0] + cur[1][0] * cur[1][0] + cur[2][0] * cur[2][0]);
    if (len < 1.0e-4f) return 1;
    const f32 dx = cur[0][0] / len;
    const f32 dy = cur[1][0] / len;
    const f32 dz = cur[2][0] / len;
    const f32 horizontal = std::sqrt(dx * dx + dz * dz);
    if (horizontal < 1.0e-4f) return 1;
    const int k = j - kCapFirstJoint;
    const f32 phase = static_cast<f32>(g_Counter.mCounter0) * kCapSwaySpeed - k * kCapSwayLag;
    const f32 sway = kCapSwayDegrees[k] * (3.14159265f / 180.0f) * std::sin(phase);
    const f32 angle = std::atan2(horizontal, -dy) * kCapDroop[k] + sway;
    rotate_about_axis(cur, dz / horizontal, 0.0f, -dx / horizontal, angle);
    model->setAnmMtx(j, cur);
    return 1;
}

void setup_cap(J3DModel* head) {
    if (head == nullptr) return;
    J3DModelData* data = head->getModelData();
    if (data->getJointNum() <= kCapLastJoint) return;
    for (u16 j = kCapFirstJoint; j <= kCapLastJoint; j++) {
        data->getJointNodePointer(j)->setCallBack(cap_joint_callback);
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
    setup_cap(s_head);
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

void setup_specular_light(J3DLightObj& light, const f32* dir, const GXColor& color) {
    const f32 len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    const f32 dx = dir[0] / len;
    const f32 dy = dir[1] / len;
    const f32 dz = dir[2] / len;
    f32 hx = -dx;
    f32 hy = -dy;
    f32 hz = 1.0f - dz;
    const f32 halfLen = std::sqrt(hx * hx + hy * hy + hz * hz);
    if (halfLen > 0.0f) {
        hx /= halfLen;
        hy /= halfLen;
        hz /= halfLen;
    }
    const f32 colorScale = kSpecularIntensity / kTevScaleFactor;
    J3DLightInfo* info = light.getLightInfo();
    info->mLightPosition.x = -dx * kLightDistance;
    info->mLightPosition.y = -dy * kLightDistance;
    info->mLightPosition.z = -dz * kLightDistance;
    info->mLightDirection.x = hx;
    info->mLightDirection.y = hy;
    info->mLightDirection.z = hz;
    info->mColor.r = clamp_u8(color.r * colorScale);
    info->mColor.g = clamp_u8(color.g * colorScale);
    info->mColor.b = clamp_u8(color.b * colorScale);
    info->mColor.a = 255;
    info->mCosAtten.x = 0.0f;
    info->mCosAtten.y = 0.0f;
    info->mCosAtten.z = 1.0f;
    info->mDistAtten.x = kShininess * 0.5f;
    info->mDistAtten.y = 0.0f;
    info->mDistAtten.z = 1.0f - kShininess * 0.5f;
}

void apply_dark_light(J3DModel* model) {
    daAlink_c* alink = daAlink_getAlinkActorClass();
    if (alink == nullptr) return;
    g_env_light.settingTevStruct_colget_player(&alink->tevStr);
    std::memcpy(static_cast<void*>(&s_darkTev), static_cast<const void*>(&alink->tevStr),
                sizeof(dKy_tevstr_c));
    s_darkTev.AmbCol.r = 0;
    s_darkTev.AmbCol.g = 0;
    s_darkTev.AmbCol.b = 0;
    const u8 tint = clamp_u8(kBaseTint / kTevScaleFactor);
    s_darkTev.TevKColor.r = tint;
    s_darkTev.TevKColor.g = tint;
    s_darkTev.TevKColor.b = tint;
    s_darkTev.TevKColor.a = kGhostAlpha;
    g_env_light.setLightTevColorType_MAJI(model, &s_darkTev);
    setup_specular_light(s_darkTev.mLights[0], kKeyLightDir, kKeyLightColor);
    setup_specular_light(s_darkTev.mLights[1], kRimLightDir, kRimLightColor);
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

namespace {

constexpr int kFightZoneSwitch = 0xE;
constexpr u32 kIntroMessageId = 0x487;
constexpr f32 kTriggerDistance = 250.0f;
constexpr f32 kTriggerOvershoot = 650.0f;
constexpr u32 kRetrySkipTimeoutFrames = 1800;
constexpr int kRetrySkipTimeoutTicks = 1800;
constexpr f32 kRetrySkipFadeInSpeed = 0.1f;
constexpr int kRetrySkipSettleTicks = 20;
constexpr int kRetrySkipLogInterval = 300;

enum class SkipPhase { Off, Trigger, Challenge, Intro, Settle };

SkipPhase s_skipPhase = SkipPhase::Off;
u32 s_skipStartFrame = 0;
int s_skipTicks = 0;
bool s_skipSawActor = false;
bool s_fakeLockon = false;
bool s_injectA = false;
bool s_injectToggle = false;
fpc_ProcID s_introMessageId = fpcM_ERROR_PROCESS_ID_e;
fopAc_ac_c* s_darkLinkActor = nullptr;
bool s_warpBlack = false;
bool s_inDarkLinkExecute = false;
int s_settleTicks = 0;
bool s_haveReturnPos = false;
cXyz s_returnPos;
s16 s_returnAngle = 0;

const char* skip_phase_name(SkipPhase phase) {
    switch (phase) {
    case SkipPhase::Trigger: return "trigger";
    case SkipPhase::Challenge: return "challenge";
    case SkipPhase::Intro: return "intro";
    case SkipPhase::Settle: return "settle";
    default: return "off";
    }
}

fopAc_ac_c* find_darklink_actor() {
    const s16 profile = boss_rush_darklink_actor_profile();
    return profile >= 0 ? fopAcM_SearchByName(profile) : nullptr;
}

bool in_darklink_stage() {
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr && std::strcmp(stage, kDarknutStage) == 0;
}

void hold_screen_black() {
    mDoGph_gInf_c::fadeOut(0.0f, g_blackColor);
    mDoGph_gInf_c::setFadeRate(1.0f);
}

void restore_link_start() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr || !s_haveReturnPos) return;
    link->current.pos = s_returnPos;
    link->old.pos = s_returnPos;
    link->shape_angle.y = s_returnAngle;
    link->current.angle.y = s_returnAngle;
    link->speed.set(0.0f, 0.0f, 0.0f);
    link->mNormalSpeed = 0.0f;

    camera_process_class* cam = dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
    if (cam == nullptr) return;
    const f32 fx = cM_ssin(s_returnAngle);
    const f32 fz = cM_scos(s_returnAngle);
    cXyz center(s_returnPos.x + fx * 200.0f, s_returnPos.y + 100.0f, s_returnPos.z + fz * 200.0f);
    cXyz eye(s_returnPos.x - fx * 450.0f, s_returnPos.y + 170.0f, s_returnPos.z - fz * 450.0f);
    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    cam->mCamera.QuickStart();
    cam->mCamera.SetTrimSize(0);
    cam->view.lookat.center.set(center.x, center.y, center.z);
    cam->view.lookat.eye.set(eye.x, eye.y, eye.z);
    fopCamM_SetAngleY(cam, s_returnAngle);
}

void stop_skip_helpers() {
    s_fakeLockon = false;
    s_injectA = false;
    s_inDarkLinkExecute = false;
    s_introMessageId = fpcM_ERROR_PROCESS_ID_e;
    fast_forward_set_hidden_run(false);
}

void end_retry_skip(const char* reason) {
    if (s_skipPhase == SkipPhase::Off || s_skipPhase == SkipPhase::Settle) return;
    dl_log("[darklink] retry skip end: %s (phase %s, %d ticks)", reason, skip_phase_name(s_skipPhase), s_skipTicks);
    stop_skip_helpers();
    restore_link_start();
    s_skipPhase = SkipPhase::Settle;
    s_settleTicks = kRetrySkipSettleTicks;
    hold_screen_black();
}

void abort_retry_skip(const char* reason) {
    if (s_skipPhase == SkipPhase::Off) return;
    dl_log("[darklink] retry skip aborted: %s (phase %s, %d ticks)", reason, skip_phase_name(s_skipPhase), s_skipTicks);
    stop_skip_helpers();
    s_skipPhase = SkipPhase::Off;
    mDoGph_gInf_c::fadeIn(kRetrySkipFadeInSpeed, g_blackColor);
}

void place_link_past_trigger() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    fopAc_ac_c* tn = fopAcM_SearchByName(fpcNm_B_TN_e);
    if (link == nullptr || tn == nullptr) return;
    const f32 targetZ = tn->home.pos.z - kTriggerDistance - kTriggerOvershoot;
    if (link->current.pos.z < targetZ + 50.0f) return;
    cXyz pos(tn->home.pos.x, link->current.pos.y, targetZ);
    link->current.pos = pos;
    link->old.pos = pos;
    link->shape_angle.y = static_cast<s16>(0x8000);
    link->current.angle.y = static_cast<s16>(0x8000);
    link->speed.set(0.0f, 0.0f, 0.0f);
    link->mNormalSpeed = 0.0f;
    dl_log("[darklink] retry skip: link moved past the trigger (z %d, darknut home z %d)",
           static_cast<int>(pos.z), static_cast<int>(tn->home.pos.z));
}

void set_skip_phase(SkipPhase phase) {
    if (phase == s_skipPhase) return;
    s_skipPhase = phase;
    dl_log("[darklink] retry skip phase: %s", skip_phase_name(phase));
}

}

bool boss_rush_darklink_fight_started() {
    return in_darklink_stage() &&
           dComIfGs_isOneZoneSwitch(kFightZoneSwitch, dComIfGp_roomControl_getStayNo()) != 0;
}

void boss_rush_darklink_on_fight_landed(bool retry) {
    if (!in_darklink_stage()) return;
    dComIfGs_offOneZoneSwitch(kFightZoneSwitch, dComIfGp_roomControl_getStayNo());
    if (!retry) return;
    daAlink_c* link = daAlink_getAlinkActorClass();
    s_haveReturnPos = link != nullptr;
    if (link != nullptr) {
        s_returnPos = link->current.pos;
        s_returnAngle = link->shape_angle.y;
    }
    s_skipStartFrame = g_Counter.mCounter0;
    s_skipTicks = 0;
    s_skipSawActor = false;
    s_fakeLockon = false;
    s_injectA = false;
    s_introMessageId = fpcM_ERROR_PROCESS_ID_e;
    s_skipPhase = SkipPhase::Off;
    set_skip_phase(SkipPhase::Trigger);
    hold_screen_black();
    fast_forward_set_hidden_run(true);
}

void boss_rush_darklink_set_warp_black(bool on) {
    s_warpBlack = on;
    if (on) hold_screen_black();
}

bool boss_rush_darklink_retry_skip_active() {
    return s_skipPhase != SkipPhase::Off;
}

void update_boss_rush_darklink_retry_skip() {
    if (s_skipPhase == SkipPhase::Off) return;
    ++s_skipTicks;

    if (!is_boss_rush_active() || boss_rush_is_returning_to_chamber() || dComIfGp_isEnableNextStage() ||
        !in_darklink_stage()) {
        abort_retry_skip("left the fight");
        return;
    }
    if (s_skipPhase == SkipPhase::Settle) {
        hold_screen_black();
        if (--s_settleTicks > 0) return;
        restore_link_start();
        s_skipPhase = SkipPhase::Off;
        mDoGph_gInf_c::fadeIn(kRetrySkipFadeInSpeed, g_blackColor);
        return;
    }
    if (boss_rush_darklink_fight_started()) {
        end_retry_skip("fight started");
        return;
    }
    if (s_skipTicks > kRetrySkipTimeoutTicks || g_Counter.mCounter0 - s_skipStartFrame > kRetrySkipTimeoutFrames) {
        end_retry_skip("timeout");
        return;
    }

    hold_screen_black();
    fast_forward_set_hidden_run(true);

    fopAc_ac_c* darkLink = find_darklink_actor();
    s_darkLinkActor = darkLink;
    const bool event = dComIfGp_event_runCheck() != 0;
    if (s_skipTicks % kRetrySkipLogInterval == 0) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        dl_log("[darklink] retry skip status: phase %s, actor %d, event %d, link z %d room %d, stay room %d",
               skip_phase_name(s_skipPhase), darkLink != nullptr, static_cast<int>(event),
               link != nullptr ? static_cast<int>(link->current.pos.z) : 0,
               link != nullptr ? static_cast<int>(fopAcM_GetRoomNo(link)) : -1,
               static_cast<int>(dComIfGp_roomControl_getStayNo()));
    }
    if (darkLink == nullptr) {
        if (s_skipSawActor) {
            end_retry_skip("dark link vanished");
            return;
        }
        set_skip_phase(SkipPhase::Trigger);
        s_fakeLockon = false;
        s_injectA = false;
        if (!event) place_link_past_trigger();
        return;
    }
    s_skipSawActor = true;
    if (!event && s_introMessageId == fpcM_ERROR_PROCESS_ID_e) {
        set_skip_phase(SkipPhase::Challenge);
        place_link_past_trigger();
        s_fakeLockon = true;
        s_injectA = false;
        return;
    }
    set_skip_phase(SkipPhase::Intro);
    s_fakeLockon = false;
    s_injectA = s_introMessageId != fpcM_ERROR_PROCESS_ID_e && fopMsgM_SearchByID(s_introMessageId) != nullptr;
}

DEFINE_HOOK(&dAttention_c::LockonTarget, DarkLinkLockonTargetHook);
DEFINE_HOOK(&dAttention_c::LockonTruth, DarkLinkLockonTruthHook);
DEFINE_HOOK(&mDoCPd_c::read, DarkLinkSkipPadReadHook);
DEFINE_HOOK(&fopMsgM_messageSetDemo, DarkLinkIntroMessageHook);
DEFINE_HOOK_SYMBOL("src/f_op/f_op_actor.cpp#fopAc_Execute", int(void*), DarkLinkActorExecuteHook);
DEFINE_HOOK(&mDoGph_gInf_c::calcFade, DarkLinkCalcFadeHook);
DEFINE_HOOK(&dMeter2_c::_draw, DarkLinkMeterDrawHook);
DEFINE_HOOK(&dAttention_c::Draw, DarkLinkAttentionDrawHook);
DEFINE_HOOK(&dMsgObject_c::_draw, DarkLinkMessageDrawHook);

namespace {

bool retry_black_active() {
    return s_warpBlack || s_skipPhase != SkipPhase::Off;
}

HookAction on_hidden_draw_pre(ModContext*, void*, void* retval, void*) {
    if (!retry_black_active()) return HOOK_CONTINUE;
    if (retval != nullptr) *static_cast<int*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

HookAction on_attention_draw_pre(ModContext*, void*, void*, void*) {
    return retry_black_active() ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
}

HookAction on_calc_fade_pre(ModContext*, void*, void*, void*) {
    if (retry_black_active()) hold_screen_black();
    return HOOK_CONTINUE;
}

HookAction on_actor_execute_pre(ModContext*, void* args, void*, void*) {
    if (s_fakeLockon && s_darkLinkActor != nullptr) {
        s_inDarkLinkExecute = mods::arg<void*>(args, 0) == static_cast<void*>(s_darkLinkActor);
    }
    return HOOK_CONTINUE;
}

void on_actor_execute_post(ModContext*, void*, void*, void*) {
    s_inDarkLinkExecute = false;
}

void on_lockon_target_post(ModContext*, void* args, void* retval, void*) {
    if (!s_fakeLockon || !s_inDarkLinkExecute || retval == nullptr || mods::arg<s32>(args, 1) != 0) return;
    *static_cast<fopAc_ac_c**>(retval) = s_darkLinkActor;
}

void on_lockon_truth_post(ModContext*, void*, void* retval, void*) {
    if (s_fakeLockon && s_inDarkLinkExecute && retval != nullptr) *static_cast<bool*>(retval) = true;
}

void on_intro_message_post(ModContext*, void* args, void* retval, void*) {
    if (s_skipPhase == SkipPhase::Off || retval == nullptr) return;
    if (mods::arg<u32>(args, 0) != kIntroMessageId) return;
    s_introMessageId = *static_cast<fpc_ProcID*>(retval);
    dl_log("[darklink] retry skip: intro message opened");
}

void on_skip_pad_read_post(ModContext*, void*, void*, void*) {
    if (!s_injectA) return;
    s_injectToggle = !s_injectToggle;
    if (!s_injectToggle) return;
    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    pad.mPressedButtonFlags |= PAD_BUTTON_A;
    pad.mButtonFlags |= PAD_BUTTON_A;
}

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
    result = mods::hook::add_pre<BossRushVanillaDarknutDrawHook>(
        hook_svc, on_vanilla_darknut_draw_pre, &options);
    if (result != MOD_OK) return result;
    mods::hook::add_post<DarkLinkLockonTargetHook>(hook_svc, on_lockon_target_post);
    mods::hook::add_post<DarkLinkLockonTruthHook>(hook_svc, on_lockon_truth_post);
    mods::hook::add_post<DarkLinkSkipPadReadHook>(hook_svc, on_skip_pad_read_post);
    mods::hook::add_post<DarkLinkIntroMessageHook>(hook_svc, on_intro_message_post);
    mods::hook::add_pre<DarkLinkActorExecuteHook>(hook_svc, on_actor_execute_pre);
    mods::hook::add_pre<DarkLinkCalcFadeHook>(hook_svc, on_calc_fade_pre);
    HookOptions hideOptions = HOOK_OPTIONS_INIT;
    hideOptions.priority = kVanillaDarknutHookPriority;
    mods::hook::add_pre<DarkLinkMeterDrawHook>(hook_svc, on_hidden_draw_pre, &hideOptions);
    mods::hook::add_pre<DarkLinkAttentionDrawHook>(hook_svc, on_attention_draw_pre, &hideOptions);
    mods::hook::add_pre<DarkLinkMessageDrawHook>(hook_svc, on_hidden_draw_pre, &hideOptions);
    mods::hook::add_post<DarkLinkActorExecuteHook>(hook_svc, on_actor_execute_post);
    return MOD_OK;
}
