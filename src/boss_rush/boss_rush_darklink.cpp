// this file mostly ai cuz of guessing
#include "boss_rush_darklink.hpp"

#include "../util.hpp"
#include "boss_rush.hpp"
#include "../general/fast_forward_cutscenes.hpp"

#include "d/d_attention.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "d/d_particle.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_wether.h"
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
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DMatBlock.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphLoader/J3DAnmLoader.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "mods/svc/log.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern const LogService* svc_log;


namespace {

constexpr const char* kDarkLinkModId = "hopex.dark_link";
constexpr const char* kKmdlPath = "/res/Object/Kmdl.arc";
constexpr const char* kAlinkPath = "/res/Object/Alink.arc";
constexpr const char* kWoodShieldPath = "/res/Object/CWShd.arc";
constexpr const char* kOrdonShieldPath = "/res/Object/SWShd.arc";
constexpr const char* kHylianShieldPath = "/res/Object/HyShd.arc";
constexpr u32 kTagBmwr = 'BMWR';
constexpr u32 kTagBmwe = 'BMWE';
constexpr u32 kOrdonSwordIndex = 0x3C;
constexpr u32 kOrdonSheathIndex = 0x3B;
constexpr u32 kMasterSwordIndex = 0x38;
constexpr u32 kMasterSheathIndex = 0x37;
constexpr u32 kWoodSwordIndex = 0x11;
constexpr u32 kShieldIndex = 0x3;
constexpr u32 kHeapSize = 0x600000;
constexpr u32 kRootHeapReserve = 0xC00000;
constexpr u32 kGameHeapReserve = 0x400000;
constexpr u32 kModelHeadroom = 0x40000;
constexpr u32 kMaxPartFileSize = 0x4000000;
constexpr u32 kIdleWaitAnmIndex = 0x26A;
constexpr u32 kLinkAnmBufferSize = 0x10800;
constexpr u32 kBodyDiffFlags = 0x11000084;
constexpr u32 kEquipDiffFlags = 0x11000087;
constexpr u16 kHeadJoint = 4;
constexpr u16 kSheathJoint = 5;
constexpr u16 kLeftHandJoint = 9;
constexpr u16 kSwordJoint = 10;
constexpr u16 kRightHandJoint = 0xE;
constexpr u16 kShieldJoint = 15;
constexpr u16 kSwordGripMaterial = 0;
constexpr u16 kOpenHandMaterial = 4;
constexpr u16 kShieldGripMaterial = 6;
constexpr u16 kEmptyHandMaterial = 10;
constexpr u16 kHandsMinMaterials = 11;
constexpr u16 kHatFirstJoint = 6;
constexpr u16 kHatTipJoint = 7;
constexpr u16 kHatLastJoint = 9;
constexpr u16 kHatBaseJoint = 2;

constexpr u16 kSpecularChanId = 0x4202;
constexpr u16 kUnlitChanId = 0x0400;
constexpr u8 kSpecularAmbient = 7;
constexpr u8 kEyeRed = 0xE6;
constexpr u8 kEyeGreen = 0x08;
constexpr u8 kEyeBlue = 0x05;
constexpr u8 kSolidAlpha = 170;
constexpr u16 kSolidZModeId = 0x16;
constexpr u16 kSolidAlphaCmpId = 0x87;
constexpr int kMaxSolidEntries = 0x80;
constexpr u8 kMaxTevStages = 15;
constexpr u8 kMaxEyeBaseStages = 11;

constexpr const char* kParticlePath = "/res/Particle/Pscene171.jpc";
constexpr u16 kWarpResourceA = 0x88FE;
constexpr u16 kWarpResourceB = 0x88FF;
constexpr u16 kAuraResource = 0x8900;
constexpr u32 kParticleMax = 0x400;
constexpr u32 kEmitterMax = 0x20;
constexpr u8 kAuraDrawGroup = 9;
constexpr int kAuraCount = 7;
constexpr u16 kAuraJoints[kAuraCount] = {4, 2, 16, 8, 13, 19, 24};
constexpr f32 kAuraHeadRate = 0.08f;
constexpr f32 kAuraRate = 0.15f;
constexpr s16 kAuraLifeTime = 24;
constexpr f32 kAuraScale = 0.22f;
constexpr u8 kAuraAlpha = 120;

constexpr const char* kEyeMaterialLeft = "al_eyeballL_m";
constexpr const char* kEyeMaterialRight = "al_eyeballR_m";
constexpr const char* kEyeTexture = "al_eyeball";

enum class State { Idle, Mounting, Ready, Failed };

struct SolidEntry {
    J3DModelData* data;
    J3DMaterial* material;
    int reg;
};

struct HatState {
    s16 pitch[3];
    s16 yaw[3];
    s16 pitchSpeed[3];
    s16 yawSpeed[3];
    s16 sway[3];
    cXyz prevPos;
    s16 prevPitch;
    s16 prevYaw;
    s16 phase;
    bool ready;
};

class DepthPacket : public J3DPacket {
public:
    void draw() override;

    J3DModel* mModels[7] = {};
};

State s_state = State::Idle;
JKRExpHeap* s_heap = nullptr;
JKRExpHeap* s_swordHeap = nullptr;
JKRExpHeap* s_sheathHeap = nullptr;
JKRExpHeap* s_shieldHeap = nullptr;
u8 s_loadedSword = dItemNo_NONE_e;
u8 s_loadedShield = dItemNo_NONE_e;
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
SolidEntry s_solid[kMaxSolidEntries] = {};
int s_solidCount = 0;
HatState s_hat = {};
dKy_tevstr_c s_tevstr;
bool s_tevstrReady = false;
J3DLightObj s_frameLight;
DepthPacket s_depthPacket;
mDoDvdThd_toMainRam_c* s_particleCmd = nullptr;
JPAEmitterManager* s_particles = nullptr;
JPABaseEmitter* s_aura[kAuraCount] = {};
u32 s_particleTick = 0;
u32 s_statueDrawTick = 0;

void dl_log(const char* fmt, ...) {
    if (svc_log == nullptr || svc_log->info == nullptr) return;
    char msg[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    svc_log->info(mod_ctx, msg);
}

JKRHeap* pick_parent_heap(u32 size) {
    JKRHeap* root = JKRHeap::getRootHeap();
    const u32 rootFree = root != nullptr ? static_cast<u32>(root->getFreeSize()) : 0;
    if (rootFree > size + kRootHeapReserve) return root;
    JKRHeap* game = mDoExt_getGameHeap();
    const u32 gameFree = game != nullptr ? static_cast<u32>(game->getFreeSize()) : 0;
    if (gameFree > size + kGameHeapReserve) return game;
    dl_log("[darklink] not enough memory for %u bytes (root free %u, game free %u)", size, rootFree,
           gameFree);
    return nullptr;
}

u32 model_budget(u32 size) {
    return size * 2 + kModelHeadroom;
}

u32 read_be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);
}

