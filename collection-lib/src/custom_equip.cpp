#include "collection_lib/custom_equip.hpp"
#include "collection_layout.hpp"

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

extern const ResourceService* cl_get_resource_service();

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

DEFINE_HOOK(&J3DModel::calc, J3DModelCalcHook);

DEFINE_HOOK(&daAlink_c::warpModelTexScroll, CeWarpModelTexScrollHook);
DEFINE_HOOK(&daAlink_c::changeWarpMaterial, CeChangeWarpMaterialHook);

namespace {

constexpr int kMaxDefs = 32;

struct Entry {
    CustomEquipDef def;

    ResourceBuffer iconBuf = RESOURCE_BUFFER_INIT;
    ResTIMG*       iconTex = nullptr;

    ResourceBuffer iconArcBuf  = RESOURCE_BUFFER_INIT;
    JKRArchive*    iconArc     = nullptr;
    bool           iconArcTried = false;

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

constexpr int kMaxFailedIconPaths = 8;
const char* s_failedIconPaths[kMaxFailedIconPaths] = {};
int         s_failedIconPathCount = 0;

bool icon_path_known_missing(const char* path) {
    if (path == nullptr) return false;
    for (int i = 0; i < s_failedIconPathCount; i++) {
        if (std::strcmp(s_failedIconPaths[i], path) == 0) return true;
    }
    return false;
}

void mark_icon_path_missing(const char* path) {
    if (path == nullptr || icon_path_known_missing(path)) return;
    if (s_failedIconPathCount < kMaxFailedIconPaths) {
        s_failedIconPaths[s_failedIconPathCount++] = path;
    }
}

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

    const ResourceService* res = cl_get_resource_service();
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
        if (!hatBmd) hatBmd = get_arc_res(e.arc, "al_head.bmd", 0x0010);
        if (hatBmd) {
            e.hatModel = load_single_bmd(hatBmd, 0x11000084, true);
        }

        void* faceBmd = find_bmd_matching(e.arc, "face");
        if (!faceBmd) faceBmd = get_arc_res(e.arc, "al_face.bmd", 0x000E);
        if (faceBmd) {
            e.faceModel = load_single_bmd(faceBmd, 0x11020284, true);
        }

        void* handBmd = find_bmd_matching(e.arc, "hand");
        if (!handBmd) handBmd = get_arc_res(e.arc, "al_hands.bmd", 0x000F);
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

Entry* active_entry(CustomEquipKind kind) {
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

struct CustomEquipSaveBlob {
    u8 shieldItem = 0;
    u8 swordItem  = 0;
    u8 tunicItem  = 0;
    u8 padding    = 0;
};

static void save_custom_equip_state(CustomEquipKind kind, u8 item) {
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

    if (g_saveSvc != nullptr && g_modCtx != nullptr) {
        CustomEquipSaveBlob blob{};
        size_t size = sizeof(blob);
        g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size);
        if (kind == CE_SHIELD)      blob.shieldItem = item;
        else if (kind == CE_SWORD)  blob.swordItem  = item;
        else if (kind == CE_TUNIC)  blob.tunicItem  = item;
        g_saveSvc->set_blob(g_modCtx, "custom_equip", &blob, sizeof(blob));
    }
}

static bool s_restoredFromSave = false;

static bool s_prevEquipWasActive[3] = { false, false, false };
static bool s_healPending = false;
static int s_healAttemptsLeft = 0;

static int s_framesSinceUpdateStart = 0;
constexpr int kModelLoadReloadSettleFrames = 1;

void custom_equip_restore_from_save() {
    if (!is_gameplay_ready()) return;

    if (s_count == 0) {
        collectionlib_run_slot_registration();
    }

    dSv_player_status_a_c& st = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA();

    if (s_activeId[CE_SHIELD] < 0) {
        u8 item = 0;
        if (g_saveSvc != nullptr && g_modCtx != nullptr) {
            CustomEquipSaveBlob blob{};
            size_t size = sizeof(blob);
            if (g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size) == MOD_OK) {
                item = blob.shieldItem;
            }
        }
        if (item == 0) item = st.getSelectEquip(5);
        if (item != 0) {
            for (int i = 0; i < s_count; i++) {
                if (s_entries[i].def.kind == CE_SHIELD && s_entries[i].def.item == item) {
                    s_activeId[CE_SHIELD] = i;

                    if (dComIfGs_getSelectEquipShield() == dItemNo_NONE_e) {
                        u8 backing = dItemNo_WOOD_SHIELD_e;
                        if (dComIfGs_isItemFirstBit(dItemNo_HYLIA_SHIELD_e)) backing = dItemNo_HYLIA_SHIELD_e;
                        else if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) backing = dItemNo_SHIELD_e;
                        dMeter2Info_setShield(backing, false);
                    }
                    break;
                }
            }
        }
    }

