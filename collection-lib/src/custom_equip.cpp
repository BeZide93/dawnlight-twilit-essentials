#include "collection_internal.hpp"

#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphAnimator/J3DMaterialAnm.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "JSystem/J3DGraphBase/J3DEnum.h"
#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "dolphin/gx/GXStruct.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "SSystem/SComponent/c_lib.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_stage.h"
#include "d/d_camera.h"
#include "d/d_resorce.h"
#include "res/Object/Always.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_manager.h"
#include "dolphin/pad.h"
#include "m_Do/m_Do_mtx.h"
#include "mods/svc/resource.h"
#include "mods/svc/save.h"
#include <cctype>
#include <cstring>
#include <string>

DEFINE_HOOK(&daAlink_c::draw, CeAlinkDrawHook);
DEFINE_HOOK(&daAlink_c::statusWindowDraw, CeAlinkSwDrawHook);
DEFINE_HOOK(&daAlink_c::modelDraw, CeModelDrawHook);
DEFINE_HOOK(&daAlink_c::basicModelDraw, CeBasicModelDrawHook);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, CeSetWaterDropColorHook);
DEFINE_HOOK(&dDlst_shadowControl_c::addReal, CeShadowAddRealHook);
DEFINE_HOOK(&daAlink_c::shadowDraw, CeAlinkShadowDrawHook);
DEFINE_HOOK(&dMenu_Collect3D_c::_create, CeCollect3DCreateHook);
DEFINE_HOOK(&dMenu_Collect3D_c::_delete, CeCollect3DDeleteHook);

DEFINE_HOOK(&daAlink_c::initStatusWindow, CeInitStatusWindowHook);

DEFINE_HOOK(&daAlink_c::warpModelTexScroll, CeWarpModelTexScrollHook);
DEFINE_HOOK(&daAlink_c::changeWarpMaterial, CeChangeWarpMaterialHook);

namespace {

constexpr int kMaxDefs = 32;

struct Entry {
    CustomEquipDef def;

    ResTIMG*       iconTex = nullptr;   // owned by the icon cache (collection_common.cpp)

    ResourceBuffer arcBuf  = RESOURCE_BUFFER_INIT;
    JKRArchive*    arc     = nullptr;
    bool           arcIsGame = false;
    J3DModel*      model       = nullptr;
    J3DModel*      sheathModel = nullptr;
    J3DModel*      hatModel    = nullptr;
    J3DModel*      faceModel   = nullptr;
    J3DModel*      handModel   = nullptr;
    bool           tried     = false;
    u8             tryCount  = 0;
};

Entry s_entries[kMaxDefs];
int   s_count = 0;

int s_activeId[3] = { -1, -1, -1 };
int s_equipDebounce = 0;
char s_cachedStage[16] = {};
Mtx s_lastBaseMtx[3];
bool s_hasLastBaseMtx[3] = { false, false, false };
bool s_linkModelIsWolf = false;

int s_tunicPosWatchFrames = 0;
cXyz s_tunicPosWatchLast = {};

csXyz s_tunicAngleWatchLast = {};
csXyz s_tunicShapeAngleWatchLast = {};

csXyz s_tunicCamAngleWatchLast = {};

cXyz s_tunicCamEyeWatchLast = {};
cXyz s_tunicCamCenterWatchLast = {};

int s_tunicPosClampFrames = 0;
constexpr f32 kTunicPosClampThreshold = 150.0f;

static J3DModel* s_originalLinkModel = nullptr;
static J3DModel* s_originalHatModel  = nullptr;
static J3DModel* s_originalFaceModel = nullptr;
static J3DModel* s_originalHandModel = nullptr;
static J3DShape* s_origShape_06d0 = nullptr;
static J3DShape* s_origShape_06d4 = nullptr;
static J3DShape* s_origShape_06d8 = nullptr;
static J3DShape* s_origShape_06dc = nullptr;
static J3DShape* s_origShape_06e0 = nullptr;
static J3DShape* s_origShape_06e8 = nullptr;
static J3DShape* s_origShape_06ec = nullptr;
static J3DShape* s_origShape_06f0 = nullptr;
static J3DShape* s_origShape_06e4 = nullptr;

inline s16 deg2s16(f32 d) { return static_cast<s16>(d * (65536.0f / 360.0f)); }

bool is_wolf(daAlink_c* a) {
    return a == nullptr || a->checkWolf() || a->checkWolfShapeReverse() || a->checkMetamorphose();
}

bool is_full_wolf(daAlink_c* a) {
    return a == nullptr || (a->checkWolf() && !a->checkMetamorphose());
}

daAlink_c* player() { return static_cast<daAlink_c*>(dComIfGp_getPlayer(0)); }

static bool is_title_or_menu() {
    daPy_py_c* p = daPy_getLinkPlayerActorClass();
    if (p == nullptr) return true;
    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr) {
    if (std::strcmp(stage, "F_SP102") == 0 ||
        std::strcmp(stage, "title") == 0 ||
        std::strcmp(stage, "opening") == 0 ||
        std::strcmp(stage, "name") == 0) {
        return true;
    }
    }
    return false;
}

static bool is_gameplay_ready() {
    if (is_title_or_menu()) return false;

    fpc_ProcID sceneId = dStage_roomControl_c::getProcID();
    if (sceneId == fpcM_ERROR_PROCESS_ID_e || fpcM_IsCreating(sceneId)) {
        return false;
    }

    daAlink_c* a = player();
    if (a == nullptr) return false;
    fpc_ProcID linkId = fopAcM_GetID(a);
    if (linkId == fpcM_ERROR_PROCESS_ID_e || fpcM_IsCreating(linkId)) {
        return false;
    }

    if (a->mpLinkModel == nullptr || a->mpLinkModel->getModelData() == nullptr) return false;
    if (a->mpLinkHatModel == nullptr || a->mpLinkFaceModel == nullptr || a->mpLinkHandModel == nullptr) return false;
    if (a->field_0x1f20 == nullptr || a->field_0x1f24 == nullptr) return false;
    if (a->field_0x2180[0] == nullptr || a->field_0x2180[1] == nullptr) return false;

    if (dComIfGp_isEnableNextStage()) return false;

    return true;
}

static bool str_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    for (; *haystack != '\0'; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h != '\0' && *n != '\0' &&
               std::tolower(static_cast<unsigned char>(*h)) == std::tolower(static_cast<unsigned char>(*n))) {
            h++;
            n++;
        }
        if (*n == '\0') return true;
    }
    return false;
}

static void* find_bmd_matching(JKRArchive* arc, const char* pattern) {
    if (!arc || !arc->mFiles || !arc->mStringTable) return nullptr;
    u32 num = arc->countFile();
    for (u32 i = 0; i < num; i++) {
        if (arc->mFiles[i].isDirectory()) continue;
        const char* name = arc->mStringTable + arc->mFiles[i].getNameOffset();
        if (name != nullptr && str_contains_ci(name, ".bmd") && str_contains_ci(name, pattern)) {
            return arc->fetchResource(&arc->mFiles[i], nullptr);
        }
    }
    return nullptr;
}

static bool arc_has_file_matching(JKRArchive* arc, const char* ext, const char* pattern) {
    if (!arc || !arc->mFiles || !arc->mStringTable) return false;
    u32 num = arc->countFile();
    for (u32 i = 0; i < num; i++) {
        if (arc->mFiles[i].isDirectory()) continue;
        const char* name = arc->mStringTable + arc->mFiles[i].getNameOffset();
        if (name != nullptr && str_contains_ci(name, ext) && str_contains_ci(name, pattern)) {
            return true;
        }
    }
    return false;
}

static void* find_body_bmd(JKRArchive* arc) {
    if (!arc || !arc->mFiles || !arc->mStringTable) return nullptr;
    u32 num = arc->countFile();
    for (u32 i = 0; i < num; i++) {
        if (arc->mFiles[i].isDirectory()) continue;
        const char* name = arc->mStringTable + arc->mFiles[i].getNameOffset();
        if (name != nullptr && str_contains_ci(name, ".bmd")) {
            if (!str_contains_ci(name, "head") &&
                !str_contains_ci(name, "face") &&
                !str_contains_ci(name, "hand") &&
                !str_contains_ci(name, "kantera") &&
                !str_contains_ci(name, "glow") &&
                !str_contains_ci(name, "boot") &&
                !str_contains_ci(name, "swb")) {
                return arc->fetchResource(&arc->mFiles[i], nullptr);
            }
        }
    }
    return nullptr;
}

J3DModel* vanilla_model(daAlink_c* a, CustomEquipKind kind) {
    if (!a) return nullptr;
    switch (kind) {
    case CE_SWORD:  return a->mSwordModel;
    case CE_SHIELD: return a->mShieldModel;
    default:        return nullptr;
    }
}

static void* get_arc_res(JKRArchive* arc, const char* name, u16 fallbackId = 0xFFFF) {
    if (!arc) return nullptr;
    void* res = nullptr;
    if (name && name[0] != '\0') {
        res = arc->getResource(0, name);
        if (!res) res = arc->getResource(0x424D4433, name);
        if (!res) res = arc->getResource(name);
    }
    if (!res && fallbackId != 0xFFFF) {
        res = arc->getResource(fallbackId);
        if (!res) res = arc->getIdxResource(fallbackId);
    }
    return res;
}

static const GXAttr kVtxBlockAttrOrder[13] = {
    GX_VA_POS, GX_VA_NRM, GX_VA_NBT, GX_VA_CLR0, GX_VA_CLR1,
    GX_VA_TEX0, GX_VA_TEX1, GX_VA_TEX2, GX_VA_TEX3,
    GX_VA_TEX4, GX_VA_TEX5, GX_VA_TEX6, GX_VA_TEX7,
};