bool is_eye_material(J3DModelData* data, u16 index) {
    JUTNameTab* names = data->getMaterialName();
    const char* name = names != nullptr ? names->getName(index) : nullptr;
    return name != nullptr && (std::strcmp(name, kEyeMaterialLeft) == 0 || std::strcmp(name, kEyeMaterialRight) == 0);
}

int tev_texture_slots(J3DTevBlock* tev) {
    switch (tev->getType()) {
    case 'TVB1': return 1;
    case 'TVB2': return 2;
    case 'TVB4': return 4;
    case 'TV16':
    case 'TVPT': return 8;
    default: return 0;
    }
}

J3DTevOrder tev_order(u8 texCoord, u8 texMap, u8 channel) {
    J3DTevOrderInfo info;
    info.mTexCoord = texCoord;
    info.mTexMap = texMap;
    info.mColorChan = channel;
    info.field_0x3 = 0;
    return J3DTevOrder(info);
}

J3DTevSwapModeTable swap_table(u8 r, u8 g, u8 b, u8 a) {
    J3DTevSwapModeTableInfo info;
    info.field_0x0 = r;
    info.field_0x1 = g;
    info.field_0x2 = b;
    info.field_0x3 = a;
    return J3DTevSwapModeTable(info);
}

J3DColorChan color_chan(u16 id) {
    J3DColorChan chan;
    chan.mColorChanID = id;
    return chan;
}

void write_passthrough_alpha(J3DTevStage* stage, int rasSwap) {
    stage->mTevSwapModeInfo = static_cast<u8>(((stage->mTevSwapModeInfo & 0x0C) + rasSwap) | 0x80);
    stage->mTevAlphaOp = 0x08;
    stage->mTevAlphaAB = 0xFF;
}

void apply_dark_light(J3DModelData* data, J3DLightObj* light) {
    J3DLightInfo* info = light->getLightInfo();
    info->mLightPosition.x = -400000.0f;
    info->mLightPosition.y = 500000.0f;
    info->mLightPosition.z = 768114.5625f;
    info->mLightDirection.x = -0.2127109318971634f;
    info->mLightDirection.y = 0.26588866114616394f;
    info->mLightDirection.z = 0.9402432441711426f;
    info->mColor.r = 0x60;
    info->mColor.g = 0x68;
    info->mColor.b = 0x74;
    info->mColor.a = 0xFF;
    info->mCosAtten.x = 0.0f;
    info->mCosAtten.y = 0.0f;
    info->mCosAtten.z = 1.0f;
    info->mDistAtten.x = 16.0f;
    info->mDistAtten.y = 0.0f;
    info->mDistAtten.z = -15.0f;

    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DMaterial* material = data->getMaterialNodePointer(i);
        J3DColorBlock* color = material->getColorBlock();
        color->setLight(7, light);
        if (!is_eye_material(data, i)) continue;
        color->setColorChan(2, color_chan(kUnlitChanId));
        J3DGXColor eye = *color->getMatColor(1);
        eye.r = kEyeRed;
        eye.g = kEyeGreen;
        eye.b = kEyeBlue;
        color->setMatColor(1, eye);
        if (J3DFog* fog = material->getPEBlock()->getFog()) fog->mType = 0;
    }
}

bool setup_eye_stages(J3DModelData* data, J3DTevBlock* old, J3DTevBlock16* tev, u8 stageNum, int rasSwap) {
    if (stageNum > kMaxEyeBaseStages) return false;
    JUTNameTab* texNames = data->getTextureName();
    if (texNames == nullptr || stageNum == 0) return false;
    const int texSlots = tev_texture_slots(old);

    J3DTevOrder* eyeOrder = nullptr;
    for (u8 s = 0; s < stageNum && eyeOrder == nullptr; s++) {
        J3DTevOrder* order = old->getTevOrder(s);
        const u8 texMap = order->getTexMap();
        if (texMap >= texSlots) continue;
        const u16 texNo = old->getTexNo(texMap);
        if (texNo == 0xFFFF) continue;
        const char* name = texNames->getName(texNo);
        if (name != nullptr && std::strcmp(name, kEyeTexture) == 0) eyeOrder = order;
    }
    if (eyeOrder == nullptr) return false;

    bool used[4] = {};
    used[rasSwap] = true;
    for (u8 s = 0; s < stageNum; s++) {
        const u8 swap = tev->getTevStage(s)->mTevSwapModeInfo;
        used[swap & 3] = true;
        used[(swap >> 2) & 3] = true;
    }
    u32 freeTables[2] = {};
    int freeCount = 0;
    for (u32 r = 0; r < 4 && freeCount < 2; r++) {
        if (!used[r]) freeTables[freeCount++] = r;
    }
    if (freeCount != 2) return false;

    tev->setTevSwapModeTable(freeTables[0], swap_table(0, 0, 0, 3));
    tev->setTevSwapModeTable(freeTables[1], swap_table(2, 2, 2, 3));

    J3DTevStage* red = tev->getTevStage(stageNum);
    red->setStageNo(stageNum);
    write_passthrough_alpha(red, rasSwap);
    tev->setTevOrder(stageNum, eyeOrder);
    red->mTevSwapModeInfo = static_cast<u8>((red->mTevSwapModeInfo & 0xF3) | (freeTables[0] << 2));
    red->mTevColorCD = 0xF8;
    red->mTevColorOp = 0x08;
    red->mTevColorAB = 0xFF;

    J3DTevStage* blue = tev->getTevStage(stageNum + 1);
    blue->setStageNo(stageNum + 1);
    write_passthrough_alpha(blue, rasSwap);
    tev->setTevOrder(stageNum + 1, eyeOrder);
    blue->mTevSwapModeInfo = static_cast<u8>((blue->mTevSwapModeInfo & 0xF3) | (freeTables[1] << 2));
    blue->mTevColorCD = 0xF8;
    blue->mTevColorOp = 0x2C;
    blue->mTevColorAB = 0x0F;

    J3DTevStage* cut = tev->getTevStage(stageNum + 2);
    cut->setStageNo(stageNum + 2);
    write_passthrough_alpha(cut, rasSwap);
    tev->setTevOrder(stageNum + 2, tev_order(0xFF, 0xFF, GX_COLOR1A1));
    tev->setTevKColorSel(stageNum + 2, static_cast<u8>(GX_TEV_KCSEL_2_8));
    cut->mTevColorCD = 0xF0;
    cut->mTevColorOp = 0x2C;
    cut->mTevColorAB = 0xEF;

    J3DTevStage* tint = tev->getTevStage(stageNum + 3);
    tint->setStageNo(stageNum + 3);
    write_passthrough_alpha(tint, rasSwap);
    tev->setTevOrder(stageNum + 3, tev_order(0xFF, 0xFF, GX_COLOR1A1));
    tint->mTevColorCD = 0xAF;
    tint->mTevColorOp = 0x08;
    tint->mTevColorAB = 0xF0;
    return true;
}