    if (s_activeId[CE_SWORD] < 0) {
        u8 item = 0;
        if (g_saveSvc != nullptr && g_modCtx != nullptr) {
            CustomEquipSaveBlob blob{};
            size_t size = sizeof(blob);
            if (g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size) == MOD_OK) {
                item = blob.swordItem;
            }
        }
        if (item == 0) item = st.unk31[0];
        if (item != 0) {
            for (int i = 0; i < s_count; i++) {
                if (s_entries[i].def.kind == CE_SWORD && s_entries[i].def.item == item) {
                    s_activeId[CE_SWORD] = i;
                    if (dComIfGs_getSelectEquipSword() == dItemNo_NONE_e) {
                        u8 backing = dItemNo_SWORD_e;
                        if (dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e)) backing = dItemNo_LIGHT_SWORD_e;
                        else if (dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e)) backing = dItemNo_MASTER_SWORD_e;
                        else if (dComIfGs_isItemFirstBit(dItemNo_SWORD_e)) backing = dItemNo_SWORD_e;
                        else if (dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e)) backing = dItemNo_WOOD_STICK_e;
                        dMeter2Info_setSword(backing, false);
                    }
                    break;
                }
            }
        }
    }

    if (s_activeId[CE_TUNIC] < 0) {
        u8 item = 0;
        if (g_saveSvc != nullptr && g_modCtx != nullptr) {
            CustomEquipSaveBlob blob{};
            size_t size = sizeof(blob);
            if (g_saveSvc->get_blob(g_modCtx, "custom_equip", &blob, &size) == MOD_OK) {
                item = blob.tunicItem;
            }
        }
        if (item == 0) item = st.unk31[1];
        if (item != 0) {
            for (int i = 0; i < s_count; i++) {
                if (s_entries[i].def.kind == CE_TUNIC && s_entries[i].def.item == item) {
                    s_activeId[CE_TUNIC] = i;
                    dMeter2Info_setCloth(s_entries[i].def.baseClothes, false);
                    dComIfGs_setSelectEquipClothes(s_entries[i].def.baseClothes);
                    break;
                }
            }
        }
    }
}

void custom_equip_reset_registry() {
    for (int i = 0; i < s_count; i++) {
        Entry& e = s_entries[i];
        const ResourceService* res = cl_get_resource_service();
        if (res != nullptr && g_modCtx != nullptr) {
            res->free(g_modCtx, &e.iconBuf);
            res->free(g_modCtx, &e.iconArcBuf);
            res->free(g_modCtx, &e.arcBuf);
        }
        if (e.iconArc != nullptr) { JKRUnmountArchive(e.iconArc); e.iconArc = nullptr; }
        if (e.arc != nullptr && !e.arcIsGame) JKRUnmountArchive(e.arc);
        e = Entry{};
    }
    s_count = 0;
    for (int k = 0; k < 3; k++) s_activeId[k] = -1;
}