static u32 be_swap_u32(u32 v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

static bool bmd_vertex_format_ok(const void* bmd) {
    const auto* fileData = static_cast<const J3DModelFileData*>(bmd);
    const J3DModelBlock* block = fileData->mBlocks;
    const u32 blockNum = fileData->mBlockNum;

    for (u32 i = 0; i < blockNum; i++) {
        if (block->mBlockType == 'VTX1') {
            const auto* vtx = reinterpret_cast<const J3DVertexBlock*>(block);
            const BE(u32)* attrPtrBase = &vtx->mpVtxPosArray;
            const u32 fmtListOffset = vtx->mpVtxAttrFmtList;
            if (fmtListOffset == 0) return false;
            const auto* rawFmtList = reinterpret_cast<const GXVtxAttrFmtList*>(
                reinterpret_cast<uintptr_t>(vtx) + fmtListOffset);

            for (int a = 0; a < 13; a++) {
                if (attrPtrBase[a] == 0) continue;

                bool found = false;
                for (const GXVtxAttrFmtList* raw = rawFmtList;; raw++) {
                    u32 rawAttr;
                    std::memcpy(&rawAttr, raw, sizeof(rawAttr));
                    const GXAttr attr = static_cast<GXAttr>(be_swap_u32(rawAttr));
                    if (attr == GX_VA_NULL) break;
                    if (attr == kVtxBlockAttrOrder[a]) { found = true; break; }
                }
                if (!found) return false;
            }
            return true;
        }
        block = reinterpret_cast<const J3DModelBlock*>(
            reinterpret_cast<uintptr_t>(block) + static_cast<u32>(block->mBlockSize));
    }
    return true;
}

static const J3DTexMtxInfo s_warpTexMtxInfo = {
    0x00,
    0x08, 0x00, 0x00,
    {0.5f, 0.5f, 0.0f},
    {0.1f, 0.1f, 0, 0.0f, 0.0f},
    {
        {0.5f, 0.0f, 0.0f, 0.5f},
        {0.0f, 0.5f, 0.0f, 0.5f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    },
};

static void safe_on_warp_material(J3DModelData* modelData) {
    if (modelData == nullptr) return;
    u16 matNum = modelData->getMaterialNum();
    for (u16 i = 0; i < matNum; ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        if (material == nullptr) continue;
        J3DTevBlock* tevBlock = material->getTevBlock();
        if (tevBlock == nullptr) continue;
        u8 tevStageNum = tevBlock->getTevStageNum();
        if (tevStageNum == 0) continue;
        J3DTevOrder* tevorder = tevBlock->getTevOrder(tevStageNum - 1);
        if (tevorder != nullptr && tevorder->getTexMap() == 3) {
            continue;
        }
        tevBlock->setTevStageNum(tevStageNum + 1);
        J3DTexGenBlock* texGenBlock = material->getTexGenBlock();
        if (texGenBlock != nullptr) {
            texGenBlock->setTexGenNum(texGenBlock->getTexGenNum() + 1);
        }
    }
}

static void safe_off_warp_material(J3DModelData* modelData) {
    if (modelData == nullptr) return;
    u16 matNum = modelData->getMaterialNum();
    for (u16 i = 0; i < matNum; ++i) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        if (material == nullptr) continue;
        J3DTevBlock* tevBlock = material->getTevBlock();
        if (tevBlock == nullptr) continue;
        u8 tevStageNum = tevBlock->getTevStageNum();
        if (tevStageNum <= 1) continue;
        J3DTevOrder* tevorder = tevBlock->getTevOrder(tevStageNum - 1);
        if (tevorder == nullptr || tevorder->getTexMap() != 3) {
            continue;
        }
        tevBlock->setTevStageNum(tevStageNum - 1);
        J3DTexGenBlock* texGenBlock = material->getTexGenBlock();
        if (texGenBlock != nullptr && texGenBlock->getTexGenNum() > 1) {
            texGenBlock->setTexGenNum(texGenBlock->getTexGenNum() - 1);
        }
    }
}

static void safe_apply_warp_srt(J3DModelData* modelData, const cXyz& pos, f32 transX, f32 transY) {
    if (modelData == nullptr) return;
    u16 matNum = modelData->getMaterialNum();
    if (matNum == 0) return;

    mDoMtx_stack_c::transS(-pos.x, -pos.y, -pos.z);
    camera_process_class* camera = dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
    if (camera == nullptr) {
        camera = dComIfGp_getCamera(0);
    }
    if (camera != nullptr) {
        mDoMtx_stack_c::YrotM(fopCamM_GetAngleY(camera));
    } else {
        mDoMtx_stack_c::YrotM(0);
    }

    J3DTexMtx* updatedMtxs[8] = {};
    int updatedCount = 0;

    for (u16 m = 0; m < matNum && m < 8; ++m) {
        J3DMaterial* mat = modelData->getMaterialNodePointer(m);
        if (mat == nullptr || mat->getTexGenBlock() == nullptr) continue;
        J3DTexGenBlock* texGen = mat->getTexGenBlock();
        u32 num = texGen->getTexGenNum();
        J3DTexMtx* warpTexMtx = nullptr;
        if (num > 0 && num <= 8) {
            warpTexMtx = texGen->getTexMtx(num - 1);
        }
        if (warpTexMtx == nullptr) {
            for (u32 i = 0; i < 8; ++i) {
                if (texGen->getTexMtx(i) != nullptr) {
                    warpTexMtx = texGen->getTexMtx(i);
                    break;
                }
            }
        }
        if (warpTexMtx == nullptr) continue;

        bool alreadyUpdated = false;
        for (int u = 0; u < updatedCount; ++u) {
            if (updatedMtxs[u] == warpTexMtx) {
                alreadyUpdated = true;
                break;
            }
        }
        if (alreadyUpdated) continue;
        if (updatedCount < 8) {
            updatedMtxs[updatedCount++] = warpTexMtx;
        }

        J3DTexMtxInfo& texMtxInfo = warpTexMtx->getTexMtxInfo();
        texMtxInfo.mSRT.mTranslationX = transX;
        texMtxInfo.mSRT.mTranslationY = transY;
        cMtx_concat(s_warpTexMtxInfo.mEffectMtx, mDoMtx_stack_c::get(), texMtxInfo.mEffectMtx);
    }
}

static J3DModel* load_single_bmd(void* bmd, u32 diffFlags = 0x11000084, bool isWarpModel = false) {
    if (!bmd) return nullptr;

    static const char* const kValidMagics[] = { "J3D2bmd3", "J3D2bdl4" };
    bool magicOk = false;
    for (const char* magic : kValidMagics) {
        if (std::memcmp(bmd, magic, 8) == 0) { magicOk = true; break; }
    }
    if (!magicOk) return nullptr;

    if (!bmd_vertex_format_ok(bmd)) return nullptr;

    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    JKRHeap* old = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;

    J3DModelData* data = nullptr;
    if (isWarpModel) {
        data = dRes_info_c::loaderBasicBmd('BMWR', bmd);
    }

    if (data == nullptr) {
        data = J3DModelLoaderDataBase::load(bmd, 0x59020010);
        if (!data || data->getMaterialNum() == 0) {
            if (old != nullptr) mDoExt_setCurrentHeap(old);
            return nullptr;
        }

        if (J3DTexture* tex = data->getTexture()) {
            const u16 texNum = tex->getNum();
            for (u16 i = 0; i < texNum; ++i) {
                ResTIMG* t = tex->getResTIMG(i);
                if (t == nullptr) continue;
                const bool degenerate = t->width == 0 || t->height == 0;
                const bool unsupported = !(t->format <= 6 || t->format == 14 ||
                                           (t->format >= 0x41 && t->format <= 0x4E));
                if (t->width == 0) t->width = 8;
                if (t->height == 0) t->height = 8;
                if (t->imageOffset == 0) t->imageOffset = 0x20;
                if (unsupported) t->format = 3;
                if (degenerate || unsupported) {
                    tex->setResTIMG(i, *t);
                }
            }
        }

        for (u16 i = 0; i < data->getMaterialNum(); i++) {
            J3DMaterial* mat = data->getMaterialNodePointer(i);
            mat->change();
            if (J3DMaterialAnm* anm = JKR_NEW J3DMaterialAnm()) {
                mat->setMaterialAnm(anm);
            }
        }

        if (data->newSharedDisplayList(J3DMdlFlag_UseSingleDL) == kJ3DError_Success) {
            data->simpleCalcMaterial(const_cast<MtxP>(j3dDefaultMtx));
            data->makeSharedDL();
        }
    }

    safe_on_warp_material(data);
    J3DModel* model = mDoExt_J3DModel__create(data, 0x80000, diffFlags | 0x2000400);
    safe_off_warp_material(data);

    if (old != nullptr) mDoExt_setCurrentHeap(old);
    return model;
}

static void note_load_fail(Entry& e) {
    if (e.arc != nullptr && !e.arcIsGame) { JKRUnmountArchive(e.arc); e.arc = nullptr; }
    e.model = e.sheathModel = e.hatModel = e.faceModel = e.handModel = nullptr;
    if (++e.tryCount >= 30) {
        e.tried = true;
    }
}

void load_model(Entry& e) {
    if (e.model != nullptr || e.tried) return;
    if (e.def.modelArc == nullptr || e.def.modelArc[0] == '\0') {
        e.tried = true;
        return;
    }

    const ResourceService* res = cl_resource_service();
    if (res == nullptr || g_modCtx == nullptr) return;

    if (e.arcBuf.data == nullptr && e.arc == nullptr) {

        if (res->load(g_modCtx, e.def.modelArc, &e.arcBuf) != MOD_OK) {
            e.arcBuf.data = nullptr;
            e.arcBuf.size = 0;
        }
    }

    JKRHeap* persistHeap = JKRHeap::getRootHeap();
    if (persistHeap == nullptr) persistHeap = static_cast<JKRHeap*>(mDoExt_getGameHeap());

    if (e.arcBuf.data == nullptr && e.arc == nullptr) {

        if (e.def.modelArc != nullptr && e.def.modelArc[0] != 0) {
            e.arc = JKRArchive::mount(e.def.modelArc, JKRArchive::MOUNT_COMP, persistHeap,
                                      JKRArchive::MOUNT_DIRECTION_HEAD);
            if (e.arc == nullptr && e.def.modelArc[0] != '/') {
                std::string withRoot = std::string("/") + e.def.modelArc;
                e.arc = JKRArchive::mount(withRoot.c_str(), JKRArchive::MOUNT_COMP, persistHeap,
                                          JKRArchive::MOUNT_DIRECTION_HEAD);
            }
            if (e.arc == nullptr) {
                std::string withRes = std::string("/res/") + e.def.modelArc;
                e.arc = JKRArchive::mount(withRes.c_str(), JKRArchive::MOUNT_COMP, persistHeap,
                                          JKRArchive::MOUNT_DIRECTION_HEAD);
            }

        }
    }

    if (e.arcBuf.data != nullptr) {
        if (e.arc == nullptr) {
            e.arc = JKRArchive::mount(e.arcBuf.data, persistHeap, JKRArchive::MOUNT_DIRECTION_HEAD);
            if (e.arc == nullptr) { note_load_fail(e); return; }
        }
    } else if (e.arc == nullptr) {

        char arcName[16] = {};
        const char* base = e.def.modelArc;
        for (const char* c = e.def.modelArc; *c != '\0'; c++) {
            if (*c == '/') base = c + 1;
        }
        for (u32 i = 0; base[i] != '\0' && base[i] != '.' && i + 1 < sizeof(arcName); i++) {
            arcName[i] = base[i];
        }
        if (arcName[0] != '\0') {
            dRes_info_c* info = dComIfG_getObjectResInfo(arcName);
            if (info != nullptr) {
                e.arc = info->getArchive();
                e.arcIsGame = (e.arc != nullptr);
            }
        }
        if (e.arc == nullptr) { note_load_fail(e); return; }
    }

    JKRHeap* old = mDoExt_setCurrentHeap(persistHeap);

    if (e.def.kind == CE_TUNIC) {
        void* hatBmd = find_bmd_matching(e.arc, "head");
        if (!hatBmd) hatBmd = find_bmd_matching(e.arc, "hat");
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "al_head.bmd", 0x0010);
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "_head.bmd");
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "head.bmd");
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "_hat.bmd");
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "hat.bmd");
        if (hatBmd) {
            e.hatModel = load_single_bmd(hatBmd, 0x11000084, true);
        }

        void* faceBmd = find_bmd_matching(e.arc, "face");
        if (!faceBmd) faceBmd = get_arc_res(e.arc, "al_face.bmd", 0x000E);
        if (!faceBmd) faceBmd = get_arc_res(e.arc, "_face.bmd");
        if (!faceBmd) faceBmd = get_arc_res(e.arc, "face.bmd");
        if (faceBmd) {
            e.faceModel = load_single_bmd(faceBmd, 0x11020284, true);
        }

        void* handBmd = find_bmd_matching(e.arc, "hand");
        if (!handBmd) handBmd = find_bmd_matching(e.arc, "hands");
        if (!handBmd) handBmd = get_arc_res(e.arc, "al_hands.bmd", 0x000F);
        if (!handBmd) handBmd = get_arc_res(e.arc, "_hands.bmd");
        if (!handBmd) handBmd = get_arc_res(e.arc, "hands.bmd");
        if (!handBmd) handBmd = get_arc_res(e.arc, "_hand.bmd");
        if (!handBmd) handBmd = get_arc_res(e.arc, "hand.bmd");
        if (!handBmd) handBmd = get_arc_res(e.arc, "al_hand.bmd");
        if (handBmd) {
            e.handModel = load_single_bmd(handBmd, 0x11000084, true);
        }

        void* bodyBmd = nullptr;
        if (e.def.modelFileId != 0xFFFF) {
            bodyBmd = e.arc->getResource(static_cast<u16>(e.def.modelFileId));
            if (!bodyBmd) bodyBmd = e.arc->getIdxResource(e.def.modelFileId);
        }
        if (!bodyBmd) {
            bodyBmd = find_body_bmd(e.arc);
        }
        if (!bodyBmd) {
            bodyBmd = get_arc_res(e.arc, "al.bmd");
        }
        if (bodyBmd) {
            e.model = load_single_bmd(bodyBmd, 0x11000084, true);
        }
    } else {
        void* bmd = nullptr;
        if (e.def.modelFileId != 0xFFFF) {
            bmd = e.arc->getResource(static_cast<u16>(e.def.modelFileId));
            if (bmd == nullptr) bmd = e.arc->getIdxResource(e.def.modelFileId);
        }
        if (bmd != nullptr) {
            e.model = load_single_bmd(bmd, 0x11000084, true);
        }

        if (e.def.kind == CE_SWORD && e.def.sheathFileId != 0xFFFF) {
            void* sBmd = e.arc->getResource(static_cast<u16>(e.def.sheathFileId));
            if (sBmd == nullptr) sBmd = e.arc->getIdxResource(e.def.sheathFileId);
            if (sBmd != nullptr) {
                e.sheathModel = load_single_bmd(sBmd, 0x11000084, true);
            }
        }
    }

    mDoExt_setCurrentHeap(old);

    if (e.model == nullptr) {
        note_load_fail(e);
        return;
    }

    e.tried = true;
    e.tryCount = 0;
}