bool prepare_dark_materials(J3DModelData* data) {
    J3DLightObj* lights[8] = {};
    for (J3DLightObj*& light : lights) {
        light = JKR_NEW J3DLightObj();
        if (light == nullptr) return false;
    }
    const J3DTevSwapModeTableInfo& swapInfo = j3dDefaultTevSwapModeTable;
    const u8 defaultSwap = calcTevSwapTableID(swapInfo.field_0x0, swapInfo.field_0x1, swapInfo.field_0x2,
                                              swapInfo.field_0x3);

    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DMaterial* material = data->getMaterialNodePointer(i);
        J3DTevBlock* old = material->getTevBlock();
        const u8 stageNum = old->getTevStageNum();
        const int texSlots = tev_texture_slots(old);
        if (texSlots == 0 || stageNum < 1 || stageNum > kMaxTevStages) return false;

        J3DTevBlock16* tev = JKR_NEW J3DTevBlock16();
        J3DColorBlockLightOn* color = JKR_NEW J3DColorBlockLightOn();
        if (tev == nullptr || color == nullptr) return false;

        for (int t = 0; t < texSlots; t++) {
            tev->setTexNo(t, old->getTexNo(t));
        }
        for (u8 s = 0; s < stageNum; s++) {
            tev->setTevOrder(s, old->getTevOrder(s));
            tev->setTevStage(s, old->getTevStage(s));
            tev->getTevStage(s)->setStageNo(s);
            if (J3DIndTevStage* ind = old->getIndTevStage(s)) tev->setIndTevStage(s, ind);
            tev->setTevKColorSel(s, old->getTevKColorSel(s));
            tev->setTevKAlphaSel(s, old->getTevKAlphaSel(s));
        }
        for (u32 r = 0; r < 4; r++) {
            if (J3DGXColorS10* c = old->getTevColor(r)) tev->setTevColor(r, c);
            if (J3DGXColor* k = old->getTevKColor(r)) tev->setTevKColor(r, k);
            if (J3DTevSwapModeTable* sw = old->getTevSwapModeTable(r)) tev->setTevSwapModeTable(r, sw);
        }

        J3DColorBlock* oldColor = material->getColorBlock();
        color->reset(oldColor);
        color->setCullMode(oldColor->getCullMode());
        color->setColorChanNum(static_cast<u8>(2));
        color->setLight(0, lights[0]);
        for (u32 l = 2; l < 8; l++) {
            color->setLight(l, lights[l]);
        }
        color->setColorChan(2, color_chan(kSpecularChanId));
        J3DGXColor ambient = *color->getAmbColor(1);
        ambient.r = kSpecularAmbient;
        ambient.g = kSpecularAmbient;
        ambient.b = kSpecularAmbient;
        color->setAmbColor(1, ambient);
        J3DGXColor matColor = *color->getMatColor(1);
        matColor.r = 0xFF;
        matColor.g = 0xFF;
        matColor.b = 0xFF;
        color->setMatColor(1, matColor);

        tev->setTevOrder(stageNum, tev_order(0xFF, 0xFF, GX_COLOR1A1));
        J3DTevStage* stage = tev->getTevStage(stageNum);
        stage->setStageNo(stageNum);
        stage->mTevColorCD = 0xFA;
        stage->mTevColorOp = 0x08;
        stage->mTevColorAB = 0xFF;
        stage->mTevSwapModeInfo = static_cast<u8>((stage->mTevSwapModeInfo & 0x0F) | 0x80);
        stage->mTevAlphaOp = 0x08;
        stage->mTevAlphaAB = 0xFF;

        int rasSwap = -1;
        for (u32 r = 0; r < 4 && rasSwap < 0; r++) {
            if (tev->getTevSwapModeTable(r)->mIdx == defaultSwap) rasSwap = static_cast<int>(r);
        }
        if (rasSwap < 0) return false;
        stage->mTevSwapModeInfo = static_cast<u8>((stage->mTevSwapModeInfo & 0xFC) | rasSwap);

        u8 extraStages = 1;
        if (is_eye_material(data, i)) {
            if (!setup_eye_stages(data, old, tev, stageNum, rasSwap)) return false;
            extraStages = 4;
        }
        tev->setTevStageNum(static_cast<u8>(stageNum + extraStages));
        material->mTevBlock = tev;
        material->mColorBlock = color;
        material->mSharedDLObj = nullptr;
        if (material->newSingleSharedDisplayList(material->countDLSize()) != kJ3DError_Success) return false;
    }
    apply_dark_light(data, lights[7]);
    return true;
}

int free_alpha_register(J3DTevBlock* tev) {
    for (u8 reg = 0; reg < 4; reg++) {
        const u8 sel = static_cast<u8>(GX_TEV_KASEL_K0_A + reg);
        bool used = false;
        for (u8 s = 0; s < tev->getTevStageNum() && !used; s++) {
            used = tev->getTevKAlphaSel(s) == sel || tev->getTevKColorSel(s) == sel;
        }
        if (!used) return reg;
    }
    return -1;
}

bool prepare_solid_fade(J3DModelData* data) {
    J3DBlendInfo blendInfo;
    blendInfo.mType = GX_BM_BLEND;
    blendInfo.mSrcFactor = GX_BL_SRCALPHA;
    blendInfo.mDstFactor = GX_BL_INVSRCALPHA;
    blendInfo.mOp = GX_LO_COPY;
    J3DZMode zMode;
    zMode = kSolidZModeId;

    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DMaterial* material = data->getMaterialNodePointer(i);
        J3DPEBlockFogOff* pe = JKR_NEW J3DPEBlockFogOff();
        if (pe == nullptr) return false;
        pe->setBlend(J3DBlend(blendInfo));
        pe->setZMode(zMode);
        pe->setAlphaComp(J3DAlphaComp(kSolidAlphaCmpId));
        pe->setZCompLoc(static_cast<u8>(0));
        material->mPEBlock = pe;
        material->mMaterialMode = 4;

        J3DTevBlock* tev = material->getTevBlock();
        const int reg = free_alpha_register(tev);
        if (reg < 0) return false;
        const u8 stageNum = tev->getTevStageNum();
        if (stageNum > kMaxTevStages) return false;
        tev->setTevOrder(stageNum, tev_order(0xFF, 0xFF, 0xFF));
        J3DTevStage* stage = tev->getTevStage(stageNum);
        stage->setStageNo(stageNum);
        stage->mTevColorCD = 0xF0;
        stage->mTevColorOp = 0x08;
        stage->mTevColorAB = 0xFF;
        stage->mTevAlphaOp = 0x08;
        tev->setTevStageNum(static_cast<u8>(stageNum + 1));
        J3DGXColor konst = *tev->getTevKColor(reg);
        konst.a = 0xFF;
        tev->setTevKColor(reg, konst);
        tev->setTevKAlphaSel(stageNum, static_cast<u8>(GX_TEV_KASEL_K0_A + reg));
        stage->mTevAlphaAB = 0xF8;
        stage->mTevSwapModeInfo = static_cast<u8>((stage->mTevSwapModeInfo & 0x0F) | 0x70);

        material->mSharedDLObj = nullptr;
        if (material->newSingleSharedDisplayList(material->countDLSize()) != kJ3DError_Success) return false;
    }
    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DColorBlock* color = data->getMaterialNodePointer(i)->getColorBlock();
        J3DGXColor matColor = *color->getMatColor(1);
        matColor.a = 0xFF;
        color->setMatColor(1, matColor);
    }
    data->makeSharedDL();
    return true;
}