void custom_equip_remove(int id) {
    if (id < 0 || id >= s_count) return;

    const CustomEquipKind kind = s_entries[id].def.kind;
    if (s_activeId[kind] == id) {
        custom_equip_clear(kind);
        s_activeId[kind] = -1;
    }

    const ResourceService* res = cl_get_resource_service();
    Entry& e = s_entries[id];
    if (res != nullptr && g_modCtx != nullptr) {
        res->free(g_modCtx, &e.iconBuf);
        res->free(g_modCtx, &e.iconArcBuf);
        res->free(g_modCtx, &e.arcBuf);
    }
    if (e.iconArc != nullptr) { JKRUnmountArchive(e.iconArc); e.iconArc = nullptr; }
    if (e.arc != nullptr && !e.arcIsGame) { JKRUnmountArchive(e.arc); e.arc = nullptr; }

    for (int i = id; i < s_count - 1; i++) s_entries[i] = s_entries[i + 1];
    s_count--;

    for (int k = 0; k < 3; k++) {
        if (s_activeId[k] > id) s_activeId[k]--;
    }
}
#include "collection_lib/collection_common.hpp"

int custom_equip_register(const CustomEquipDef& def) {
    CustomEquipDef resolved = def;
    if (resolved.item == 0) {
        u8 row = (resolved.kind == CE_SWORD) ? 1 : (resolved.kind == CE_SHIELD) ? 2 : 3;
        const u8 first = (row == 3) ? 5 : 4;
        u8 found = 0;
        for (u8 col = first; col <= 12; ++col) {
            if (!cl_item_exists(row, col)) {
                found = col;
                break;
            }
        }
        if (found == 0) return -1;
        resolved.item = found;
    }

    for (int i = 0; i < s_count; i++) {
        if (s_entries[i].def.kind == resolved.kind && s_entries[i].def.item == resolved.item) {
            if (s_entries[i].def.modelFileId != resolved.modelFileId ||
                s_entries[i].def.sheathFileId != resolved.sheathFileId ||
                (s_entries[i].def.modelArc != nullptr && std::strcmp(s_entries[i].def.modelArc, resolved.modelArc) != 0)) {
                s_entries[i].model = nullptr;
                s_entries[i].sheathModel = nullptr;
                s_entries[i].tried = false;
                s_entries[i].tryCount = 0;
            }
            s_entries[i].def = resolved;
            return i;
        }
    }
    if (s_count >= kMaxDefs) return -1;
    int id = s_count++;

    ResourceBuffer ib = s_entries[id].iconBuf; ResTIMG* it = s_entries[id].iconTex;
    ResourceBuffer ab = s_entries[id].arcBuf;  JKRArchive* ar = s_entries[id].arc;
    bool ag = s_entries[id].arcIsGame;
    J3DModel* md = s_entries[id].model;
    J3DModel* sm = s_entries[id].sheathModel;
    J3DModel* hm = s_entries[id].hatModel;
    J3DModel* fm = s_entries[id].faceModel;
    J3DModel* hd = s_entries[id].handModel;
    bool tr = s_entries[id].tried;
    if (s_entries[id].def.modelFileId != resolved.modelFileId ||
        s_entries[id].def.sheathFileId != resolved.sheathFileId ||
        s_entries[id].def.kind != resolved.kind ||
        s_entries[id].def.item != resolved.item ||
        (s_entries[id].def.modelArc != nullptr && std::strcmp(s_entries[id].def.modelArc, resolved.modelArc) != 0)) {
        md = nullptr;
        sm = nullptr;
        hm = nullptr;
        fm = nullptr;
        hd = nullptr;
        tr = false;
    }
    s_entries[id] = Entry{};
    s_entries[id].def = resolved;
    s_entries[id].iconBuf = ib; s_entries[id].iconTex = it;
    s_entries[id].arcBuf = ab;  s_entries[id].arc = ar;
    s_entries[id].arcIsGame = ag;
    s_entries[id].model = md;
    s_entries[id].sheathModel = sm;
    s_entries[id].hatModel = hm;
    s_entries[id].faceModel = fm;
    s_entries[id].handModel = hd;
    s_entries[id].tried = tr;
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
    if (id < 0 || id >= s_count) return;
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

bool custom_equip_active(CustomEquipKind kind) { return s_activeId[kind] >= 0; }

int custom_equip_active_id(CustomEquipKind kind) { return s_activeId[kind]; }

static u8 kind_row(CustomEquipKind k) { return k == CE_SWORD ? 1 : k == CE_SHIELD ? 2 : 3; }

static int def_at_cell(u8 x, u8 y) {
    const SlotSpec* s = slot_at(x, y);
    if (s != nullptr) {
        CustomEquipKind kind = (s->at.row == 1) ? CE_SWORD : (s->at.row == 2) ? CE_SHIELD : CE_TUNIC;
        for (int i = 0; i < s_count; i++) {
            if (s_entries[i].def.kind == kind && s_entries[i].def.item == s->at.item) {
                return i;
            }
        }
    }
    for (int i = 0; i < s_count; i++) {
        SlotCell cell = grid_cell(kind_row(s_entries[i].def.kind), s_entries[i].def.item);
        if (cell.x == x && cell.y == y) return i;
    }
    return -1;
}

void update_frame_highlights(dMenu_Collect2D_c* collect2D);

void custom_equip_on_equip(dMenu_Collect2D_c* collect2D) {
    if (!collect2D || collect2D->mIsWolf || s_equipDebounce > 0) return;
    daAlink_c* alink = daAlink_getAlinkActorClass();
    int id = def_at_cell(collect2D->mCursorX, collect2D->mCursorY);
    if (id < 0) return;

    CustomEquipKind kind = s_entries[id].def.kind;
    if (kind == CE_SHIELD && alink && alink->getShieldChangeWaitTimer() != 0) return;
    if (kind == CE_SWORD && alink && alink->getSwordChangeWaitTimer() != 0) return;
    if (kind == CE_TUNIC && alink && alink->getClothesChangeWaitTimer() != 0) return;

    if (s_activeId[kind] == id) {

        if (kind == CE_TUNIC) return;

        s_equipDebounce = 8;
        custom_equip_clear(kind);
        if (kind == CE_SHIELD) {
            dMeter2Info_setShield(dItemNo_NONE_e, false);
        } else if (kind == CE_SWORD) {
            dMeter2Info_setSword(dItemNo_NONE_e, false);
            refresh_sword_model(player());
        }
        Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_COMBINE_OFF, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dMeter2Info_set2DVibration();
        update_frame_highlights(collect2D);
    } else {

        s_equipDebounce = 8;
        custom_equip_activate(id);
        if (kind == CE_SHIELD) {
            if (dComIfGs_getSelectEquipShield() == dItemNo_NONE_e) {
                u8 backing = dItemNo_WOOD_SHIELD_e;
                if (dComIfGs_isItemFirstBit(dItemNo_HYLIA_SHIELD_e)) backing = dItemNo_HYLIA_SHIELD_e;
                else if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) backing = dItemNo_SHIELD_e;
                dMeter2Info_setShield(backing, false);
            }
        } else if (kind == CE_SWORD) {
            if (dComIfGs_getSelectEquipSword() == dItemNo_NONE_e) {
                u8 backing = dItemNo_SWORD_e;
                if (dComIfGs_isItemFirstBit(dItemNo_LIGHT_SWORD_e)) backing = dItemNo_LIGHT_SWORD_e;
                else if (dComIfGs_isItemFirstBit(dItemNo_MASTER_SWORD_e)) backing = dItemNo_MASTER_SWORD_e;
                else if (dComIfGs_isItemFirstBit(dItemNo_SWORD_e)) backing = dItemNo_SWORD_e;
                else if (dComIfGs_isItemFirstBit(dItemNo_WOOD_STICK_e)) backing = dItemNo_WOOD_STICK_e;
                dMeter2Info_setSword(backing, false);
            }
            refresh_sword_model(player());
        } else if (kind == CE_TUNIC) {
            dMeter2Info_setCloth(s_entries[id].def.baseClothes, false);
            dComIfGs_setSelectEquipClothes(s_entries[id].def.baseClothes);
        }
        Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_SET_X, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        dMeter2Info_set2DVibration();
        update_frame_highlights(collect2D);
    }
}