static bool s_customEquipSuppressed = false;

Entry* active_entry(CustomEquipKind kind) {
    if (s_customEquipSuppressed) return nullptr;
    int id = s_activeId[kind];
    return (id >= 0 && id < s_count) ? &s_entries[id] : nullptr;
}

HookAction on_alink_model_draw_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* a = args ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    J3DModel*  m = args ? mods::arg<J3DModel*>(args, 1) : nullptr;
    if (!a || !m || s_linkModelIsWolf || is_full_wolf(a)) return HOOK_CONTINUE;

    if (active_entry(CE_SWORD) != nullptr) {
        if (m == a->mSwordModel || m == a->mSheathModel) {
            return HOOK_SKIP_ORIGINAL;
        }
    }
    if (active_entry(CE_SHIELD) != nullptr) {
        if (m == a->mShieldModel) {
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

static void update_custom_model_matrix(Entry* e, CustomEquipKind kind, daAlink_c* a) {
    if (!e || !e->model || !a) return;

    J3DModel* vm = vanilla_model(a, kind);
    MtxP m = (vm != nullptr) ? vm->getBaseTRMtx() : nullptr;

    if (m != nullptr) {
        mDoMtx_copy(m, s_lastBaseMtx[kind]);
        s_hasLastBaseMtx[kind] = true;
    } else if (s_hasLastBaseMtx[kind]) {
        m = s_lastBaseMtx[kind];
    } else if (a->mpLinkModel != nullptr) {
        if (kind == CE_SHIELD) {
            mDoMtx_stack_c::copy(a->mpLinkModel->getAnmMtx(a->field_0x30b6));
            mDoMtx_stack_c::transM(4.2f, -4.4f, -20.0f);
            mDoMtx_stack_c::XYZrotM(deg2s16(91.0f), deg2s16(57.0f), deg2s16(180.0f));
            mDoMtx_copy(mDoMtx_stack_c::get(), s_lastBaseMtx[kind]);
            s_hasLastBaseMtx[kind] = true;
            m = s_lastBaseMtx[kind];
        } else if (kind == CE_SWORD) {
            mDoMtx_stack_c::copy(a->mpLinkModel->getAnmMtx(a->field_0x30b6));
            mDoMtx_stack_c::transM(-18.5f, 0.14f, 12.2f);
            mDoMtx_stack_c::XYZrotM(0, deg2s16(33.1f), 0);
            mDoMtx_copy(mDoMtx_stack_c::get(), s_lastBaseMtx[kind]);
            s_hasLastBaseMtx[kind] = true;
            m = s_lastBaseMtx[kind];
        }
    }
    if (m == nullptr) return;

    mDoMtx_stack_c::copy(m);
    mDoMtx_stack_c::transM(e->def.offX, e->def.offY, e->def.offZ);
    mDoMtx_stack_c::XYZrotM(deg2s16(e->def.rotX), deg2s16(e->def.rotY), deg2s16(e->def.rotZ));
    const f32 s = e->def.scale;
    e->model->setBaseScale(cXyz(s, s, s));
    e->model->setBaseTRMtx(mDoMtx_stack_c::get());
    e->model->calc();

    if (kind == CE_SWORD && e->sheathModel != nullptr) {
        MtxP sm = (a->mSheathModel != nullptr) ? a->mSheathModel->getBaseTRMtx() : nullptr;
        if (sm != nullptr) {
            mDoMtx_stack_c::copy(sm);
        } else if (a->mpLinkModel != nullptr) {
            mDoMtx_stack_c::copy(a->mpLinkModel->getAnmMtx(a->field_0x30b6));
        } else {
            return;
        }
        mDoMtx_stack_c::transM(e->def.offX, e->def.offY, e->def.offZ);
        mDoMtx_stack_c::XYZrotM(deg2s16(e->def.rotX), deg2s16(e->def.rotY), deg2s16(e->def.rotZ));
        e->sheathModel->setBaseScale(cXyz(s, s, s));
        e->sheathModel->setBaseTRMtx(mDoMtx_stack_c::get());
        e->sheathModel->calc();
    }
}

static bool is_warp_visual(daAlink_c* a) {
    if (!a) return false;
    u16 proc = a->mProcID;
    return proc == daAlink_c::PROC_DUNGEON_WARP_READY ||
           proc == daAlink_c::PROC_DUNGEON_WARP ||
           proc == daAlink_c::PROC_DUNGEON_WARP_SCN_START ||
           proc == daAlink_c::PROC_WARP ||
           proc == daAlink_c::PROC_TW_GATE;
}

DEFINE_HOOK(&PADSetColor, CePadSetColorHook);

static HookAction on_pad_set_color_pre(ModContext*, void* args, void*, void*) {
    const int id = custom_equip_active_id(CE_TUNIC);
    if (id < 0) return HOOK_CONTINUE;
    const CustomEquipDef* d = custom_equip_get(id);
    if (d == nullptr || d->padColor == 0xFFFFFFFFu) return HOOK_CONTINUE;

    mods::arg_ref<u8>(args, 1) = static_cast<u8>((d->padColor >> 16) & 0xFF);
    mods::arg_ref<u8>(args, 2) = static_cast<u8>((d->padColor >> 8) & 0xFF);
    mods::arg_ref<u8>(args, 3) = static_cast<u8>(d->padColor & 0xFF);
    return HOOK_CONTINUE;
}

void on_alink_draw_post(ModContext*, void*, void*, void*) {
    daAlink_c* a = player();
    if (!a || s_linkModelIsWolf || is_full_wolf(a) || is_warp_visual(a)) return;

    if (a->checkPlayerNoDraw()) return;

    for (int k = 0; k < 3; k++) {
        CustomEquipKind kind = static_cast<CustomEquipKind>(k);
        if (kind == CE_TUNIC) continue;
        Entry* e = active_entry(kind);
        if (!e || !e->model) continue;

        update_custom_model_matrix(e, kind, a);

        g_env_light.settingTevStruct_colget_player(&a->tevStr);
        g_env_light.setLightTevColorType_MAJI(e->model, &a->tevStr);
        mDoExt_modelUpdateDL(e->model);

        if (kind == CE_SWORD && e->sheathModel != nullptr) {
            g_env_light.settingTevStruct_colget_player(&a->tevStr);
            g_env_light.setLightTevColorType_MAJI(e->sheathModel, &a->tevStr);
            mDoExt_modelUpdateDL(e->sheathModel);
        }
    }
}

HookAction on_add_real_shadow_pre(ModContext*, void* args, void* ret, void*) {
    if (!args) return HOOK_CONTINUE;
    dDlst_shadowControl_c* self = mods::arg<dDlst_shadowControl_c*>(args, 0);
    u32 key = mods::arg<u32>(args, 1);
    J3DModel* model = mods::arg<J3DModel*>(args, 2);
    daAlink_c* a = player();
    if (!a || !model || !self || s_linkModelIsWolf || is_full_wolf(a)) return HOOK_CONTINUE;

    Entry* swordEntry = active_entry(CE_SWORD);
    if (swordEntry != nullptr) {
        if (model == a->mSwordModel) {
            bool r = false;
            if (swordEntry->model != nullptr) {
                update_custom_model_matrix(swordEntry, CE_SWORD, a);
                r = self->addReal(key, swordEntry->model);
            }
            if (ret) *(bool*)ret = r;
            return HOOK_SKIP_ORIGINAL;
        }
        if (model == a->mSheathModel) {
            bool r = false;
            if (swordEntry->sheathModel != nullptr) {
                update_custom_model_matrix(swordEntry, CE_SWORD, a);
                r = self->addReal(key, swordEntry->sheathModel);
            }
            if (ret) *(bool*)ret = r;
            return HOOK_SKIP_ORIGINAL;
        }
    }

    Entry* shieldEntry = active_entry(CE_SHIELD);
    if (shieldEntry != nullptr) {
        if (model == a->mShieldModel) {
            bool r = false;
            if (shieldEntry->model != nullptr) {
                update_custom_model_matrix(shieldEntry, CE_SHIELD, a);
                r = self->addReal(key, shieldEntry->model);
            }
            if (ret) *(bool*)ret = r;
            return HOOK_SKIP_ORIGINAL;
        }
    }

    return HOOK_CONTINUE;
}

static J3DModel* s_shadowStash[3] = { nullptr, nullptr, nullptr };

static u32 ce_link_shadow_id(daAlink_c* a) {
    return static_cast<u32>(a->field_0x31a4);
}

HookAction on_alink_shadow_draw_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* a = args ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    s_shadowStash[0] = s_shadowStash[1] = s_shadowStash[2] = nullptr;
    if (!a || s_linkModelIsWolf || is_full_wolf(a)) return HOOK_CONTINUE;

    if (active_entry(CE_SWORD) != nullptr) {
        s_shadowStash[0] = a->mSwordModel;  a->mSwordModel  = nullptr;
        s_shadowStash[1] = a->mSheathModel; a->mSheathModel = nullptr;
    }
    if (active_entry(CE_SHIELD) != nullptr) {
        s_shadowStash[2] = a->mShieldModel; a->mShieldModel = nullptr;
    }
    return HOOK_CONTINUE;
}

void on_alink_shadow_draw_post(ModContext*, void* args, void*, void*) {
    daAlink_c* a = args ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    if (!a) return;

    if (s_shadowStash[0] != nullptr) a->mSwordModel  = s_shadowStash[0];
    if (s_shadowStash[1] != nullptr) a->mSheathModel = s_shadowStash[1];
    if (s_shadowStash[2] != nullptr) a->mShieldModel = s_shadowStash[2];
    s_shadowStash[0] = s_shadowStash[1] = s_shadowStash[2] = nullptr;

    if (s_linkModelIsWolf || is_full_wolf(a)) return;

    const u32 sid = ce_link_shadow_id(a);
    if (sid == 0) return;

    Entry* sw = active_entry(CE_SWORD);
    if (sw != nullptr && sw->model != nullptr && a->checkSwordDraw()) {
        update_custom_model_matrix(sw, CE_SWORD, a);
        dComIfGd_addRealShadow(sid, sw->model);
        if (sw->sheathModel != nullptr) dComIfGd_addRealShadow(sid, sw->sheathModel);
    }
    Entry* sh = active_entry(CE_SHIELD);
    if (sh != nullptr && sh->model != nullptr && a->checkShieldDraw()) {
        update_custom_model_matrix(sh, CE_SHIELD, a);
        dComIfGd_addRealShadow(sid, sh->model);
    }
}

HookAction on_set_water_drop_color_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* a = args ? mods::arg<daAlink_c*>(args, 0) : nullptr;
    const J3DGXColorS10* i_color = args ? mods::arg<const J3DGXColorS10*>(args, 1) : nullptr;
    if (!a || !i_color) return HOOK_CONTINUE;

    if (active_entry(CE_TUNIC) != nullptr) {
        J3DModelData* bodyData = (a->field_0x064C != nullptr) ? a->field_0x064C :
                                 (a->mpLinkModel != nullptr ? a->mpLinkModel->getModelData() : nullptr);
        J3DModelData* hatData  = (a->mpLinkHatModel != nullptr) ? a->mpLinkHatModel->getModelData() : nullptr;

        if (bodyData != nullptr) {
            u16 num = bodyData->getMaterialNum();
            const u16 bodyIndices[] = {17, 9, 0, 1, 2, 16, 15, 14};
            for (u16 idx : bodyIndices) {
                if (idx < num) {
                    bodyData->getMaterialNodePointer(idx)->setTevColor(1, i_color);
                }
            }
        }
        if (hatData != nullptr) {
            u16 num = hatData->getMaterialNum();
            if (num > 0) {
                hatData->getMaterialNodePointer(0)->setTevColor(1, i_color);
            }
            if (num > 1) {
                hatData->getMaterialNodePointer(1)->setTevColor(1, i_color);
            }
        }
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

}

static void retarget_face_material_anims(daAlink_c* a);

// Hide the body's boots while the Iron Boots are on (what changeLink() does for a new body).
static void apply_heavy_boots_to_feet(daAlink_c* a) {
    if (a->field_0x06e0 == nullptr) return;
    if (a->checkEquipHeavyBoots()) {
        a->field_0x06e0->hide();
        if (a->field_0x06e4 != nullptr) a->field_0x06e4->hide();
    } else {
        a->field_0x06e0->show();
    }
}

// Put Link's vanilla body, hat, face and hands back after a custom tunic swap.
static void restore_original_link_models(daAlink_c* a) {
    a->mpLinkModel     = s_originalLinkModel;
    a->mpLinkHatModel  = s_originalHatModel;
    a->mpLinkFaceModel = s_originalFaceModel;
    a->mpLinkHandModel = s_originalHandModel;
    a->field_0x06d0    = s_origShape_06d0;
    a->field_0x06d4    = s_origShape_06d4;
    a->field_0x06d8    = s_origShape_06d8;
    a->field_0x06dc    = s_origShape_06dc;
    a->field_0x06e0    = s_origShape_06e0;
    a->field_0x06e8    = s_origShape_06e8;
    a->field_0x06ec    = s_origShape_06ec;
    a->field_0x06f0    = s_origShape_06f0;
    a->field_0x06e4    = s_origShape_06e4;
    // The Iron Boots may have been put on while the custom body was worn.
    apply_heavy_boots_to_feet(a);

    a->mpLinkModel->setUserArea((uintptr_t)a);
    if (a->mpLinkHatModel) a->mpLinkHatModel->setUserArea((uintptr_t)a);

    retarget_face_material_anims(a);
    a->changeModelDataDirect(1);

    a->mEyeHL1.remove();
    if (a->mpLinkFaceModel != nullptr && a->mpLinkFaceModel->getModelData() != nullptr) {
        a->mEyeHL1.entry(a->mpLinkFaceModel->getModelData(), "highlight02");
    }
}

struct CustomEquipSaveBlob {
    u8 shieldItem = 0;
    u8 swordItem  = 0;
    u8 tunicItem  = 0;
    u8 padding    = 0;
};

static void save_custom_equip_state(CustomEquipKind kind, u8 item) {
    if (g_saveSvc != nullptr && g_modCtx != nullptr) {
        CustomEquipSaveBlob blob{};
        size_t size = sizeof(blob);
        g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size);
        if (kind == CE_SHIELD)      blob.shieldItem = item;
        else if (kind == CE_SWORD)  blob.swordItem  = item;
        else if (kind == CE_TUNIC)  blob.tunicItem  = item;
        g_saveSvc->set_blob(g_modCtx, "custom_equip", &blob, sizeof(blob));
        return;
    }

    // Without the save service: spare bytes of the vanilla player status.
    if (!is_title_or_menu()) {
        dSv_player_status_a_c& st = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();
        if (kind == CE_SHIELD) {
            st.setSelectEquip(5, item);
        } else if (kind == CE_SWORD) {
            st.unk31[0] = item;
        } else if (kind == CE_TUNIC) {
            st.unk31[1] = item;
        }
    }
}