bool prepare_solid(J3DModelData* data) {
    if (s_solidCount + data->getMaterialNum() > kMaxSolidEntries) return false;
    const int start = s_solidCount;
    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        J3DMaterial* material = data->getMaterialNodePointer(i);
        const int reg = free_alpha_register(material->getTevBlock());
        if (reg < 0) {
            s_solidCount = start;
            return false;
        }
        s_solid[s_solidCount++] = {data, material, reg};
    }
    if (!prepare_solid_fade(data)) {
        s_solidCount = start;
        return false;
    }
    return true;
}

void refresh_solid(J3DModelData* data) {
    for (int i = 0; i < s_solidCount; i++) {
        if (s_solid[i].data != data) continue;
        J3DTevBlock* tev = s_solid[i].material->getTevBlock();
        J3DGXColor konst = *tev->getTevKColor(s_solid[i].reg);
        konst.a = kSolidAlpha;
        tev->setTevKColor(s_solid[i].reg, konst);
    }
}

bool shape_packet_hidden(J3DShapePacket* packet) {
    return packet->checkFlag(0x10) || packet->getShape()->checkFlag(1);
}

void DepthPacket::draw() {
    j3dSys.reinitGX();
    J3DTexture* savedTexture = j3dSys.getTexture();
    GXSetColorUpdate(GX_FALSE);
    GXSetAlphaUpdate(GX_FALSE);
    for (J3DModel* model : mModels) {
        if (model == nullptr) continue;
        J3DModelData* data = model->getModelData();
        j3dSys.setTexture(data->getTexture());
        for (u16 i = 0; i < data->getMaterialNum(); i++) {
            J3DMatPacket* matPacket = model->getMatPacket(i);
            J3DShapePacket* first = matPacket->getShapePacket();
            if (first == nullptr || shape_packet_hidden(first)) continue;
            matPacket->getMaterial()->load();
            matPacket->callDL();
            first->getShape()->loadPreDrawSetting();
            for (J3DShapePacket* packet = first; packet != nullptr;
                 packet = static_cast<J3DShapePacket*>(packet->getNextPacket())) {
                if (shape_packet_hidden(packet)) continue;
                if (packet->getDisplayListObj() != nullptr) packet->callDL();
                GXSetColorUpdate(GX_FALSE);
                GXSetAlphaUpdate(GX_FALSE);
                GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
                GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
                GXSetZCompLoc(GX_FALSE);
                packet->drawFast();
            }
            J3DShape::resetVcdVatCache();
        }
    }
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    j3dSys.setTexture(savedTexture);
    j3dSys.reinitGX();
}

void submit_depth_packet(J3DModel* const (&models)[7]) {
    J3DDrawBuffer* buffer = j3dSys.getDrawBuffer(1);
    if (buffer == nullptr) return;
    for (u32 b = 0; b < buffer->mEntryTableSize; b++) {
        int guard = 0x2000;
        for (J3DPacket* packet = buffer->mpBuffer[b]; packet != nullptr; packet = packet->getNextPacket()) {
            if (packet == &s_depthPacket || --guard <= 0) return;
        }
    }
    for (int i = 0; i < 7; i++) {
        s_depthPacket.mModels[i] = models[i];
    }
    s_depthPacket.drawClear();
    const u16 index = buffer->mDrawMode == 1 ? static_cast<u16>(buffer->mEntryTableSize - 1) : 0;
    buffer->entryImm(&s_depthPacket, index);
}

s16 half_angle(s16 value) {
    return static_cast<s16>(value >> 1);
}