bool custom_equip_is_unlocked(u8 x, u8 y) {
    int id = def_at_cell(x, y);
    if (id < 0) return false;
    const CustomEquipDef& d = s_entries[id].def;
    return d.unlocked ? d.unlocked() : true;
}

bool custom_equip_is_equipped(u8 x, u8 y) {
    int id = def_at_cell(x, y);
    return id >= 0 && s_activeId[s_entries[id].def.kind] == id;
}

ResTIMG* custom_equip_icon(int id) {
    if (id < 0 || id >= s_count) return nullptr;
    Entry& e = s_entries[id];
    if (e.iconTex != nullptr) return e.iconTex;

    if (e.def.iconArcFileId.fileId != 0xFFFF) {
        const ResourceService* resSvc = cl_get_resource_service();

        if (e.iconArc == nullptr && e.iconArcBuf.data == nullptr && !e.iconArcTried
            && resSvc != nullptr && g_modCtx != nullptr
            && !icon_path_known_missing(e.def.iconBti)) {
            e.iconArcTried = true;
            if (resSvc->load(g_modCtx, e.def.iconBti, &e.iconArcBuf) == MOD_OK
                && e.iconArcBuf.data != nullptr) {
                JKRHeap* heap = JKRHeap::getRootHeap();
                if (heap == nullptr) heap = static_cast<JKRHeap*>(mDoExt_getGameHeap());
                e.iconArc = JKRArchive::mount(e.iconArcBuf.data, heap, JKRArchive::MOUNT_DIRECTION_HEAD);
            } else {
                mark_icon_path_missing(e.def.iconBti);
            }
        }

        JKRArchive* arc = e.iconArc;
        if (arc == nullptr) arc = dComIfGp_getCollectResArchive();
        if (arc == nullptr) return nullptr;

        void* img = arc->getIdxResource(e.def.iconArcFileId.fileId);
        if (img == nullptr) img = arc->getResource(e.def.iconArcFileId.fileId);
        if (img == nullptr) return nullptr;

        e.iconTex = reinterpret_cast<ResTIMG*>(img);
        e.iconTex->alphaEnabled = 1;
        return e.iconTex;
    }

    if (e.iconBuf.data == nullptr) {
        const ResourceService* res = cl_get_resource_service();
        if (res == nullptr || g_modCtx == nullptr) return nullptr;
        res->load(g_modCtx, e.def.iconBti, &e.iconBuf);
        if (e.iconBuf.data == nullptr) return nullptr;
    }
    JKRHeap* gameHeap = mDoExt_getGameHeap();
    if (gameHeap != nullptr && e.iconBuf.size > 0) {
        void* p = gameHeap->alloc(e.iconBuf.size, 32);
        if (p != nullptr) {
            memcpy(p, e.iconBuf.data, e.iconBuf.size);
            e.iconTex = reinterpret_cast<ResTIMG*>(p);
            e.iconTex->alphaEnabled = 1;
            return e.iconTex;
        }
    }
    e.iconTex = reinterpret_cast<ResTIMG*>(e.iconBuf.data);
    e.iconTex->alphaEnabled = 1;
    return e.iconTex;
}