static u8 saved_custom_item(CustomEquipKind kind) {
    if (g_saveSvc != nullptr && g_modCtx != nullptr) {
        CustomEquipSaveBlob blob{};
        size_t size = sizeof(blob);
        if (g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size) != MOD_OK) return 0;
        return kind == CE_SHIELD ? blob.shieldItem : kind == CE_SWORD ? blob.swordItem : blob.tunicItem;
    }
    dSv_player_status_a_c& st = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();
    return kind == CE_SHIELD ? st.getSelectEquip(5) : kind == CE_SWORD ? st.unk31[0] : st.unk31[1];
}

static bool s_restoredFromSave = false;

static bool s_prevEquipWasActive[3] = { false, false, false };
static bool s_healPending = false;
static int s_healAttemptsLeft = 0;

static int s_framesSinceUpdateStart = 0;
constexpr int kModelLoadReloadSettleFrames = 1;

static int find_entry(CustomEquipKind kind, u8 item) {
    for (int i = 0; i < s_count; i++) {
        if (s_entries[i].def.kind == kind && s_entries[i].def.item == item) return i;
    }
    return -1;
}

// Best vanilla item of a kind the player owns (backing for a custom item without base).
static u8 best_owned(CustomEquipKind kind) {
    if (kind == CE_SWORD) {
        if (dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e)) return dItemNo_LIGHT_SWORD_e;
        if (dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e)) return dItemNo_MASTER_SWORD_e;
        if (dComIfGs_isItemFirstBit(dItemNo_SWORD_e)) return dItemNo_SWORD_e;
        if (dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e)) return dItemNo_WOOD_STICK_e;
        return dItemNo_SWORD_e;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_HYLIA_SHIELD_e)) return dItemNo_HYLIA_SHIELD_e;
    if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) return dItemNo_SHIELD_e;
    return dItemNo_WOOD_SHIELD_e;
}

void custom_equip_restore_from_save() {
    if (s_customEquipSuppressed) return;
    if (!is_gameplay_ready()) return;

    if (s_count == 0) {
        collectionlib_run_slot_registration();
    }

    for (int k = 0; k < 3; k++) {
        const CustomEquipKind kind = static_cast<CustomEquipKind>(k);
        if (s_activeId[kind] >= 0) continue;
        const u8 item = saved_custom_item(kind);
        if (item == 0) continue;
        const int i = find_entry(kind, item);
        if (i < 0 || !custom_equip_has_model(i)) continue;

        s_activeId[kind] = i;
        if (kind == CE_SHIELD) {
            if (dComIfGs_getSelectEquipShield() == dItemNo_NONE_e) dMeter2Info_setShield(best_owned(kind), false);
        } else if (kind == CE_SWORD) {
            if (dComIfGs_getSelectEquipSword() == dItemNo_NONE_e) dMeter2Info_setSword(best_owned(kind), false);
        } else {
            const u8 base = custom_equip_resolved_base(s_entries[i].def);
            dMeter2Info_setCloth(base, false);
            dComIfGs_setSelectEquipClothes(base);
        }
    }
}

// Unequip a slot and get its models off Link right away (not next frame), so its archive
// can be freed.
static void detach_entry(int id) {
    const CustomEquipKind kind = s_entries[id].def.kind;
    if (s_activeId[kind] != id) return;
    custom_equip_clear(kind);
    daAlink_c* a = player();
    if (kind == CE_TUNIC && a != nullptr && s_originalLinkModel != nullptr &&
        a->mpLinkModel == s_entries[id].model) {
        restore_original_link_models(a);
        s_originalLinkModel = nullptr;
        s_originalHatModel  = nullptr;
        s_originalFaceModel = nullptr;
        s_originalHandModel = nullptr;
    }
}