void hat_update(J3DModel* body, J3DModel* head, const cXyz& moved, s16 angle) {
    MtxP tip = head->getAnmMtx(kHatTipJoint);
    const cXyz headPos(tip[0][3], tip[1][3], tip[2][3]);
    cXyz facing;
    mDoMtx_multVecSR(body->getAnmMtx(kHeadJoint), &cXyz::BaseX, &facing);
    const s16 yaw = facing.atan2sX_Z();
    s16 pitch;
    if (cLib_distanceAngleS(yaw, angle) > 0x7000) {
        pitch = cM_atan2s(-facing.y, -facing.absXZ());
    } else {
        pitch = facing.atan2sY_XZ();
    }

    if (!s_hat.ready) {
        s_hat.prevPos = headPos;
        s_hat.prevPitch = pitch;
        s_hat.prevYaw = yaw;
        s_hat.ready = true;
        return;
    }

    cXyz drift = s_hat.prevPos - headPos;
    if (moved.x * moved.x + moved.z * moved.z < 1.0f) {
        drift.x = 0.0f;
        drift.z = 0.0f;
    }
    f32 sideX;
    f32 sideZ;
    const f32 facingXZ = facing.absXZ();
    if (facingXZ < 0.01f) {
        sideX = cM_ssin(angle);
        sideZ = cM_scos(angle);
    } else {
        sideX = facing.x / facingXZ;
        sideZ = facing.z / facingXZ;
    }

    s_hat.pitch[0] -= half_angle(static_cast<s16>(pitch - s_hat.prevPitch));
    const int absPitch = std::abs(static_cast<int>(pitch));
    if (!(absPitch > 0x3000 && absPitch < 0x5000)) {
        s_hat.yaw[0] -= half_angle(static_cast<s16>(yaw - s_hat.prevYaw));
    }
    drift.y += -2.0f;
    s_hat.prevPitch = pitch;
    s_hat.prevYaw = yaw;

    cXyz up;
    MtxP base = body->getAnmMtx(kHatBaseJoint);
    mDoMtx_multVecSR(base, &cXyz::BaseY, &up);
    const s16 limitAngle = base[1][0] < 0.0f ? cM_atan2s(-up.y, -up.absXZ()) : up.atan2sY_XZ();
    const int limit = limitAngle - 0x3800;

    const f32 along = drift.x * sideX + drift.z * sideZ;
    const s16 fall = cM_atan2s(drift.y, -along);
    int target = cLib_minMaxLimit<int>(fall - pitch, -0x3800, 0x3800) + pitch;
    if (target <= limit) target = limit;

    const s16 pitch0 = s_hat.pitch[0];
    const s16 yaw0 = s_hat.yaw[0];
    cLib_addCalcAngleS2(&s_hat.pitch[0], static_cast<s16>(target - pitch), 5, 0x400);
    s_hat.pitch[0] = static_cast<s16>(cLib_minMaxLimit<int>(s_hat.pitch[0] + s_hat.pitchSpeed[0], -0x3800, 0x3800));
    const s16 lean = static_cast<s16>(cLib_minMaxLimit<int>(
        cM_atan2s(-(drift.x * sideZ - drift.z * sideX), std::sqrt(drift.y * drift.y + along * along)), -0x2800,
        0x2800));
    cLib_addCalcAngleS2(&s_hat.yaw[0], lean, 5, 0x400);
    s_hat.yaw[0] = static_cast<s16>(cLib_minMaxLimit<int>(s_hat.yaw[0] + s_hat.yawSpeed[0], -0x2800, 0x2800));
    s_hat.pitchSpeed[0] = static_cast<s16>(0.2f * (s_hat.pitch[0] - pitch0));
    s_hat.yawSpeed[0] = static_cast<s16>(0.2f * (s_hat.yaw[0] - yaw0));

    int chain = s_hat.pitch[0] + pitch;
    s16 prevPitchDelta = static_cast<s16>(s_hat.pitch[0] - pitch0);
    s16 prevYawDelta = static_cast<s16>(s_hat.yaw[0] - yaw0);
    for (int i = 1; i < 3; i++) {
        s_hat.pitch[i] -= half_angle(prevPitchDelta);
        s_hat.yaw[i] -= half_angle(prevYawDelta);
        const s16 pitchBefore = s_hat.pitch[i];
        const s16 yawBefore = s_hat.yaw[i];
        cLib_addCalcAngleS2(&s_hat.pitch[i], 0, 5, 0x400);
        cLib_addCalcAngleS2(&s_hat.yaw[i], 0, 5, 0x400);
        int link = cLib_minMaxLimit<int>(s_hat.pitch[i] + s_hat.pitchSpeed[i], -0x1000, 0x1000);
        const int sum = static_cast<s16>(chain + link);
        if (limit > sum) {
            link = limit - chain;
            chain = limit;
        } else {
            chain = sum;
        }
        s_hat.pitch[i] = static_cast<s16>(link);
        const int yawLink = cLib_minMaxLimit<int>(s_hat.yaw[i] + s_hat.yawSpeed[i], -0x2000, 0x2000);
        s_hat.yaw[i] = static_cast<s16>(yawLink);
        s_hat.pitchSpeed[i] = static_cast<s16>(0.2f * (s_hat.pitch[i] - pitchBefore));
        s_hat.yawSpeed[i] = static_cast<s16>(0.2f * (s_hat.yaw[i] - yawBefore));
        prevPitchDelta = static_cast<s16>(s_hat.pitch[i] - pitchBefore);
        prevYawDelta = static_cast<s16>(s_hat.yaw[i] - yawBefore);
    }

    cXyz windPos = headPos;
    cXyz windDir(0.0f, 0.0f, 0.0f);
    f32 windPower = 0.0f;
    dKyw_get_AllWind_vec(&windPos, &windDir, &windPower);
    f32 strength = (25.0f * windPower * windPower + s_hat.prevPos.abs(headPos) * 0.65f) / 30.0f;
    if (strength > 1.0f) strength = 1.0f;
    const s16 step = static_cast<s16>(4060.0f * strength + 1500.0f);
    s_hat.phase = static_cast<s16>(s_hat.phase + step);
    s_hat.sway[0] = static_cast<s16>(strength * 728.0f * cM_scos(static_cast<s16>(s_hat.phase - 3 * step)));
    s_hat.sway[1] = static_cast<s16>(strength * 1456.0f * cM_scos(static_cast<s16>(s_hat.phase - 4 * step)));
    s_hat.sway[2] = static_cast<s16>(strength * 2184.0f * cM_scos(static_cast<s16>(s_hat.phase - 5 * step)));
    s_hat.prevPos = headPos;
}

int hat_joint_callback(J3DJoint* joint, int op) {
    if (op != 0) return 1;
    J3DModel* model = j3dSys.getModel();
    const u16 jnt = joint->getJntNo();
    if (model == nullptr || model != s_head || jnt < kHatFirstJoint || jnt > kHatLastJoint) return 1;
    s16 rotY;
    s16 rotZ;
    if (jnt <= kHatTipJoint) {
        rotY = half_angle(s_hat.yaw[0]);
        rotZ = half_angle(s_hat.pitch[0]);
        if (jnt == kHatTipJoint) rotZ = static_cast<s16>(rotZ + s_hat.sway[0]);
    } else {
        const int k = jnt - kHatTipJoint;
        rotY = s_hat.yaw[k];
        rotZ = static_cast<s16>(s_hat.pitch[k] + s_hat.sway[k]);
    }
    mDoMtx_stack_c::copy(J3DSys::mCurrentMtx);
    mDoMtx_stack_c::XYZrotM(0, rotY, rotZ);
    model->setAnmMtx(jnt, mDoMtx_stack_c::get());
    mDoMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    return 1;
}

int hands_joint_callback(J3DJoint* joint, int op) {
    if (op != 0) return 1;
    const u16 jnt = joint->getJntNo();
    if (jnt != 1 && jnt != 2) return 1;
    J3DModel* model = j3dSys.getModel();
    if (model == nullptr || model != s_hands || s_body == nullptr) return 1;
    MtxP hand = s_body->getAnmMtx(jnt == 1 ? kLeftHandJoint : kRightHandJoint);
    model->setAnmMtx(jnt, hand);
    mDoMtx_copy(hand, J3DSys::mCurrentMtx);
    return 1;
}

J3DModel* build_model(u8* copy, u32 size, u32 tag, u32 diffFlags, const char* label) {
    J3DModelData* data = dRes_info_c::loaderBasicBmd(tag, copy);
    if (data == nullptr || data->getMaterialNum() == 0 || data->getJointNum() == 0) {
        dl_log("[darklink] %s: model data invalid", label);
        return nullptr;
    }
    if (tag == kTagBmwr || tag == kTagBmwe) {
        dRes_info_c::offWarpMaterial(data);
    }
    if (!prepare_dark_materials(data)) {
        dl_log("[darklink] %s: material setup failed", label);
        return nullptr;
    }
    data->simpleCalcMaterial(0, const_cast<MtxP>(j3dDefaultMtx));
    data->makeSharedDL();
    if (!prepare_solid(data)) {
        dl_log("[darklink] %s: solid setup failed", label);
        return nullptr;
    }
    J3DModel* model = mDoExt_J3DModel__create(data, 0x80000, diffFlags);
    if (model == nullptr) {
        dl_log("[darklink] %s: model create failed", label);
        return nullptr;
    }
    dl_log("[darklink] %s: %u bytes, %d joints, %d materials", label, size,
           static_cast<int>(data->getJointNum()), static_cast<int>(data->getMaterialNum()));
    return model;
}