u64 custom_equip_icon_tag(int id)  { return static_cast<u64>(0x63656900) + id; }
u64 custom_equip_pic_tag(int id)   { return static_cast<u64>(0x63657000) + id; }
u64 custom_equip_frame_tag(int id) { return static_cast<u64>(0x63656700) + id; }

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
HookAction on_mw_execute_pre_doll_safety(ModContext*, void*, void*, void*);
HookAction on_j3dmodel_calc_pre(ModContext*, void*, void*, void*);

void custom_equip_init_hooks(const HookService* hook_svc, const SaveService* save_svc) {
    if (save_svc != nullptr && g_modCtx != nullptr) {
        save_svc->observe_saves(g_modCtx, on_custom_equip_new_save, on_custom_equip_save_loaded, nullptr, nullptr, nullptr);
    }
    if (!hook_svc) return;
    mods::hook::add_pre<CeModelDrawHook>(hook_svc, on_alink_model_draw_pre);
    mods::hook::add_pre<CeBasicModelDrawHook>(hook_svc, on_alink_model_draw_pre);
    mods::hook::add_post<CeAlinkDrawHook>(hook_svc, on_alink_draw_post);
    mods::hook::add_post<CeAlinkSwDrawHook>(hook_svc, on_alink_draw_post);
    mods::hook::add_pre<CeSetWaterDropColorHook>(hook_svc, on_set_water_drop_color_pre);
    mods::hook::add_pre<CeShadowAddRealHook>(hook_svc, on_add_real_shadow_pre);
    mods::hook::add_pre<CeAlinkShadowDrawHook>(hook_svc, on_alink_shadow_draw_pre);
    mods::hook::add_pre<CePadSetColorHook>(hook_svc, on_pad_set_color_pre);
    mods::hook::add_post<CeAlinkShadowDrawHook>(hook_svc, on_alink_shadow_draw_post);
    mods::hook::add_pre<CeCollect3DCreateHook>(hook_svc, on_collect_3d_create_pre);
    mods::hook::add_pre<CeCollect3DDeleteHook>(hook_svc, on_collect_3d_delete_pre);
    mods::hook::add_pre<CeInitStatusWindowHook>(hook_svc, on_alink_init_status_window_pre);

    mods::hook::add_pre<MwExecuteHook>(hook_svc, on_mw_execute_pre_doll_safety);
    mods::hook::add_pre<J3DModelCalcHook>(hook_svc, on_j3dmodel_calc_pre);

    mods::hook::replace<CeWarpModelTexScrollHook>(hook_svc, on_warp_model_tex_scroll_replace);
    mods::hook::replace<CeChangeWarpMaterialHook>(hook_svc, on_change_warp_material_replace);
}