static void release_entry(Entry& e) {
    const ResourceService* res = cl_resource_service();
    if (e.arc != nullptr && !e.arcIsGame) JKRUnmountArchive(e.arc);
    if (res != nullptr && g_modCtx != nullptr) res->free(g_modCtx, &e.arcBuf);
    e.arc = nullptr;
    e.arcIsGame = false;
    e.model = e.sheathModel = e.hatModel = e.faceModel = e.handModel = nullptr;
    e.tried = false;
    e.tryCount = 0;
}

void custom_equip_remove(int id) {
    if (id < 0 || id >= s_count) return;

    detach_entry(id);
    release_entry(s_entries[id]);
    for (int i = id; i < s_count - 1; i++) s_entries[i] = s_entries[i + 1];
    s_entries[s_count - 1] = Entry{};
    s_count--;

    for (int k = 0; k < 3; k++) {
        if (s_activeId[k] > id) s_activeId[k]--;
    }
    layout_on_custom_removed(id);
}

// Registration is idempotent per (kind, column): a slot registered again keeps its loaded
// model, icon and equipped state, so rebuilding the Collection screen costs nothing.
int custom_equip_upsert(const CustomEquipDef& def) {
    const int existing = find_entry(def.kind, def.item);
    if (existing >= 0) {
        Entry& e = s_entries[existing];
        const bool sameModel =
            e.def.modelFileId == def.modelFileId && e.def.sheathFileId == def.sheathFileId &&
            ((e.def.modelArc == nullptr && def.modelArc == nullptr) ||
             (e.def.modelArc != nullptr && def.modelArc != nullptr && std::strcmp(e.def.modelArc, def.modelArc) == 0));
        if (!sameModel) {
            detach_entry(existing);
            release_entry(e);
        }
        const bool sameIcon = e.def.iconArcFileId.fileId == def.iconArcFileId.fileId &&
            ((e.def.iconBti == nullptr && def.iconBti == nullptr) ||
             (e.def.iconBti != nullptr && def.iconBti != nullptr && std::strcmp(e.def.iconBti, def.iconBti) == 0));
        if (!sameIcon) e.iconTex = nullptr;
        e.def = def;
        return existing;
    }

    if (s_count >= kMaxDefs) {
        log_collect_info("collection-lib: more than %d custom slots, '%s' not added", kMaxDefs,
                         def.name ? def.name : "?");
        return -1;
    }
    const int id = s_count++;
    s_entries[id] = Entry{};
    s_entries[id].def = def;
    return id;
}

int custom_equip_count() { return s_count; }

const CustomEquipDef* custom_equip_get(int id) {
    return (id >= 0 && id < s_count) ? &s_entries[id].def : nullptr;
}

static void refresh_sword_model(daAlink_c* pl) {
    if (pl == nullptr) return;

    const u16 prevEquip   = pl->mEquipItem;
    const u16 prevPending = pl->field_0x2fde;
    const bool wasDrawn   = (prevEquip == 0x103);

    pl->setSwordModel();
    pl->setItemMatrix(0);

    if (!wasDrawn) {
        pl->offSwordModel();
        pl->mEquipItem   = prevEquip;
        pl->field_0x2fde = prevPending;
    }
}

void custom_equip_activate(int id) {
    // Slots without a model are their vanilla base item; nothing to activate.
    if (id < 0 || id >= s_count || !custom_equip_has_model(id)) return;
    CustomEquipKind kind = s_entries[id].def.kind;
    s_activeId[kind] = id;

    if (s_entries[id].model == nullptr) {
        s_entries[id].tried = false;
        s_entries[id].tryCount = 0;
    }
    load_model(s_entries[id]);
    save_custom_equip_state(kind, s_entries[id].def.item);

    daAlink_c* pl = player();
    if (pl && is_gameplay_ready()) {
        if (kind == CE_SWORD) {
            refresh_sword_model(pl);
        } else if (kind == CE_SHIELD) {
            pl->setShieldModel();
            pl->setItemMatrix(0);
        }
    }

    if (kind == CE_SWORD) {
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* draw = (meter != nullptr) ? meter->getMeterDrawPtr() : nullptr;
        if (draw != nullptr) {
            draw->changeTextureItemB(dComIfGs_getSelectEquipSword());
        }
    }
}

void custom_equip_clear(CustomEquipKind kind) {
    s_activeId[kind] = -1;
    save_custom_equip_state(kind, 0);

    daAlink_c* pl = player();
    if (pl && is_gameplay_ready()) {
        if (kind == CE_SWORD) {
            refresh_sword_model(pl);
        } else if (kind == CE_SHIELD) {
            pl->setShieldModel();
            pl->setItemMatrix(0);
        }
    }

    if (kind == CE_SWORD) {
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* draw = (meter != nullptr) ? meter->getMeterDrawPtr() : nullptr;
        if (draw != nullptr) {
            draw->changeTextureItemB(dComIfGs_getSelectEquipSword());
        }
    }
}

void custom_equip_deactivate(CustomEquipKind kind) {
    s_activeId[kind] = -1;

    daAlink_c* pl = player();
    if (pl && is_gameplay_ready()) {
        if (kind == CE_SWORD) {
            refresh_sword_model(pl);
        } else if (kind == CE_SHIELD) {
            pl->setShieldModel();
            pl->setItemMatrix(0);
        }
    }

    if (kind == CE_SWORD) {
        dMeter2_c* meter = g_meter2_info.getMeterClass();
        dMeter2Draw_c* draw = (meter != nullptr) ? meter->getMeterDrawPtr() : nullptr;
        if (draw != nullptr) {
            draw->changeTextureItemB(dComIfGs_getSelectEquipSword());
        }
    }
}

void custom_equip_set_suppressed(bool suppressed) {
    s_customEquipSuppressed = suppressed;
    if (suppressed) {
        daAlink_c* pl = player();
        if (pl && is_gameplay_ready()) {
            if (s_originalLinkModel != nullptr && pl->mpLinkModel != s_originalLinkModel) {
                restore_original_link_models(pl);
                s_originalLinkModel = nullptr;
                s_originalHatModel  = nullptr;
                s_originalFaceModel = nullptr;
                s_originalHandModel = nullptr;
            }
            pl->setShieldModel();
            pl->setItemMatrix(0);
            refresh_sword_model(pl);
        }
    }
}

bool custom_equip_is_suppressed() {
    return s_customEquipSuppressed;
}

bool custom_equip_active(CustomEquipKind kind) {
    if (s_customEquipSuppressed) return false;
    return s_activeId[kind] >= 0;
}

int custom_equip_active_id(CustomEquipKind kind) {
    if (s_customEquipSuppressed) return -1;
    return s_activeId[kind];
}

static bool is_kind_item(CustomEquipKind kind, u8 item) {
    switch (kind) {
    case CE_SWORD:
        return item == dItemNo_WOOD_STICK_e || item == dItemNo_SWORD_e || item == dItemNo_MASTER_SWORD_e ||
               item == dItemNo_LIGHT_SWORD_e;
    case CE_SHIELD:
        return item == dItemNo_WOOD_SHIELD_e || item == dItemNo_SHIELD_e || item == dItemNo_HYLIA_SHIELD_e;
    default:
        return item == dItemNo_WEAR_CASUAL_e || item == dItemNo_WEAR_KOKIRI_e || item == dItemNo_WEAR_ZORA_e ||
               item == dItemNo_ARMOR_e;
    }
}

u8 custom_equip_resolved_base(const CustomEquipDef& def) {
    const u8 base = def.baseItem;
    if (def.kind == CE_TUNIC) {
        // Clothes need a body to graft onto; the Hero's Clothes are the historic default.
        return is_kind_item(CE_TUNIC, base) ? base : static_cast<u8>(dItemNo_WEAR_KOKIRI_e);
    }
    if (!is_kind_item(def.kind, base)) return dItemNo_NONE_e;
    // Like the native Master Sword cell: once it is the Light Sword, that one.
    if (base == dItemNo_MASTER_SWORD_e && dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e)) {
        return dItemNo_LIGHT_SWORD_e;
    }
    return dComIfGs_isItemFirstBit(base) ? base : static_cast<u8>(dItemNo_NONE_e);
}

static u8 current_vanilla(CustomEquipKind kind) {
    if (kind == CE_SWORD) return dComIfGs_getSelectEquipSword();
    if (kind == CE_SHIELD) return dComIfGs_getSelectEquipShield();
    return dComIfGs_getSelectEquipClothes();
}

static void set_vanilla_equip(CustomEquipKind kind, u8 item, bool playerChange) {
    if (kind == CE_SWORD) {
        dMeter2Info_setSword(item, false);
    } else if (kind == CE_SHIELD) {
        dMeter2Info_setShield(item, false);
        if (playerChange) {
            if (daAlink_c* a = daAlink_getAlinkActorClass()) a->setShieldChange();
        }
    } else {
        dMeter2Info_setCloth(item, false);
        dComIfGs_setSelectEquipClothes(item);
        if (playerChange) {
            if (daPy_py_c* pl = daPy_getPlayerActorClass()) pl->setClothesChange(0);
        }
    }
}