J3DModel* create_model_from_raw(JKRArchive* archive, void* raw, u32 tag, u32 diffFlags, const char* label) {
    if (archive == nullptr || raw == nullptr) return nullptr;
    u32 size = archive->getExpandedResSize(raw);
    const u8* rawBytes = static_cast<const u8*>(raw);
    const u32 headerSize = read_be32(rawBytes + 8);
    if (std::memcmp(rawBytes, "J3D2", 4) == 0 && headerSize > 0 && headerSize < 0x800000 &&
        (size == 0xFFFFFFFFu || headerSize > size)) {
        size = headerSize;
    }
    if (size == 0 || size == 0xFFFFFFFFu) {
        dl_log("[darklink] %s: bad resource size %u", label, size);
        return nullptr;
    }
    const u32 heapFree = static_cast<u32>(s_heap->getFreeSize());
    if (heapFree < model_budget(size)) {
        dl_log("[darklink] %s: %u bytes do not fit (heap free %u)", label, size, heapFree);
        return nullptr;
    }
    u8* copy = JKR_NEW_ARRAY_ARGS(u8, size, 0x20);
    if (copy == nullptr) {
        dl_log("[darklink] %s: no memory for %u bytes", label, size);
        return nullptr;
    }
    std::memcpy(copy, raw, size);
    return build_model(copy, size, tag, diffFlags, label);
}

u32 probe_model_size(JKRArchive* archive, u32 index) {
    alignas(32) u8 header[0x20];
    if (archive->readIdxResource(header, sizeof(header), index) < sizeof(header)) return 0;
    if (std::memcmp(header, "J3D2", 4) != 0) return 0;
    return read_be32(header + 8);
}

J3DModel* create_part_model(JKRArchive* archive, u32 index, u32 tag, const char* label, JKRExpHeap*& partHeap) {
    if (archive == nullptr || partHeap != nullptr) return nullptr;
    const u32 size = probe_model_size(archive, index);
    if (size < 0x20 || size > kMaxPartFileSize) {
        dl_log("[darklink] %s: bad resource size %u", label, size);
        return nullptr;
    }
    const u32 bufferSize = (size + 0x1F) & ~0x1Fu;
    const u32 heapSize = model_budget(bufferSize);
    const bool fitsPrivate = static_cast<u32>(s_heap->getFreeSize()) > heapSize + kModelHeadroom;
    JKRHeap* parent = fitsPrivate ? s_heap : pick_parent_heap(heapSize);
    JKRExpHeap* heap = parent != nullptr ? JKRExpHeap::create(heapSize, parent, false) : nullptr;
    if (heap == nullptr) {
        dl_log("[darklink] %s: no heap for %u bytes", label, size);
        return nullptr;
    }
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(heap);
    J3DModel* model = nullptr;
    u8* copy = JKR_NEW_ARRAY_ARGS(u8, bufferSize, 0x20);
    if (copy != nullptr && archive->readIdxResource(copy, bufferSize, index) >= size) {
        model = build_model(copy, size, tag, kEquipDiffFlags, label);
    } else {
        dl_log("[darklink] %s: read of %u bytes failed", label, size);
    }
    mDoExt_setCurrentHeap(oldHeap);
    if (model == nullptr) {
        mDoExt_destroyExpHeap(heap);
        return nullptr;
    }
    partHeap = heap;
    return model;
}

J3DModel* create_kmdl_model(const char* bmdName, u32 diffFlags) {
    return create_model_from_raw(s_archive, s_archive->getResource(kTagBmwr, bmdName), kTagBmwr, diffFlags,
                                 bmdName);
}

JKRArchive* mount_dvd_archive(const char* path) {
    return JKRArchive::mount(path, JKRArchive::MOUNT_DVD, s_heap, JKRArchive::MOUNT_DIRECTION_HEAD);
}

void forget_solid(J3DModelData* data) {
    int kept = 0;
    for (int i = 0; i < s_solidCount; i++) {
        if (s_solid[i].data != data) s_solid[kept++] = s_solid[i];
    }
    s_solidCount = kept;
}

void release_part(J3DModel*& model, JKRExpHeap*& heap) {
    if (model != nullptr) {
        forget_solid(model->getModelData());
        JKR_DELETE(model);
        model = nullptr;
    }
    if (heap != nullptr) {
        mDoExt_destroyExpHeap(heap);
        heap = nullptr;
    }
}

void release_equipment() {
    release_part(s_sword, s_swordHeap);
    release_part(s_sheath, s_sheathHeap);
    release_part(s_shield, s_shieldHeap);
    for (JKRArchive** archive : {&s_shieldArchive, &s_alinkArchive}) {
        if (*archive != nullptr) {
            (*archive)->unmount();
            *archive = nullptr;
        }
    }
}

void load_equipment() {
    const u8 sword = dComIfGs_getSelectEquipSword();
    const u8 shield = dComIfGs_getSelectEquipShield();
    s_loadedSword = sword;
    s_loadedShield = shield;

    if (sword == dItemNo_SWORD_e || sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e) {
        s_alinkArchive = mount_dvd_archive(kAlinkPath);
        if (sword == dItemNo_SWORD_e) {
            s_sword = create_part_model(s_alinkArchive, kOrdonSwordIndex, kTagBmwr, "ordon sword", s_swordHeap);
            s_sheath = create_part_model(s_alinkArchive, kOrdonSheathIndex, kTagBmwr, "ordon sheath", s_sheathHeap);
        } else {
            s_sword = create_part_model(s_alinkArchive, kMasterSwordIndex, kTagBmwe, "master sword", s_swordHeap);
            s_sheath = create_part_model(s_alinkArchive, kMasterSheathIndex, kTagBmwe, "master sheath", s_sheathHeap);
        }
    } else if (sword == dItemNo_WOOD_STICK_e) {
        s_sword = create_part_model(s_archive, kWoodSwordIndex, kTagBmwr, "wood sword", s_swordHeap);
    }

    const char* shieldPath = nullptr;
    if (shield == dItemNo_WOOD_SHIELD_e) {
        shieldPath = kWoodShieldPath;
    } else if (shield == dItemNo_SHIELD_e) {
        shieldPath = kOrdonShieldPath;
    } else if (shield == dItemNo_HYLIA_SHIELD_e) {
        shieldPath = kHylianShieldPath;
    }
    if (shieldPath != nullptr) {
        s_shieldArchive = mount_dvd_archive(shieldPath);
        s_shield = create_part_model(s_shieldArchive, kShieldIndex, kTagBmwr, "shield", s_shieldHeap);
    }

    if (s_sword != nullptr) {
        J3DModelData* data = s_sword->getModelData();
        const bool wood = sword == dItemNo_WOOD_STICK_e;
        const u16 index = wood ? 1 : 0;
        if (index < data->getMaterialNum()) {
            J3DShape* shape = data->getMaterialNodePointer(index)->getShape();
            if (shape != nullptr) {
                if (wood) {
                    shape->hide();
                } else {
                    shape->show();
                }
            }
        }
    }
}