static void on_stage_changed() {
    for (int i = 0; i < kMaxDefs; i++) {
        s_entries[i].iconTex = nullptr;
    }
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

static void custom_equip_menu_doll_begin() {
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
    a->mpLinkModel->setUserArea((uintptr_t)a);
    if (a->mpLinkHatModel) a->mpLinkHatModel->setUserArea((uintptr_t)a);

    retarget_face_material_anims(a);
    a->changeModelDataDirect(1);

    a->mEyeHL1.remove();
    if (a->mpLinkFaceModel != nullptr && a->mpLinkFaceModel->getModelData() != nullptr) {
        a->mEyeHL1.entry(a->mpLinkFaceModel->getModelData(), "highlight02");
    }

    s_dollSwapActive = true;
}

static void custom_equip_menu_doll_end() {
    s_dollSwapActive = false;
}

HookAction on_collect_3d_create_pre(ModContext*, void*, void*, void*) {
    if (is_collection_menu_enabled()) custom_equip_menu_doll_begin();
    return HOOK_CONTINUE;
}

HookAction on_collect_3d_delete_pre(ModContext*, void*, void*, void*) {
    custom_equip_menu_doll_end();
    return HOOK_CONTINUE;
}

HookAction on_alink_init_status_window_pre(ModContext*, void*, void*, void*) {
    if (is_collection_menu_enabled()) custom_equip_menu_doll_begin();
    return HOOK_CONTINUE;
}

HookAction on_mw_execute_pre_doll_safety(ModContext*, void*, void*, void*) {
    return HOOK_CONTINUE;
}

HookAction on_j3dmodel_calc_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* a = player();
    if (a == nullptr || !a->checkStatusWindowDraw()) return HOOK_CONTINUE;

    J3DModel* model = args ? mods::arg<J3DModel*>(args, 0) : nullptr;
    if (model == nullptr) return HOOK_CONTINUE;

    const bool isActorModel = model == a->mpLinkModel || model == a->mpLinkFaceModel ||
                              model == a->mpLinkHatModel || model == a->mpLinkHandModel;
    const bool isVanillaModel = model == s_originalLinkModel || model == s_originalFaceModel ||
                                model == s_originalHatModel || model == s_originalHandModel;
    if (isActorModel && !isVanillaModel) {

    }
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
        if (dComIfGs_getSelectEquipClothes() != tunicEntry->def.baseClothes) {
            dMeter2Info_setCloth(tunicEntry->def.baseClothes, false);
            dComIfGs_setSelectEquipClothes(tunicEntry->def.baseClothes);
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

                if (a->field_0x064C != nullptr) {
                    if (a->field_0x064C->getMaterialNum() > 16) {
                        a->field_0x064C->getMaterialNodePointer(16)->getShape()->hide();
                    }
                    if (a->field_0x064C->getMaterialNum() > 12) {
                        a->field_0x06d8 = a->field_0x064C->getMaterialNodePointer(11)->getShape();
                        a->field_0x06dc = a->field_0x064C->getMaterialNodePointer(12)->getShape();
                        a->field_0x06e0 = a->field_0x064C->getMaterialNodePointer(6)->getShape();
                        a->field_0x06e8 = a->field_0x064C->getMaterialNodePointer(8)->getShape();
                        a->field_0x06ec = a->field_0x064C->getMaterialNodePointer(4)->getShape();
                        a->field_0x06f0 = a->field_0x064C->getMaterialNodePointer(7)->getShape();
                    }
                    a->field_0x06d0 = a->field_0x06d8;
                    a->field_0x06d4 = a->field_0x06dc;
                }

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
            if (a->mpLinkModel != s_originalLinkModel) {
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

                a->mpLinkModel->setUserArea((uintptr_t)a);
                if (a->mpLinkHatModel) a->mpLinkHatModel->setUserArea((uintptr_t)a);

                retarget_face_material_anims(a);
                a->changeModelDataDirect(1);

                a->mEyeHL1.remove();
                if (a->mpLinkFaceModel != nullptr && a->mpLinkFaceModel->getModelData() != nullptr) {
                    a->mEyeHL1.entry(a->mpLinkFaceModel->getModelData(), "highlight02");
                }
            }
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

    if (daAlink_c* a0 = player()) {
        a0->mEyeHL1.remove();
    }

    if (s_originalLinkModel != nullptr) {
        daAlink_c* a = player();
        if (a && a->mpLinkModel != s_originalLinkModel) {
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
            a->mpLinkModel->setUserArea((uintptr_t)a);
            if (a->mpLinkHatModel) a->mpLinkHatModel->setUserArea((uintptr_t)a);

            retarget_face_material_anims(a);
            a->changeModelDataDirect(1);

            a->mEyeHL1.remove();
            if (a->mpLinkFaceModel != nullptr && a->mpLinkFaceModel->getModelData() != nullptr) {
                a->mEyeHL1.entry(a->mpLinkFaceModel->getModelData(), "highlight02");
            }
        }
        s_originalLinkModel = nullptr;
        s_originalHatModel  = nullptr;
        s_originalFaceModel = nullptr;
        s_originalHandModel = nullptr;
    }

    daAlink_c* pl = player();
    const ResourceService* res = cl_get_resource_service();
    for (int i = 0; i < kMaxDefs; i++) {
        Entry& e = s_entries[i];

        bool inUse = pl != nullptr &&
            ((e.model     != nullptr && (pl->mpLinkModel == e.model ||
                                         pl->mSwordModel == e.model ||
                                         pl->mSheathModel == e.model ||
                                         pl->mShieldModel == e.model)) ||
             (e.hatModel  != nullptr && pl->mpLinkHatModel == e.hatModel) ||
             (e.faceModel != nullptr && pl->mpLinkFaceModel == e.faceModel) ||
             (e.handModel != nullptr && pl->mpLinkHandModel == e.handModel));
        if (res != nullptr && g_modCtx != nullptr) {
            res->free(g_modCtx, &e.iconBuf);
        }
        if (!inUse) {
            if (e.arc != nullptr && !e.arcIsGame) JKRUnmountArchive(e.arc);
            if (res != nullptr && g_modCtx != nullptr) res->free(g_modCtx, &e.arcBuf);
            if (e.iconArc != nullptr) JKRUnmountArchive(e.iconArc);
            if (res != nullptr && g_modCtx != nullptr) res->free(g_modCtx, &e.iconArcBuf);
            s_entries[i] = Entry{};
        } else {

            e.iconBuf = ResourceBuffer{};
            e.iconTex = nullptr;
        }
    }
    s_count = 0;
    s_activeId[0] = s_activeId[1] = s_activeId[2] = -1;
    s_equipDebounce = 0;
    s_cachedStage[0] = '\0';
    s_hasLastBaseMtx[0] = s_hasLastBaseMtx[1] = s_hasLastBaseMtx[2] = false;
}