static void equip_sound(bool equipped) {
    Z2GetAudioMgr()->seStart(equipped ? Z2SE_SY_ITEM_SET_X : Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f,
                             1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibration();
}

bool custom_equip_has_model(int id) {
    const CustomEquipDef* d = custom_equip_get(id);
    return d != nullptr && d->modelArc != nullptr && d->modelArc[0] != '\0';
}

bool custom_equip_unlocked(int id) {
    const CustomEquipDef* d = custom_equip_get(id);
    if (d == nullptr) return false;
    return d->unlocked != nullptr ? d->unlocked() : true;
}

bool custom_equip_equipped(int id) {
    const CustomEquipDef* d = custom_equip_get(id);
    if (d == nullptr || s_customEquipSuppressed) return false;
    if (custom_equip_has_model(id)) return s_activeId[d->kind] == id;
    // A slot without a model is its base item.
    const u8 base = custom_equip_resolved_base(*d);
    return s_activeId[d->kind] < 0 && base != dItemNo_NONE_e && current_vanilla(d->kind) == base;
}

bool custom_equip_toggle(int id) {
    if (id < 0 || id >= s_count || s_equipDebounce > 0) return false;
    const CustomEquipDef& d = s_entries[id].def;
    const CustomEquipKind kind = d.kind;
    const u8 base = custom_equip_resolved_base(d);

    if (custom_equip_equipped(id)) {
        // Native: A on the worn item does nothing. Clothes can never be taken off.
        if (!cl_unequip_enabled() || kind == CE_TUNIC) return false;
        s_equipDebounce = 8;
        if (s_activeId[kind] == id) custom_equip_clear(kind);
        set_vanilla_equip(kind, dItemNo_NONE_e, true);
        if (kind == CE_SWORD) refresh_sword_model(player());
        equip_sound(false);
        return true;
    }

    s_equipDebounce = 8;
    if (!custom_equip_has_model(id)) {
        if (base == dItemNo_NONE_e) return false;
        if (s_activeId[kind] >= 0) custom_equip_clear(kind);
        set_vanilla_equip(kind, base, true);
        equip_sound(true);
        return true;
    }

    if (kind != CE_TUNIC) {
        // The vanilla item underneath first: activation rebuilds the models from it.
        u8 underneath = base;
        if (underneath == dItemNo_NONE_e && current_vanilla(kind) == dItemNo_NONE_e) underneath = best_owned(kind);
        if (underneath != dItemNo_NONE_e && underneath != current_vanilla(kind)) {
            set_vanilla_equip(kind, underneath, false);
        }
    }
    custom_equip_activate(id);
    if (kind == CE_TUNIC) set_vanilla_equip(kind, base, false);
    equip_sound(true);
    return true;
}

ResTIMG* custom_equip_icon(int id) {
    if (id < 0 || id >= s_count) return nullptr;
    Entry& e = s_entries[id];
    if (e.iconTex == nullptr) e.iconTex = cl_load_icon(e.def.iconBti, e.def.iconArcFileId);
    return e.iconTex;
}

static void on_custom_equip_save_loaded(ModContext*, uint32_t, void*) {
    s_activeId[0] = s_activeId[1] = s_activeId[2] = -1;
    s_restoredFromSave = false;
    custom_equip_restore_from_save();
    s_restoredFromSave = true;
}

static void on_custom_equip_new_save(ModContext*, uint32_t, void*) {
    s_activeId[0] = s_activeId[1] = s_activeId[2] = -1;
    s_restoredFromSave = true;
    save_custom_equip_state(CE_SHIELD, 0);
    save_custom_equip_state(CE_SWORD, 0);
    save_custom_equip_state(CE_TUNIC, 0);
}

static void safe_on_warp_entry(const Entry& e) {
    if (e.model != nullptr) safe_on_warp_material(e.model->getModelData());
    if (e.sheathModel != nullptr) safe_on_warp_material(e.sheathModel->getModelData());
    if (e.hatModel != nullptr) safe_on_warp_material(e.hatModel->getModelData());
    if (e.faceModel != nullptr) safe_on_warp_material(e.faceModel->getModelData());
    if (e.handModel != nullptr) safe_on_warp_material(e.handModel->getModelData());
}

static void safe_off_warp_entry(const Entry& e) {
    if (e.model != nullptr) safe_off_warp_material(e.model->getModelData());
    if (e.sheathModel != nullptr) safe_off_warp_material(e.sheathModel->getModelData());
    if (e.hatModel != nullptr) safe_off_warp_material(e.hatModel->getModelData());
    if (e.faceModel != nullptr) safe_off_warp_material(e.faceModel->getModelData());
    if (e.handModel != nullptr) safe_off_warp_material(e.handModel->getModelData());
}

static void safe_apply_warp_srt_entry(const Entry& e, const cXyz& pos, f32 transX, f32 transY) {
    if (e.model != nullptr) safe_apply_warp_srt(e.model->getModelData(), pos, transX, transY);
    if (e.sheathModel != nullptr) safe_apply_warp_srt(e.sheathModel->getModelData(), pos, transX, transY);
    if (e.hatModel != nullptr) safe_apply_warp_srt(e.hatModel->getModelData(), pos, transX, transY);
    if (e.faceModel != nullptr) safe_apply_warp_srt(e.faceModel->getModelData(), pos, transX, transY);
    if (e.handModel != nullptr) safe_apply_warp_srt(e.handModel->getModelData(), pos, transX, transY);
}

static void on_warp_model_tex_scroll_replace(ModContext*, void* args, void* ret, void*) {
    if (!args || !ret) return;
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    int* rv = static_cast<int*>(ret);
    *rv = 0;
    if (self == nullptr) return;

    self->field_0x3478 += 0.15f;
    if (self->field_0x3478 >= 1.0f) {
        self->field_0x3478 -= 1.0f;
    }

    *rv = cLib_chaseF(&self->field_0x347c, self->field_0x3480, 0.06f);
    self->field_0x3484 = cLib_minMaxLimit<f32>(0.5f * self->field_0x347c, 0.0f, 1.0f);

    safe_apply_warp_srt(self->field_0x064C, self->current.pos, self->field_0x3478, self->field_0x347c);
    if (self->mSwordModel != nullptr) safe_apply_warp_srt(self->mSwordModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
    if (self->mShieldModel != nullptr) safe_apply_warp_srt(self->mShieldModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
    if (self->mSheathModel != nullptr) safe_apply_warp_srt(self->mSheathModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);

    if (self->checkWolf()) {
        if (self->mpWlChainModels[0] != nullptr) safe_apply_warp_srt(self->mpWlChainModels[0]->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
    } else {
        if (self->mpLinkFaceModel != nullptr) safe_apply_warp_srt(self->mpLinkFaceModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
        if (self->mpLinkHatModel != nullptr) safe_apply_warp_srt(self->mpLinkHatModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
        if (self->mpLinkHandModel != nullptr) safe_apply_warp_srt(self->mpLinkHandModel->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
        if (self->mpLinkBootModels[0] != nullptr) safe_apply_warp_srt(self->mpLinkBootModels[0]->getModelData(), self->current.pos, self->field_0x3478, self->field_0x347c);
    }

    for (int i = 0; i < kMaxDefs; ++i) {
        safe_apply_warp_srt_entry(s_entries[i], self->current.pos, self->field_0x3478, self->field_0x347c);
    }
}

static void on_change_warp_material_replace(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    int matMode = mods::arg<int>(args, 1);
    if (self == nullptr) return;

    if (matMode == daAlink_c::WARP_MAT_MODE_0) {
        safe_on_warp_material(self->field_0x064C);
        if (self->mSwordModel != nullptr) safe_on_warp_material(self->mSwordModel->getModelData());
        if (self->mShieldModel != nullptr) safe_on_warp_material(self->mShieldModel->getModelData());
        if (self->mSheathModel != nullptr) safe_on_warp_material(self->mSheathModel->getModelData());

        if (self->checkWolf()) {
            if (self->mpWlChainModels[0] != nullptr) safe_on_warp_material(self->mpWlChainModels[0]->getModelData());
        } else {
            if (self->mpLinkFaceModel != nullptr) safe_on_warp_material(self->mpLinkFaceModel->getModelData());
            if (self->mpLinkHatModel != nullptr) safe_on_warp_material(self->mpLinkHatModel->getModelData());
            if (self->mpLinkHandModel != nullptr) safe_on_warp_material(self->mpLinkHandModel->getModelData());
            if (self->mpLinkBootModels[0] != nullptr) safe_on_warp_material(self->mpLinkBootModels[0]->getModelData());
        }

        for (int i = 0; i < kMaxDefs; ++i) {
            safe_on_warp_entry(s_entries[i]);
        }
    } else {
        safe_off_warp_material(self->field_0x064C);
        if (self->mSwordModel != nullptr) safe_off_warp_material(self->mSwordModel->getModelData());
        if (self->mShieldModel != nullptr) safe_off_warp_material(self->mShieldModel->getModelData());
        if (self->mSheathModel != nullptr) safe_off_warp_material(self->mSheathModel->getModelData());

        if (self->checkWolf()) {
            if (self->mpWlChainModels[0] != nullptr) safe_off_warp_material(self->mpWlChainModels[0]->getModelData());
        } else {
            if (self->mpLinkFaceModel != nullptr) safe_off_warp_material(self->mpLinkFaceModel->getModelData());
            if (self->mpLinkHatModel != nullptr) safe_off_warp_material(self->mpLinkHatModel->getModelData());
            if (self->mpLinkHandModel != nullptr) safe_off_warp_material(self->mpLinkHandModel->getModelData());
            if (self->mpLinkBootModels[0] != nullptr) safe_off_warp_material(self->mpLinkBootModels[0]->getModelData());
        }

        for (int i = 0; i < kMaxDefs; ++i) {
            safe_off_warp_entry(s_entries[i]);
        }

        for (int i = 0; i < 6; i++) {
            JPABaseEmitter* emitterp = dComIfGp_particle_getEmitter(self->field_0x3240[i]);
            if (emitterp != nullptr) {
                emitterp->stopDrawParticle();
            }
        }
    }
}

HookAction on_collect_3d_create_pre(ModContext*, void*, void*, void*);
HookAction on_collect_3d_delete_pre(ModContext*, void*, void*, void*);
HookAction on_alink_init_status_window_pre(ModContext*, void*, void*, void*);

void custom_equip_init_hooks(const HookService* hook_svc, const SaveService* save_svc) {
    if (save_svc != nullptr && g_modCtx != nullptr) {
        save_svc->observe_saves(g_modCtx, on_custom_equip_new_save, on_custom_equip_save_loaded, nullptr, nullptr, nullptr);
    }
    if (!hook_svc) return;
    CL_HOOK_PRE(CeModelDrawHook, on_alink_model_draw_pre);
    CL_HOOK_PRE(CeBasicModelDrawHook, on_alink_model_draw_pre);
    CL_HOOK_POST(CeAlinkDrawHook, on_alink_draw_post);
    CL_HOOK_POST(CeAlinkSwDrawHook, on_alink_draw_post);
    CL_HOOK_PRE(CeSetWaterDropColorHook, on_set_water_drop_color_pre);
    CL_HOOK_PRE(CeShadowAddRealHook, on_add_real_shadow_pre);
    CL_HOOK_PRE(CeAlinkShadowDrawHook, on_alink_shadow_draw_pre);
    CL_HOOK_PRE(CePadSetColorHook, on_pad_set_color_pre);
    CL_HOOK_POST(CeAlinkShadowDrawHook, on_alink_shadow_draw_post);
    CL_HOOK_PRE(CeCollect3DCreateHook, on_collect_3d_create_pre);
    CL_HOOK_PRE(CeCollect3DDeleteHook, on_collect_3d_delete_pre);
    CL_HOOK_PRE(CeInitStatusWindowHook, on_alink_init_status_window_pre);


    CL_HOOK_REPLACE(CeWarpModelTexScrollHook, on_warp_model_tex_scroll_replace);
    CL_HOOK_REPLACE(CeChangeWarpMaterialHook, on_change_warp_material_replace);
}

static void on_stage_changed() {
    s_hasLastBaseMtx[0] = s_hasLastBaseMtx[1] = s_hasLastBaseMtx[2] = false;
    s_originalLinkModel = nullptr;
    s_originalHatModel  = nullptr;
    s_originalFaceModel = nullptr;
    s_originalHandModel = nullptr;
}

template <typename AnmT>
static void safe_search_update_material_id(AnmT* anm, J3DModelData* faceData) {
    if (anm == nullptr || faceData == nullptr) {
        return;
    }
    JUTNameTab* dstNames = faceData->getMaterialName();
    if (dstNames == nullptr) {
        return;
    }
    if (anm->mUpdateMaterialID == nullptr) {
        return;
    }
    if (anm->mUpdateMaterialName.getResNameTable() == nullptr) {
        return;
    }
    const u16 num = anm->getUpdateMaterialNum();
    if (num == 0 || num > 64) {
        return;
    }
    const u16 dstNum = faceData->getMaterialNum();
    for (u16 i = 0; i < num; i++) {
        const char* name = anm->mUpdateMaterialName.getName(i);
        s32 idx = (name != nullptr) ? dstNames->getIndex(name) : -1;
        anm->mUpdateMaterialID[i] =
            (idx >= 0 && idx < dstNum) ? static_cast<u16>(idx) : static_cast<u16>(0xFFFF);
    }
}

static void retarget_face_material_anims(daAlink_c* a) {
    if (a == nullptr || a->mpLinkFaceModel == nullptr) {
        return;
    }
    J3DModelData* faceData = a->mpLinkFaceModel->getModelData();
    if (faceData == nullptr || faceData->getMaterialNum() <= 0) {
        return;
    }
    if (a->field_0x2180[0] != nullptr && a->field_0x2180[1] != nullptr && faceData->getMaterialNum() > 3) {
        faceData->getMaterialNodePointer(2)->setMaterialAnm(a->field_0x2180[0]);
        faceData->getMaterialNodePointer(3)->setMaterialAnm(a->field_0x2180[1]);
    }
    if (a->mpFaceBtp != nullptr) {
        safe_search_update_material_id(a->mpFaceBtp, faceData);
    }
    if (a->mpFaceBtk != nullptr) {
        safe_search_update_material_id(a->mpFaceBtk, faceData);
    }
}

static bool s_dollSwapActive = false;

static void doll_session_watchdog() {
    if (s_dollSwapActive && s_currentCollect2D == nullptr) {
        s_dollSwapActive = false;
    }
}

void custom_equip_menu_doll_begin() {
    s_dollSwapActive = false;
    daAlink_c* a = player();
    if (a == nullptr) return;
    if (is_wolf(a)) return;

    if (s_originalLinkModel == nullptr) return;
    if (a->mpLinkModel == s_originalLinkModel) return;

    if (a->mpLinkModel == nullptr || a->mpLinkHatModel == nullptr ||
        a->mpLinkFaceModel == nullptr || a->mpLinkHandModel == nullptr) {
        return;
    }
    if (s_originalLinkModel == nullptr || s_originalHatModel == nullptr ||
        s_originalFaceModel == nullptr || s_originalHandModel == nullptr) {
        return;
    }

    restore_original_link_models(a);
    s_dollSwapActive = true;
}

void custom_equip_menu_doll_end() {
    s_dollSwapActive = false;
}

HookAction on_collect_3d_create_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_begin();
    return HOOK_CONTINUE;
}

HookAction on_collect_3d_delete_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_end();
    return HOOK_CONTINUE;
}

HookAction on_alink_init_status_window_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_begin();
    return HOOK_CONTINUE;
}

static void custom_equip_apply(daAlink_c* a, bool duringRebuild = false);

void custom_equip_update() {

    if (s_framesSinceUpdateStart < kModelLoadReloadSettleFrames) s_framesSinceUpdateStart++;

    if (s_equipDebounce > 0) s_equipDebounce--;

    if (is_title_or_menu()) {
        s_activeId[0] = s_activeId[1] = s_activeId[2] = -1;
        s_restoredFromSave = false;
        s_prevEquipWasActive[0] = s_prevEquipWasActive[1] = s_prevEquipWasActive[2] = false;
        s_dollSwapActive = false;
        return;
    }

    if (s_customEquipSuppressed) {
        s_healPending = false;
        return;
    }

    doll_session_watchdog();

    for (int k = 0; k < 3; k++) {
        if (s_prevEquipWasActive[k] && s_activeId[k] < 0) {
            s_healPending = true;
            s_healAttemptsLeft = 10;
        }
    }
    s_prevEquipWasActive[0] = s_activeId[0] >= 0;
    s_prevEquipWasActive[1] = s_activeId[1] >= 0;
    s_prevEquipWasActive[2] = s_activeId[2] >= 0;

    if (s_healPending) {
        if (is_gameplay_ready()) {
            custom_equip_restore_from_save();
            s_healPending = false;
        } else if (--s_healAttemptsLeft <= 0) {
            s_healPending = false;
        }
    }

    const char* stage = dComIfGp_getStartStageName();
    if (s_cachedStage[0] == '\0') {
        if (stage != nullptr) {
            std::strncpy(s_cachedStage, stage, sizeof(s_cachedStage) - 1);
            s_cachedStage[sizeof(s_cachedStage) - 1] = '\0';
        }
    } else if (stage != nullptr && std::strncmp(stage, s_cachedStage, sizeof(s_cachedStage) - 1) != 0) {
        std::strncpy(s_cachedStage, stage, sizeof(s_cachedStage) - 1);
        s_cachedStage[sizeof(s_cachedStage) - 1] = '\0';
        on_stage_changed();
    }

    if (!is_gameplay_ready()) {
        return;
    }

    custom_equip_apply(player());

    if (s_tunicPosWatchFrames > 0) {
        daAlink_c* a = player();
        if (a != nullptr) {
            cXyz p = a->current.pos;

            if (s_tunicPosClampFrames > 0) {
                const f32 dx = p.x - s_tunicPosWatchLast.x;
                const f32 dy = p.y - s_tunicPosWatchLast.y;
                const f32 dz = p.z - s_tunicPosWatchLast.z;
                const bool jumpDetectedThisFrame = dx * dx + dy * dy + dz * dz > kTunicPosClampThreshold * kTunicPosClampThreshold;
                if (jumpDetectedThisFrame) {

                    a->current.pos = s_tunicPosWatchLast;
                    a->current.angle = s_tunicAngleWatchLast;
                    a->shape_angle = s_tunicShapeAngleWatchLast;
                    a->speed.x = a->speed.y = a->speed.z = 0.0f;
                    a->speedF = 0.0f;
                    p = s_tunicPosWatchLast;

                    const f32 sinYaw = cM_ssin(a->shape_angle.y);
                    const f32 cosYaw = cM_scos(a->shape_angle.y);

                    cXyz diff = s_tunicCamEyeWatchLast - s_tunicCamCenterWatchLast;
                    f32 dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);
                    if (dist < 100.0f || dist > 1000.0f) {
                        dist = 300.0f;
                    }
                    f32 heightOffset = diff.y;
                    if (heightOffset < -50.0f || heightOffset > 300.0f) {
                        heightOffset = 0.0f;
                    }

                    cXyz targetCenter = a->current.pos;
                    targetCenter.y += 130.0f;

                    cXyz targetEye = targetCenter;
                    targetEye.x -= sinYaw * dist;
                    targetEye.z -= cosYaw * dist;
                    targetEye.y += heightOffset;

                    s_tunicCamCenterWatchLast = targetCenter;
                    s_tunicCamEyeWatchLast = targetEye;

                    if (dCamera_c* dcam = dCam_getBody()) {
                        dcam->Reset(targetCenter, targetEye);
                    }
                    if (camera_process_class* cam = dComIfGp_getCamera(0)) {
                        cam->view.lookat.eye = targetEye;
                        cam->view.lookat.center = targetCenter;
                    }
                }
                s_tunicPosClampFrames--;
            }

            if (p.x != s_tunicPosWatchLast.x || p.y != s_tunicPosWatchLast.y || p.z != s_tunicPosWatchLast.z) {
                s_tunicPosWatchLast = p;
            }
            s_tunicAngleWatchLast = a->current.angle;
            s_tunicShapeAngleWatchLast = a->shape_angle;

            camera_process_class* cam = dComIfGp_getCamera(0);
            if (cam != nullptr) {
                const csXyz camAngle = cam->angle;
                if (camAngle.x != s_tunicCamAngleWatchLast.x || camAngle.y != s_tunicCamAngleWatchLast.y ||
                    camAngle.z != s_tunicCamAngleWatchLast.z) {
                    s_tunicCamAngleWatchLast = camAngle;
                }

                if (s_tunicPosClampFrames == 0) {
                    if (dCamera_c* dcam = dCam_getBody()) {
                        s_tunicCamEyeWatchLast = dcam->Eye();
                        s_tunicCamCenterWatchLast = dcam->Center();
                    } else {
                        s_tunicCamEyeWatchLast = cam->view.lookat.eye;
                        s_tunicCamCenterWatchLast = cam->view.lookat.center;
                    }
                }
            }
        }
        s_tunicPosWatchFrames--;
    }
}

// Link's code shows and hides parts of the body model through shape pointers: the hands
// (swapped for held items), the boots (under the Iron Boots), belt, earring and the ear (the
// helmet on the Zora Armor). Which material holds which part differs per body model
// (changeLink() in d_a_alink_wolf.inc); the material names are shared by Link's models, so
// a custom body is mapped by name.
static J3DShape* body_shape(J3DModelData* data, const char* suffix) {
    JUTNameTab* names = data->getMaterialName();
    if (names == nullptr) return nullptr;
    const size_t suffixLen = std::strlen(suffix);
    for (u16 i = 0; i < data->getMaterialNum(); i++) {
        const char* name = names->getName(i);
        if (name == nullptr) continue;
        const size_t len = std::strlen(name);
        if (len >= suffixLen && std::strcmp(name + len - suffixLen, suffix) == 0) {
            return data->getMaterialNodePointer(i)->getShape();
        }
    }
    return nullptr;
}

static void map_body_shapes(daAlink_c* a, const CustomEquipDef& def) {
    J3DModelData* data = a->field_0x064C;
    if (data == nullptr) return;

    J3DShape* handL = body_shape(data, "_handLA_m");
    J3DShape* handR = body_shape(data, "_handRA_m");
    if (handL != nullptr && handR != nullptr) {
        J3DShape* boots = body_shape(data, "_bootsA_m");
        if (boots == nullptr) boots = body_shape(data, "_boots_m");
        J3DShape* ear = body_shape(data, "_mask_m");
        if (ear == nullptr) ear = body_shape(data, "_ear_m");

        a->field_0x06d8 = handL;
        a->field_0x06dc = handR;
        a->field_0x06e0 = boots;
        a->field_0x06e4 = body_shape(data, "_bootsB_m");
        a->field_0x06e8 = body_shape(data, "_earring_m");
        a->field_0x06ec = body_shape(data, "_beltS_m");
        a->field_0x06f0 = ear;
        if (a->field_0x06e4 != nullptr) a->field_0x06e4->hide();
        // The Hero's Clothes body carries a skirt the game never shows.
        if (J3DShape* skirt = body_shape(data, "_skirt_m")) skirt->hide();
    } else if (data->getMaterialNum() > 16) {
        // Unknown material names: assume the Hero's Clothes layout.
        data->getMaterialNodePointer(16)->getShape()->hide();
        a->field_0x06d8 = data->getMaterialNodePointer(11)->getShape();
        a->field_0x06dc = data->getMaterialNodePointer(12)->getShape();
        a->field_0x06e0 = data->getMaterialNodePointer(6)->getShape();
        a->field_0x06e4 = nullptr;
        a->field_0x06e8 = data->getMaterialNodePointer(8)->getShape();
        a->field_0x06ec = data->getMaterialNodePointer(4)->getShape();
        a->field_0x06f0 = data->getMaterialNodePointer(7)->getShape();
    }

    if (!def.ironBootsHideFeet && a->field_0x06e0 != nullptr) {
        // setHeavyBoots() toggles the boots through this pointer only; without it they stay.
        a->field_0x06e0->show();
        a->field_0x06e0 = nullptr;
    }
    apply_heavy_boots_to_feet(a);
    a->field_0x06d0 = a->field_0x06d8;
    a->field_0x06d4 = a->field_0x06dc;
}

static void custom_equip_apply(daAlink_c* a, bool duringRebuild) {
    if (!s_restoredFromSave && is_gameplay_ready()) {
        custom_equip_restore_from_save();
        s_restoredFromSave = true;
    }

    if (s_framesSinceUpdateStart >= kModelLoadReloadSettleFrames) {
        for (int k = 0; k < 3; k++) {
            Entry* e = active_entry(static_cast<CustomEquipKind>(k));
            if (e) load_model(*e);
        }
    }

    Entry* tunicEntry = active_entry(CE_TUNIC);
    if (tunicEntry != nullptr && tunicEntry->model != nullptr) {
        const u8 base = custom_equip_resolved_base(tunicEntry->def);
        if (dComIfGs_getSelectEquipClothes() != base) {
            dMeter2Info_setCloth(base, false);
            dComIfGs_setSelectEquipClothes(base);
        }
    }

    if (a && !s_dollSwapActive && (duringRebuild || !is_wolf(a))) {
        if (tunicEntry && tunicEntry->model) {
            if (a->mpLinkModel != tunicEntry->model) {

                const cXyz savedSwapPos = a->current.pos;
                const s16  savedSwapAngleY = a->current.angle.y;

                s_tunicPosWatchFrames = 300;
                s_tunicPosWatchLast = savedSwapPos;
                s_tunicAngleWatchLast = a->current.angle;
                s_tunicShapeAngleWatchLast = a->shape_angle;
                if (dCamera_c* dcam0 = dCam_getBody()) {
                    s_tunicCamEyeWatchLast = dcam0->Eye();
                    s_tunicCamCenterWatchLast = dcam0->Center();
                } else if (camera_process_class* cam0 = dComIfGp_getCamera(0)) {
                    s_tunicCamAngleWatchLast = cam0->angle;
                    s_tunicCamEyeWatchLast = cam0->view.lookat.eye;
                    s_tunicCamCenterWatchLast = cam0->view.lookat.center;
                }

                s_tunicPosClampFrames = 10;

                if (s_originalLinkModel == nullptr) {
                    s_originalLinkModel = a->mpLinkModel;
                    s_originalHatModel  = a->mpLinkHatModel;
                    s_originalFaceModel = a->mpLinkFaceModel;
                    s_originalHandModel = a->mpLinkHandModel;
                    s_origShape_06d0 = a->field_0x06d0;
                    s_origShape_06d4 = a->field_0x06d4;
                    s_origShape_06d8 = a->field_0x06d8;
                    s_origShape_06dc = a->field_0x06dc;
                    s_origShape_06e0 = a->field_0x06e0;
                    s_origShape_06e8 = a->field_0x06e8;
                    s_origShape_06ec = a->field_0x06ec;
                    s_origShape_06f0 = a->field_0x06f0;
                    s_origShape_06e4 = a->field_0x06e4;
                }

                a->mpLinkModel = tunicEntry->model;
                a->mpLinkModel->setUserArea((uintptr_t)a);

                if (tunicEntry->hatModel != nullptr) {
                    a->mpLinkHatModel = tunicEntry->hatModel;
                    a->mpLinkHatModel->setUserArea((uintptr_t)a);
                }

                if (tunicEntry->faceModel != nullptr) {
                    a->mpLinkFaceModel = tunicEntry->faceModel;
                }

                if (tunicEntry->handModel != nullptr) {
                    a->mpLinkHandModel = tunicEntry->handModel;
                }

                retarget_face_material_anims(a);

                a->changeModelDataDirect(1);

                if (a->field_0x2060 != nullptr && a->field_0x1f20 != nullptr && a->field_0x1f20->getAnm(0) != nullptr) {
                    J3DTransformInfo ti;
                    a->field_0x1f20->getAnm(0)->getTransform(0, &ti);
                    if (J3DTransformInfo* oldTi = a->field_0x2060->getOldFrameTransInfo(0)) {
                        oldTi->mTranslate = ti.mTranslate;
                    }
                    a->field_0x2060->initOldFrameMorf(0.0f, 0, 35);
                }

                map_body_shapes(a, tunicEntry->def);

                if (a->mpLinkHandModel != nullptr && a->mpLinkHandModel->getModelData() != nullptr) {
                    J3DModelData* handData = a->mpLinkHandModel->getModelData();
                    u16 numMats = handData->getMaterialNum();
                    for (u16 i = 0; i < 11 && i < numMats; i++) {
                        handData->getMaterialNodePointer(i)->getShape()->hide();
                    }
                }

                if (a->mpLinkFaceModel != nullptr && a->mpLinkFaceModel->getModelData() != nullptr) {
                    J3DModelData* faceData = a->mpLinkFaceModel->getModelData();
                    a->mEyeHL1.remove();
                    a->mEyeHL1.entry(faceData, "highlight02");

                    J3DTexture* tex = faceData->getTexture();
                    JUTNameTab* nametable = faceData->getTextureName();
                    if (tex != nullptr && nametable != nullptr) {
                        for (u16 i = 0; i < tex->getNum(); i++) {
                            const char* tex_name = nametable->getName(i);
                            if (tex_name != nullptr &&
                                (strcmp(tex_name, "al_eyeball") == 0 || strcmp(tex_name, "highlight02") == 0 ||
                                 strcmp(tex_name, "eye_kage01") == 0))
                            {
                                ResTIMG* timg = tex->getResTIMG(i);
                                timg->maxLOD = 0;
                            }
                        }
                    }
                }
                if (a->current.pos.x != savedSwapPos.x || a->current.pos.y != savedSwapPos.y ||
                    a->current.pos.z != savedSwapPos.z) {
                    a->current.pos = savedSwapPos;
                    a->current.angle.y = savedSwapAngleY;
                }

                if (a->speed.x != 0.0f || a->speed.y != 0.0f || a->speed.z != 0.0f || a->speedF != 0.0f) {
                    a->speed.x = a->speed.y = a->speed.z = 0.0f;
                    a->speedF = 0.0f;
                }
            }
        } else if (s_originalLinkModel != nullptr) {
            if (a->mpLinkModel != s_originalLinkModel) restore_original_link_models(a);
            s_originalLinkModel = nullptr;
            s_originalHatModel  = nullptr;
            s_originalFaceModel = nullptr;
            s_originalHandModel = nullptr;
        }
    }
}

void custom_equip_set_link_model_wolf(bool isWolf) {
    s_linkModelIsWolf = isWolf;
}

void custom_equip_before_link_rebuild() {
    s_originalLinkModel = nullptr;
    s_originalHatModel  = nullptr;
    s_originalFaceModel = nullptr;
    s_originalHandModel = nullptr;
    s_hasLastBaseMtx[0] = s_hasLastBaseMtx[1] = s_hasLastBaseMtx[2] = false;
}

void custom_equip_on_alink_created(daAlink_c* a) {

    if (a == nullptr) return;

    s_tunicPosWatchFrames = 300;
    s_tunicPosWatchLast = a->current.pos;
    s_tunicAngleWatchLast = a->current.angle;
    s_tunicShapeAngleWatchLast = a->shape_angle;
    if (dCamera_c* dcam0 = dCam_getBody()) {
        s_tunicCamEyeWatchLast = dcam0->Eye();
        s_tunicCamCenterWatchLast = dcam0->Center();
    } else if (camera_process_class* cam0 = dComIfGp_getCamera(0)) {
        s_tunicCamAngleWatchLast = cam0->angle;
        s_tunicCamEyeWatchLast = cam0->view.lookat.eye;
        s_tunicCamCenterWatchLast = cam0->view.lookat.center;
    }

    if (a->mpLinkModel == nullptr || a->mpLinkHatModel == nullptr ||
        a->mpLinkFaceModel == nullptr || a->mpLinkHandModel == nullptr) return;
    if (a->field_0x2180[0] == nullptr || a->field_0x2180[1] == nullptr) return;

    if (s_activeId[CE_SWORD] < 0 && s_activeId[CE_SHIELD] < 0 && s_activeId[CE_TUNIC] < 0) return;
    custom_equip_apply(a, true);
}

void custom_equip_shutdown() {
    daAlink_c* pl = player();
    if (pl != nullptr) pl->mEyeHL1.remove();

    if (s_originalLinkModel != nullptr) {
        if (pl != nullptr && pl->mpLinkModel != s_originalLinkModel) restore_original_link_models(pl);
        s_originalLinkModel = nullptr;
        s_originalHatModel  = nullptr;
        s_originalFaceModel = nullptr;
        s_originalHandModel = nullptr;
    }

    for (int i = 0; i < kMaxDefs; i++) {
        Entry& e = s_entries[i];
        const bool inUse = pl != nullptr &&
            ((e.model     != nullptr && (pl->mpLinkModel == e.model ||
                                         pl->mSwordModel == e.model ||
                                         pl->mSheathModel == e.model ||
                                         pl->mShieldModel == e.model)) ||
             (e.hatModel  != nullptr && pl->mpLinkHatModel == e.hatModel) ||
             (e.faceModel != nullptr && pl->mpLinkFaceModel == e.faceModel) ||
             (e.handModel != nullptr && pl->mpLinkHandModel == e.handModel));
        // A model Link still wears keeps its archive (it is only ever freed with the process).
        if (!inUse) release_entry(e);
        e = Entry{};
    }
    s_count = 0;
    s_activeId[0] = s_activeId[1] = s_activeId[2] = -1;
    s_equipDebounce = 0;
    s_cachedStage[0] = '\0';
    s_hasLastBaseMtx[0] = s_hasLastBaseMtx[1] = s_hasLastBaseMtx[2] = false;
}