void update_hand_grips(J3DModel* hands) {
    J3DModelData* data = hands->getModelData();
    for (u16 i = 0; i < data->getShapeNum(); i++) {
        data->getShapeNodePointer(i)->hide();
    }
    const u16 grip = s_sword != nullptr ? kSwordGripMaterial : kOpenHandMaterial;
    const u16 guard = s_shield != nullptr ? kShieldGripMaterial : kEmptyHandMaterial;
    for (u16 index : {grip, guard}) {
        if (J3DShape* shape = data->getMaterialNodePointer(index)->getShape()) shape->show();
    }
}

bool setup_hands(J3DModel* hands) {
    J3DModelData* data = hands->getModelData();
    if (data->getMaterialNum() < kHandsMinMaterials) return false;
    for (u16 j = 0; j < data->getJointNum(); j++) {
        data->getJointNodePointer(j)->setCallBack(hands_joint_callback);
    }
    update_hand_grips(hands);
    return true;
}

void reload_equipment() {
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(s_heap);
    release_equipment();
    load_equipment();
    mDoExt_setCurrentHeap(oldHeap);
    update_hand_grips(s_hands);
    dl_log("[darklink] equipment changed: sword=%d shield=%d heap free %u", s_sword != nullptr,
           s_shield != nullptr, static_cast<u32>(s_heap->getFreeSize()));
}

bool setup_hat(J3DModel* head) {
    J3DModelData* data = head->getModelData();
    if (data->getJointNum() <= kHatLastJoint) return false;
    for (u16 j = kHatFirstJoint; j <= kHatLastJoint; j++) {
        data->getJointNodePointer(j)->setCallBack(hat_joint_callback);
    }
    return true;
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

bool init_particles(const void* jpc) {
    if (jpc == nullptr) return false;
    JKRHeap* heap = JKRHeap::getCurrentHeap();
    JPAResourceManager* resources = JKR_NEW JPAResourceManager(jpc, heap);
    if (resources == nullptr || resources->getResource(kWarpResourceA) == nullptr ||
        resources->getResource(kWarpResourceB) == nullptr || resources->getResource(kAuraResource) == nullptr) {
        return false;
    }
    resources->swapTexture(mDoGph_gInf_c::getFrameBufferTimg(), "dummy");
    JPAEmitterManager* particles = JKR_NEW JPAEmitterManager(kParticleMax, kEmitterMax, heap, 1, 1);
    if (particles == nullptr) return false;
    particles->entryResourceManager(resources, 0);
    s_particles = particles;
    return true;
}

void update_aura() {
    for (int i = 0; i < kAuraCount; i++) {
        MtxP joint = s_body->getAnmMtx(kAuraJoints[i]);
        const JGeometry::TVec3<f32> pos(joint[0][3], joint[1][3], joint[2][3]);
        JPABaseEmitter*& emitter = s_aura[i];
        if (emitter == nullptr) {
            emitter = s_particles->createSimpleEmitterID(pos, kAuraResource, 0, 0, nullptr, nullptr);
            if (emitter == nullptr) continue;
            emitter->mStatus |= JPAEmtrStts_Immortal;
            emitter->mMaxFrame = 0;
            emitter->mRate = i == 0 ? kAuraHeadRate : kAuraRate;
            emitter->mLifeTime = kAuraLifeTime;
            emitter->mGlobalScl.set(kAuraScale, kAuraScale, kAuraScale);
            emitter->mGlobalPScl.set(kAuraScale, kAuraScale);
            emitter->mGlobalPrmClr.a = kAuraAlpha;
        }
        emitter->mGlobalTrs.set(pos.x, pos.y, pos.z);
    }
}

void release_particles() {
    if (s_particles != nullptr) {
        s_particles->forceDeleteAllEmitter();
        s_particles = nullptr;
    }
    for (JPABaseEmitter*& emitter : s_aura) {
        emitter = nullptr;
    }
}

void release_all() {
    if (s_mountCmd != nullptr || s_particleCmd != nullptr) return;
    release_particles();
    if (s_bck != nullptr) {
        JKR_DELETE(s_bck);
        s_bck = nullptr;
    }
    release_equipment();
    for (J3DModel** model : {&s_hands, &s_face, &s_head, &s_body}) {
        if (*model != nullptr) {
            JKR_DELETE(*model);
            *model = nullptr;
        }
    }
    s_solidCount = 0;
    s_hat = {};
    s_tevstrReady = false;
    for (J3DModel*& model : s_depthPacket.mModels) {
        model = nullptr;
    }
    if (s_archive != nullptr) {
        s_archive->unmount();
        s_archive = nullptr;
    }
    if (s_heap != nullptr) {
        mDoExt_destroyExpHeap(s_heap);
        s_heap = nullptr;
    }
}

bool build_models() {
    dl_log("[darklink] Kmdl mounted, heap free %u", static_cast<u32>(s_heap->getFreeSize()));
    JKRHeap* oldHeap = mDoExt_setCurrentHeap(s_heap);
    s_solidCount = 0;
    s_hat = {};
    s_body = create_kmdl_model("al.bmd", kBodyDiffFlags);
    s_head = create_kmdl_model("al_head.bmd", kBodyDiffFlags);
    s_face = create_kmdl_model("al_face.bmd", kBodyDiffFlags);
    s_hands = create_kmdl_model("al_hands.bmd", kBodyDiffFlags);
    load_equipment();
    s_bck = create_link_bck(kIdleWaitAnmIndex);
    mDoExt_setCurrentHeap(oldHeap);
    dl_log("[darklink] body=%d head=%d face=%d hands=%d sword=%d sheath=%d shield=%d bck=%d heap free %u",
           s_body != nullptr, s_head != nullptr, s_face != nullptr, s_hands != nullptr, s_sword != nullptr,
           s_sheath != nullptr, s_shield != nullptr, s_bck != nullptr, static_cast<u32>(s_heap->getFreeSize()));

    if (s_body == nullptr || s_head == nullptr || s_face == nullptr || s_hands == nullptr ||
        s_body->getModelData()->getJointNum() <= kShieldJoint) {
        return false;
    }
    if (!setup_hands(s_hands) || !setup_hat(s_head)) {
        dl_log("[darklink] hands or hat setup failed");
        return false;
    }
    return true;
}

bool update_loading() {
    switch (s_state) {
    case State::Ready:
        return true;
    case State::Failed:
        return false;
    case State::Idle: {
        JKRHeap* parent = pick_parent_heap(kHeapSize);
        s_heap = parent != nullptr ? JKRExpHeap::create(kHeapSize, parent, false) : nullptr;
        if (s_heap == nullptr) {
            s_state = State::Failed;
            return false;
        }
        s_mountCmd = mDoDvdThd_mountArchive_c::create(kKmdlPath, 0, s_heap);
        s_particleCmd = mDoDvdThd_toMainRam_c::create(kParticlePath, 0, s_heap);
        dl_log("[darklink] heap created, mounting Kmdl (cmd=%d, particles=%d)", s_mountCmd != nullptr,
               s_particleCmd != nullptr);
        s_state = State::Mounting;
        return false;
    }
    case State::Mounting: {
        if (s_mountCmd != nullptr && !s_mountCmd->sync()) return false;
        if (s_particleCmd != nullptr && !s_particleCmd->sync()) return false;
        void* jpc = nullptr;
        if (s_particleCmd != nullptr) {
            jpc = s_particleCmd->getMemAddress();
            s_particleCmd->destroy();
            s_particleCmd = nullptr;
        }
        if (s_mountCmd != nullptr) {
            s_archive = s_mountCmd->getArchive();
            s_mountCmd->destroy();
            s_mountCmd = nullptr;
        }
        s_state = State::Failed;
        if (s_archive == nullptr || !build_models()) {
            dl_log("[darklink] build failed (archive=%d)", s_archive != nullptr);
            release_all();
            s_state = State::Failed;
            return false;
        }
        JKRHeap* oldHeap = mDoExt_setCurrentHeap(s_heap);
        const bool particles = init_particles(jpc);
        mDoExt_setCurrentHeap(oldHeap);
        dl_log("[darklink] aura particles=%d heap free %u", particles, static_cast<u32>(s_heap->getFreeSize()));
        s_state = State::Ready;
        return true;
    }
    }
    return false;
}

void calc_equipment(J3DModel* model, u16 joint) {
    if (model == nullptr) return;
    model->setBaseTRMtx(s_body->getAnmMtx(joint));
    model->calc();
}

void calc_models(const cXyz& pos, s16 angleY) {
    mDoMtx_stack_c::transS(pos);
    mDoMtx_stack_c::YrotM(angleY);
    s_body->setBaseTRMtx(mDoMtx_stack_c::get());
    if (s_bck != nullptr) {
        s_bck->play();
        s_bck->entry(s_body->getModelData());
    }
    s_body->calc();
    if (s_hat.ready) {
        hat_update(s_body, s_head, cXyz::Zero, angleY);
    }
    s_head->setBaseTRMtx(s_body->getAnmMtx(kHeadJoint));
    s_head->calc();
    s_face->setBaseTRMtx(s_body->getAnmMtx(kHeadJoint));
    s_face->calc();
    if (!s_hat.ready) {
        hat_update(s_body, s_head, cXyz::Zero, angleY);
    }
    s_hands->setBaseTRMtx(s_body->getBaseTRMtx());
    s_hands->calc();
    calc_equipment(s_sword, kSwordJoint);
    calc_equipment(s_sheath, kSheathJoint);
    calc_equipment(s_shield, kShieldJoint);
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
    if (!update_loading()) return;

    dComIfGd_setList();
    if (!s_tevstrReady) {
        dKy_tevstr_init(&s_tevstr, dComIfGp_roomControl_getStayNo(), 0xFF);
        s_tevstrReady = true;
    }
    cXyz lightPos = pos;
    g_env_light.settingTevStruct(0, &lightPos, &s_tevstr);

    if (dComIfGs_getSelectEquipSword() != s_loadedSword || dComIfGs_getSelectEquipShield() != s_loadedShield) {
        reload_equipment();
    }

    calc_models(pos, angle.y);

    s_statueDrawTick = g_Counter.mCounter0;
    if (s_particles != nullptr && g_Counter.mCounter0 != s_particleTick) {
        s_particleTick = g_Counter.mCounter0;
        s_particles->calc(0);
        update_aura();
    }

    J3DModel* const models[7] = {s_body, s_head, s_face, s_hands, s_sword, s_sheath, s_shield};
    for (J3DModel* model : models) {
        if (model == nullptr) continue;
        J3DModelData* data = model->getModelData();
        g_env_light.setLightTevColorType_MAJI(data, &s_tevstr);
        apply_dark_light(data, &s_frameLight);
        refresh_solid(data);
        mDoExt_modelEntryDL(model);
    }
    submit_depth_packet(models);

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
DEFINE_HOOK(&dPa_control_c::draw, DarkLinkParticleDrawHook);

namespace {

HookAction on_particle_draw_pre(ModContext*, void* args, void*, void*) {
    if (s_particles == nullptr || s_particles->getEmitterNumber() <= 0) return HOOK_CONTINUE;
    if (mods::arg<u8>(args, 2) != kAuraDrawGroup) return HOOK_CONTINUE;
    if (g_Counter.mCounter0 - s_statueDrawTick > 2) return HOOK_CONTINUE;
    j3dSys.reinitGX();
    dKy_setLight_again();
    dKy_GxFog_set();
    s_particles->draw(mods::arg<JPADrawInfo*>(args, 1), 0);
    return HOOK_CONTINUE;
}

}

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

bool boss_rush_darklink_gear_locked() {
    return in_darklink_stage() && boss_rush_darklink_mod_installed() && find_darklink_actor() != nullptr;
}

namespace {

constexpr u8 kSwordPriority[] = {dItemNo_LIGHT_SWORD_e, dItemNo_MASTER_SWORD_e, dItemNo_SWORD_e,
                                 dItemNo_WOOD_STICK_e};
constexpr u8 kShieldPriority[] = {dItemNo_HYLIA_SHIELD_e, dItemNo_SHIELD_e, dItemNo_WOOD_SHIELD_e};

template <size_t N>
u8 best_owned(const u8 (&items)[N]) {
    for (u8 item : items) {
        if (dComIfGs_isItemFirstBit(item)) return item;
    }
    return dItemNo_NONE_e;
}

}

void update_boss_rush_darklink_gear_lock() {
    if (!boss_rush_darklink_gear_locked()) return;
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return;
    if (dComIfGs_getSelectEquipSword() == dItemNo_NONE_e) {
        const u8 sword = best_owned(kSwordPriority);
        if (sword != dItemNo_NONE_e) {
            dMeter2Info_setSword(sword, false);
            dl_log("[darklink] sword re-equipped for the fight (%d)", sword);
        }
    }
    if (dComIfGs_getSelectEquipShield() == dItemNo_NONE_e) {
        const u8 shield = best_owned(kShieldPriority);
        if (shield != dItemNo_NONE_e) {
            dMeter2Info_setShield(shield, false);
            link->setShieldChange();
            dl_log("[darklink] shield re-equipped for the fight (%d)", shield);
        }
    }
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
    mods::hook::add_pre<DarkLinkParticleDrawHook>(hook_svc, on_particle_draw_pre);
    return MOD_OK;
}
