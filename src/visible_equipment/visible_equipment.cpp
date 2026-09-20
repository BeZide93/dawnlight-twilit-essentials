#include "visible_equipment.hpp"

#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphBase/J3DMatBlock.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DStruct.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/J3DGraphBase/J3DTevs.h"
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "d/d_resorce.h"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXPixel.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_profile_lst.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "mods/api.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "res/Object/Always.h"
#include "../z_button/z_button.hpp"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef _MSC_VER
#define VE_SEH_TRY __try
#define VE_SEH_EXCEPT __except (1)
#else
#define VE_SEH_TRY if (true)
#define VE_SEH_EXCEPT else
#endif

static const LogService *s_logSvc = nullptr;
static ModContext *s_modCtx = nullptr;

bool g_configVisibleEquipmentEnabled = false;
int g_configVisibleEquipDisplayMode = 1;
bool g_configVisibleEquipMirrorBow = true;

bool g_configVisibleEquipShowBow = true;
bool g_configVisibleEquipShowLantern = true;
bool g_configVisibleEquipQuiverOnBelt = true;

static J3DModel *s_customBowModel = nullptr;
static J3DModel *s_customQuiverModel = nullptr;
static int s_loadedQuiverType = 0;

static u8 *s_bowRawData = nullptr;
static u8 *s_quiverRawData = nullptr;

static u8 *allocAligned32(size_t size) {
  return static_cast<u8 *>(operator new[](size, std::align_val_t(32)));
}

static void freeAligned32(u8 *&ptr) {
  if (ptr != nullptr) {
    operator delete[](ptr, std::align_val_t(32));
    ptr = nullptr;
  }
}

static bool s_gearShaderUsable = true;
static bool s_gearShaderCapable[3] = {};
static bool s_gearModelWarpOn[3] = {};

static J3DModelData *s_lanternWarpModelData = nullptr;

DEFINE_HOOK(&daAlink_c::draw, AlinkDrawHook);
DEFINE_HOOK(&daAlink_c::statusWindowDraw, AlinkStatusWindowDrawHook);

static bool s_inStatusWindow = false;

inline s16 degToS16(f32 deg) {
  return static_cast<s16>(deg * (65536.0f / 360.0f));
}

extern bool isNativeZButtonEngine();

static u32 getLinkShadowId(daAlink_c *alink) {
  if (alink == nullptr) {
    return 0;
  }

  if (alink->checkHorseRide()) {
    daHorse_c *horse = reinterpret_cast<daHorse_c *>(dComIfGp_getHorseActor());
    if (horse != nullptr) {
      return horse->getShadowID();
    }
  }

  const u8 *basePtr = reinterpret_cast<const u8 *>(&alink->field_0x31a4);

  return static_cast<u32>(alink->field_0x31a4);
}

static void addModelShadow(daAlink_c *alink, J3DModel *model) {
  if (s_inStatusWindow) {
    return;
  }
  if (alink != nullptr && model != nullptr) {
    u32 shadowId = getLinkShadowId(alink);
    if (shadowId != 0) {
      dComIfGd_addRealShadow(shadowId, model);
    }
  }
}

static bool isWolfOrTransforming(daAlink_c *alink) {
  if (alink == nullptr) {
    return true;
  }
  if (alink->checkWolf()) {
    return true;
  }
  if (alink->checkWolfShapeReverse()) {
    return true;
  }
  if (alink->checkMetamorphose()) {
    return true;
  }
  return false;
}

static bool isInWarpVisual(daAlink_c *alink) {
  if (alink == nullptr) {
    return false;
  }
  u16 proc = alink->mProcID;
  return proc == daAlink_c::PROC_DUNGEON_WARP_READY || proc == daAlink_c::PROC_DUNGEON_WARP ||
         proc == daAlink_c::PROC_DUNGEON_WARP_SCN_START || proc == daAlink_c::PROC_TW_GATE;
}

static MtxP getBoneMtx(daAlink_c *alink, const char *boneName) {
  if (alink == nullptr || alink->mpLinkModel == nullptr ||
      boneName == nullptr || isWolfOrTransforming(alink)) {
    return nullptr;
  }

  J3DModelData *modelData = alink->mpLinkModel->getModelData();
  if (modelData != nullptr) {
    JUTNameTab *jointNames = modelData->getJointName();
    if (jointNames != nullptr && jointNames->getResNameTable() != nullptr) {
      s16 idx = jointNames->getIndex(boneName);
      if (idx >= 0 && idx < modelData->getJointNum()) {
        return alink->mpLinkModel->getAnmMtx(static_cast<u16>(idx));
      }
    }
  }
  return nullptr;
}

static bool isBowUnlocked() {
  u8 bowItem = dComIfGs_getItem(SLOT_4, false);
  return (bowItem != dItemNo_NONE_e && bowItem != 0x00 && bowItem != 0xFF);
}

static bool isLanternUnlocked() {
  if (dComIfGp_checkItemGet(dItemNo_KANTERA_e, 1)) {
    return true;
  }
  for (u8 slot = 0; slot < 24; slot++) {
    u8 item = dComIfGs_getItem(slot, false);
    if (item == dItemNo_KANTERA_e || item == dItemNo_KANTERA2_e || item == 0x48) {
      return true;
    }
  }
  return dComIfGs_isItemFirstBit(dItemNo_KANTERA_e) != 0 ||
         dComIfGs_isItemFirstBit(dItemNo_KANTERA2_e) != 0;
}

static bool isBowButtonItem(u8 item) {
  return daPy_py_c::checkBowItem(item);
}

static bool checkShouldShowBow() {
  if (!isBowUnlocked()) {
    return false;
  }

  if (g_configVisibleEquipDisplayMode == 1) {
    return true;
  }

  u8 slot0 = dComIfGp_getSelectItem(0);
  u8 slot1 = dComIfGp_getSelectItem(1);
  u8 zSlot = dComIfGs_getSelectItemIndex(2);
  u8 slot2 = (zSlot != 0xFF && zSlot != 0x00) ? dComIfGs_getItem(zSlot, false)
                                              : dComIfGp_getSelectItem(2);
  return isBowButtonItem(slot0) || isBowButtonItem(slot1) || isBowButtonItem(slot2);
}

static bool checkShouldShowLantern() {
  if (!isLanternUnlocked()) {
    return false;
  }

  if (g_configVisibleEquipDisplayMode == 1) {
    return true;
  }

  u8 slot0 = dComIfGp_getSelectItem(0);
  u8 slot1 = dComIfGp_getSelectItem(1);
  u8 zSlot = dComIfGs_getSelectItemIndex(2);
  u8 slot2 = (zSlot != 0xFF && zSlot != 0x00) ? dComIfGs_getItem(zSlot, false)
                                              : dComIfGp_getSelectItem(2);
  return (slot0 == dItemNo_KANTERA_e || slot1 == dItemNo_KANTERA_e ||
          slot2 == dItemNo_KANTERA_e || slot0 == 0x48 || slot1 == 0x48 ||
          slot2 == 0x48);
}

static void enable_material_fog(J3DModel *model) {
  if (model == nullptr || model->getModelData() == nullptr) {
    return;
  }
  J3DModelData *mData = model->getModelData();
  for (u16 m = 0; m < mData->getMaterialNum(); ++m) {
    J3DMaterial *mat = mData->getMaterialNodePointer(m);
    if (mat == nullptr || mat->getFog() == nullptr) {
      continue;
    }
    J3DFogInfo *info = mat->getFog()->getFogInfo();
    if (info != nullptr && info->mType == 0) {
      info->mType = 2;
    }
  }
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

static void safeOnWarpMaterial(J3DModelData *modelData) {
  if (modelData == nullptr) return;
  u16 matNum = modelData->getMaterialNum();
  for (u16 i = 0; i < matNum; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    if (material == nullptr) continue;
    J3DTevBlock *tevBlock = material->getTevBlock();
    if (tevBlock == nullptr) continue;
    u8 tevStageNum = tevBlock->getTevStageNum();
    if (tevStageNum == 0) continue;
    J3DTevOrder *tevorder = tevBlock->getTevOrder(tevStageNum - 1);
    if (tevorder != nullptr && tevorder->getTexMap() == 3) {
      continue;
    }
    tevBlock->setTevStageNum(tevStageNum + 1);
    J3DTexGenBlock *texGenBlock = material->getTexGenBlock();
    if (texGenBlock != nullptr) {
      texGenBlock->setTexGenNum(texGenBlock->getTexGenNum() + 1);
    }
  }
}

static void safeOffWarpMaterial(J3DModelData *modelData) {
  if (modelData == nullptr) return;
  u16 matNum = modelData->getMaterialNum();
  for (u16 i = 0; i < matNum; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    if (material == nullptr) continue;
    J3DTevBlock *tevBlock = material->getTevBlock();
    if (tevBlock == nullptr) continue;
    u8 tevStageNum = tevBlock->getTevStageNum();
    if (tevStageNum <= 1) continue;
    J3DTevOrder *tevorder = tevBlock->getTevOrder(tevStageNum - 1);
    if (tevorder == nullptr || tevorder->getTexMap() != 3) {
      continue;
    }
    tevBlock->setTevStageNum(tevStageNum - 1);
    J3DTexGenBlock *texGenBlock = material->getTexGenBlock();
    if (texGenBlock != nullptr && texGenBlock->getTexGenNum() > 1) {
      texGenBlock->setTexGenNum(texGenBlock->getTexGenNum() - 1);
    }
  }
}

static void applyWarpSRT(J3DModelData *modelData, const cXyz &pos, f32 transX, f32 transY, const char *name) {
  if (modelData == nullptr) return;
  u16 matNum = modelData->getMaterialNum();
  if (matNum == 0) return;

  mDoMtx_stack_c::transS(-pos.x, -pos.y, -pos.z);
  camera_process_class *camera = dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
  if (camera == nullptr) {
    camera = dComIfGp_getCamera(0);
  }
  if (camera != nullptr) {
    mDoMtx_stack_c::YrotM(fopCamM_GetAngleY(camera));
  } else {
    mDoMtx_stack_c::YrotM(0);
  }

  J3DTexMtx *updatedMtxs[8] = {};
  int updatedCount = 0;

  for (u16 m = 0; m < matNum && m < 8; ++m) {
    J3DMaterial *mat = modelData->getMaterialNodePointer(m);
    if (mat == nullptr || mat->getTexGenBlock() == nullptr) continue;
    J3DTexGenBlock *texGen = mat->getTexGenBlock();
    u32 num = texGen->getTexGenNum();
    J3DTexMtx *warpTexMtx = nullptr;
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

    J3DTexMtxInfo &texMtxInfo = warpTexMtx->getTexMtxInfo();
    texMtxInfo.mSRT.mTranslationX = transX;
    texMtxInfo.mSRT.mTranslationY = transY;
    cMtx_concat(s_warpTexMtxInfo.mEffectMtx, mDoMtx_stack_c::get(), texMtxInfo.mEffectMtx);
  }

  if (updatedCount == 0) {
    static int s_loggedMissing = 0;
    if (s_loggedMissing++ < 5) {
    }
  }
}

static void loadBowModel(const LogService *log_svc, ModContext *mod_ctx) {
  if (s_customBowModel != nullptr) {
    return;
  }

  JKRArchive *arc = g_dComIfG_gameInfo.play.getAnmArchive();
  if (arc == nullptr) {
    return;
  }

  JKRArchive::SDIFileEntry *entry = arc->findIdxResource(0x0314);
  if (entry == nullptr) {
    return;
  }

  u32 fileSize = entry->getSize();
  u32 allocSize = std::max<u32>(fileSize, 0x4C00);
  if (allocSize == 0) {
    return;
  }

  freeAligned32(s_bowRawData);
  s_bowRawData = allocAligned32(allocSize);
  u32 readBytes = arc->readIdxResource(s_bowRawData, allocSize, 0x0314);
  if (readBytes == 0) {
    freeAligned32(s_bowRawData);
    return;
  }

  J3DModelData *modelData = dRes_info_c::loaderBasicBmd('BMWR', s_bowRawData);
  if (modelData == nullptr) {
    freeAligned32(s_bowRawData);
    return;
  }

  safeOnWarpMaterial(modelData);
  s_customBowModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084 | 0x2000400);
  safeOffWarpMaterial(modelData);

  if (s_customBowModel != nullptr) {
    s_gearShaderCapable[0] = true;
    s_customBowModel->setBaseScale(cXyz(1.0f, 1.0f, 1.0f));
    enable_material_fog(s_customBowModel);
  }
}

static u8* loadPristineBmdFromDvd(const char *dvdPath, const char *resName, s16 resIdx) {
  u32 arcSize = 0;
  void *arcBuf = JKRDvdRipper::loadToMainRAM(
      dvdPath, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, nullptr,
      JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr, &arcSize);
  if (arcBuf == nullptr) {
    return nullptr;
  }

  u8 *bmdBuffer = nullptr;
  const u8 *rawBuf = static_cast<const u8 *>(arcBuf);

  u32 sig = read_big_endian_u32(rawBuf);
  if (sig == 0x52415243) {
    u32 header_length = read_big_endian_u32(rawBuf + 0x08);
    u32 file_data_offset = read_big_endian_u32(rawBuf + 0x0C);

    const u8 *infoBlock = rawBuf + header_length;
    u32 num_file_entries = read_big_endian_u32(infoBlock + 0x08);
    u32 file_entry_offset = read_big_endian_u32(infoBlock + 0x0C);
    u32 string_table_offset = read_big_endian_u32(infoBlock + 0x14);

    const char *stringTable = reinterpret_cast<const char *>(infoBlock + string_table_offset);
    const u8 *archiveData = rawBuf + header_length + file_data_offset;

    for (u32 i = 0; i < num_file_entries; ++i) {
      const u8 *entryPtr = infoBlock + file_entry_offset + (i * 0x14);
      u32 type_flags_and_name = read_big_endian_u32(entryPtr + 0x04);
      u32 nameOffset = type_flags_and_name & 0x00FFFFFF;
      const char *entryName = stringTable + nameOffset;

      bool match = false;
      if (resName != nullptr && std::strcmp(entryName, resName) == 0) {
        match = true;
      } else if (resIdx >= 0 && static_cast<s32>(i) == resIdx) {
        match = true;
      }

      if (match) {
        u32 dataOffset = read_big_endian_u32(entryPtr + 0x08);
        u32 dataSize = read_big_endian_u32(entryPtr + 0x0C);
        u32 allocSize = std::max<u32>(dataSize, 0x4000);
        bmdBuffer = allocAligned32(allocSize);
        if (bmdBuffer != nullptr) {
          std::memcpy(bmdBuffer, archiveData + dataOffset, dataSize);
        }
        break;
      }
    }
  }

  JKRFree(arcBuf);

  if (bmdBuffer == nullptr) {
  }

  return bmdBuffer;
}

static void loadQuiverModel(const LogService *log_svc, ModContext *mod_ctx) {
  u8 arrowMax = dComIfGs_getArrowMax();
  int targetQuiverType = (arrowMax >= 100) ? 3 : ((arrowMax >= 60) ? 2 : 1);
  const char *quiverArcName =
      (arrowMax >= 100) ? "O_gD_quL3"
                        : ((arrowMax >= 60) ? "O_gD_quL2" : "O_gD_quL1");

  if (s_loadedQuiverType != targetQuiverType) {
    s_customQuiverModel = nullptr;
    freeAligned32(s_quiverRawData);
    s_quiverRawData = nullptr;
    s_gearShaderCapable[1] = false;
    s_loadedQuiverType = targetQuiverType;
  }

  if (s_customQuiverModel != nullptr) {
    return;
  }

  char dvdPath[64];
  std::snprintf(dvdPath, sizeof(dvdPath), "/res/Object/%s.arc", quiverArcName);

  freeAligned32(s_quiverRawData);
  s_quiverRawData = loadPristineBmdFromDvd(dvdPath, "O_gD_quiver.bmd", 3);
  if (s_quiverRawData == nullptr) {
    return;
  }

  J3DModelData *modelData = dRes_info_c::loaderBasicBmd('BMWR', s_quiverRawData);
  if (modelData == nullptr) {
    freeAligned32(s_quiverRawData);
    s_quiverRawData = nullptr;
    return;
  }

  safeOnWarpMaterial(modelData);
  s_customQuiverModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084 | 0x2000400);
  safeOffWarpMaterial(modelData);

  if (s_customQuiverModel != nullptr) {
    s_gearShaderCapable[1] = true;
    s_customQuiverModel->setBaseScale(cXyz(1.25f, 1.25f, 1.25f));
    enable_material_fog(s_customQuiverModel);
  }
}

struct LanternRetrofitInfo {
  u32 mMagic;
  J3DTevBlock4 *mFreshBlock;
  J3DTevBlock *mVanillaBlock;
  J3DDisplayListObj *mVanillaSharedDLObj;
  J3DDisplayListObj *mRetrofittedDLObj;
};

static J3DTexCoordInfo l_lanternTexCoordInfo = {0x00, 0x00, 0x27};
static J3DTevOrderInfo l_lanternTevOrderInfo = {0x00, 0x03, 0xFF, 0x00};
static const J3DTevStageInfo l_lanternTevStageInfo = {
    0x05, 0x0F, 0x08, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x07, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00,
};
static const J3DAlphaCompInfo l_lanternAlphaCompInfo = {0x04, 0x80, 0x00, 0x03, 0xFF};
static const J3DTexMtxInfo l_lanternTexMtxInfo = {
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

struct GearWarpState {
  bool visible;
  f32 scale;
};

static GearWarpState s_bowWarp    = {true, 1.0f};
static GearWarpState s_quiverWarp = {true, 1.0f};
static GearWarpState s_lanternWarp= {true, 1.0f};

static J3DTevBlock *s_vanillaTevBlocks[8] = {nullptr};
static J3DDisplayListObj *s_vanillaSharedDLObj[8] = {nullptr};

static int countLanternShapeMtxAttrs(J3DShape *shape) {
  if (shape == nullptr) return 0;
  int count = 0;
  for (GXVtxDescList *desc = shape->getVtxDesc(); desc != nullptr && desc->attr != GX_VA_NULL; ++desc) {
    if (desc->attr >= GX_VA_TEX0MTXIDX && desc->attr <= GX_VA_TEX7MTXIDX) {
      count++;
    }
  }
  return count;
}

static void sanitizeLanternShapeVcd(J3DShape *shape) {
  if (shape == nullptr) return;
  GXVtxDescList *vtxDesc = shape->getVtxDesc();
  if (vtxDesc == nullptr) return;

  int mtxAttrCount = countLanternShapeMtxAttrs(shape);
  if (mtxAttrCount <= 1) {
    return;
  }

  GXVtxDescList *dst = vtxDesc;
  bool keptFirst = false;
  for (GXVtxDescList *src = vtxDesc; src->attr != GX_VA_NULL; ++src) {
    if (src->attr >= GX_VA_TEX0MTXIDX && src->attr <= GX_VA_TEX7MTXIDX) {
      if (!keptFirst) {
        *dst++ = *src;
        keptFirst = true;
      }
    } else {
      *dst++ = *src;
    }
  }
  dst->attr = GX_VA_NULL;
  dst->type = GX_NONE;

  shape->makeVcdVatCmd();
  J3DShape::resetVcdVatCache();
}

struct LanternBmdBlock {
  BE(u32) mBlockType;
  BE(u32) mBlockSize;
};

struct LanternBmdHeader {
  BE(u32) mMagic1;
  BE(u32) mMagic2;
  u8 field_0x08[4];
  BE(u32) mBlockNum;
  u8 field_0x10[0x1C - 0x10];
  int field_0x1c;
  LanternBmdBlock mBlocks[1];
};

struct LanternTexBlock : public LanternBmdBlock {
  BE(u16) mTextureNum;
  BE(u16) mPad;
  BE(u32) mpTextureRes;
  BE(u32) mpNameTable;
};

static void healLanternTextures(J3DModelData *modelData) {
  if (modelData == nullptr) return;
  const void *rawData = modelData->getRawData();
  if (rawData == nullptr) return;

  const LanternBmdHeader *fileData = reinterpret_cast<const LanternBmdHeader *>(rawData);
  const LanternBmdBlock *block = fileData->mBlocks;
  const LanternTexBlock *texBlock = nullptr;
  u32 blockNum = fileData->mBlockNum;
  for (u32 b = 0; b < blockNum; ++b) {
    if (block->mBlockType == 'TEX1') {
      texBlock = reinterpret_cast<const LanternTexBlock *>(block);
      break;
    }
    block = reinterpret_cast<const LanternBmdBlock *>(
        reinterpret_cast<uintptr_t>(block) + static_cast<u32>(block->mBlockSize));
  }

  if (texBlock == nullptr) return;

  const ResTIMG *origRes = reinterpret_cast<const ResTIMG *>(
      reinterpret_cast<uintptr_t>(texBlock) + static_cast<u32>(texBlock->mpTextureRes));
  if (origRes == nullptr) return;

  J3DTexture *texture = modelData->getTexture();
  if (texture == nullptr || texture->getNum() == 0) return;

  texture->setResTIMG(0, *origRes);

  ResTIMG *resTimg = static_cast<ResTIMG *>(
      dComIfG_getObjectRes("Always", dRes_ID_ALWAYS_BTI_WARP_TEX_e));
  if (resTimg != nullptr && texture->getNum() > 1) {
    texture->setResTIMG(texture->getNum() - 1, *resTimg);
  }

  u16 matNum = modelData->getMaterialNum();
  for (u16 i = 0; i < matNum; ++i) {
    J3DMaterial *mat = modelData->getMaterialNodePointer(i);
    if (mat != nullptr && mat->getTevBlock() != nullptr) {
      if (mat->getTevBlock()->getType() == 'TVB4') {
        mat->getTevBlock()->setTexNo(3, texture->getNum() - 1);
      }
    }
  }
}

static bool isLanternShapeRetrofitted(J3DShape *shape) {
  if (shape == nullptr) return false;
  for (GXVtxDescList *desc = shape->getVtxDesc(); desc != nullptr && desc->attr != GX_VA_NULL; ++desc) {
    if (desc->attr >= GX_VA_TEX0MTXIDX && desc->attr <= GX_VA_TEX7MTXIDX) {
      return true;
    }
  }
  return false;
}

static bool lanternInfoBelongsToModel(J3DModelData *modelData, const LanternRetrofitInfo *info) {
  if (modelData == nullptr || info == nullptr) return false;
  const u16 matNum = modelData->getMaterialNum();
  for (u16 i = 0; i < matNum && i < 8; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    if (material != nullptr &&
        material->getTevBlock() == reinterpret_cast<J3DTevBlock *>(info->mFreshBlock)) {
      return true;
    }
  }
  return false;
}

static void restampLanternVtables() {
  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));
  if (alink == nullptr || alink->mpKanteraModel == nullptr) return;
  J3DModelData *modelData = alink->mpKanteraModel->getModelData();
  if (modelData == nullptr) return;

  J3DTevBlock4 dummy;
  void *currentVtable = *reinterpret_cast<void **>(&dummy);

  const u16 matNum = modelData->getMaterialNum();
  for (u16 i = 0; i < matNum && i < 8; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    J3DMatPacket *matPacket = alink->mpKanteraModel->getMatPacket(i);
    if (material == nullptr || matPacket == nullptr) continue;
    LanternRetrofitInfo *info =
        reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea());
    if (info != nullptr && info->mMagic == 0x4C54524E && info->mFreshBlock != nullptr &&
        lanternInfoBelongsToModel(modelData, info)) {
      *reinterpret_cast<void **>(info->mFreshBlock) = currentVtable;
      material->mTevBlock = info->mFreshBlock;
    }
  }
}

static bool isLanternModelAlreadyUpgraded(J3DModel *lanternModel, J3DModelData *modelData) {
  if (modelData == nullptr) return false;
  bool structurallyRetrofitted = false;
  if (modelData->getMaterialNum() > 0) {
    J3DMaterial *mat0 = modelData->getMaterialNodePointer(0);
    if (mat0 != nullptr) {
      J3DTexGenBlock *texGen = mat0->getTexGenBlock();
      if (texGen != nullptr && texGen->getTexMtx(1) != nullptr) {
        structurallyRetrofitted = true;
      }
    }
  }
  if (!structurallyRetrofitted) return false;

  if (lanternModel != nullptr) {
    J3DMatPacket *matPacket = lanternModel->getMatPacket(0);
    if (matPacket != nullptr) {
      LanternRetrofitInfo *info = reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea());
      if (info != nullptr && info->mMagic == 0x4C54524E && info->mFreshBlock != nullptr &&
          lanternInfoBelongsToModel(modelData, info)) {
        return true;
      }
    }
  }
  return false;
}

static bool addDormantWarpStage(J3DModel *lanternModel, J3DModelData *modelData) {
  if (isLanternModelAlreadyUpgraded(lanternModel, modelData)) {
    healLanternTextures(modelData);
    return true;
  }

  const u16 materialNum = modelData->getMaterialNum();
  if (materialNum == 0 || materialNum > 8) {
    return false;
  }

  for (u16 i = 0; i < materialNum; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    if (material == nullptr || material->getTevBlock() == nullptr ||
        material->getTexGenBlock() == nullptr || material->getShape() == nullptr ||
        material->getPEBlock() == nullptr || material->getPEBlock()->getAlphaComp() == nullptr) {
      return false;
    }
  }

  ResTIMG *resTimg = static_cast<ResTIMG *>(
      dComIfG_getObjectRes("Always", dRes_ID_ALWAYS_BTI_WARP_TEX_e));
  J3DTexture *texture = modelData->getTexture();
  if (resTimg == nullptr || texture == nullptr) {
    return false;
  }

  JKRHeap *rootHeap = JKRHeap::getRootHeap();
  JKRHeap *oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;

  J3DTevBlock4 *freshBlocks[8] = {};
  for (u16 i = 0; i < materialNum; ++i) {
    freshBlocks[i] = JKR_NEW J3DTevBlock4();
    if (freshBlocks[i] == nullptr) {
      for (u16 j = 0; j < i; ++j) {
        JKR_DELETE(freshBlocks[j]);
      }
      if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
      return false;
    }
  }

  J3DTexMtx *newTexMtx = JKR_NEW J3DTexMtx(l_lanternTexMtxInfo);
  if (newTexMtx == nullptr) {
    for (u16 j = 0; j < materialNum; ++j) {
      JKR_DELETE(freshBlocks[j]);
    }
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    return false;
  }

  u16 textureNum = texture->getNum();
  if (textureNum == 1) {
    texture->addResTIMG(1, resTimg - textureNum);
    textureNum = texture->getNum();
  }
  u16 warpTexIdx = textureNum > 1 ? textureNum - 1 : 0;
  healLanternTextures(modelData);

  for (u16 i = 0; i < materialNum; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    J3DTevBlock *tevBlock = material->getTevBlock();

    u8 stageNum = tevBlock->getTevStageNum();
    for (u8 s = 0; s < stageNum && s < 4; ++s) {
      freshBlocks[i]->setTexNo(s, tevBlock->getTexNo(s));
      freshBlocks[i]->setTevOrder(s, *tevBlock->getTevOrder(s));
      freshBlocks[i]->setTevStage(s, *tevBlock->getTevStage(s));
      freshBlocks[i]->setTevKColorSel(s, tevBlock->getTevKColorSel(s));
      freshBlocks[i]->setTevKAlphaSel(s, tevBlock->getTevKAlphaSel(s));
      freshBlocks[i]->setTevSwapModeTable(s, *tevBlock->getTevSwapModeTable(s));
      freshBlocks[i]->setIndTevStage(s, *tevBlock->getIndTevStage(s));
    }
    for (u8 c = 0; c < 4; ++c) {
      freshBlocks[i]->setTevColor(c, *tevBlock->getTevColor(c));
      freshBlocks[i]->setTevKColor(c, *tevBlock->getTevKColor(c));
    }
    freshBlocks[i]->setTevStageNum(stageNum);

    if (s_vanillaTevBlocks[i] == nullptr) {
      s_vanillaTevBlocks[i] = tevBlock;
    }
    LanternRetrofitInfo *info = JKR_NEW LanternRetrofitInfo();
    if (info != nullptr) {
      info->mMagic = 0x4C54524E;
      info->mFreshBlock = freshBlocks[i];
      info->mVanillaBlock = tevBlock;
      info->mVanillaSharedDLObj = material->mSharedDLObj;
      info->mRetrofittedDLObj = nullptr;
    }
    J3DMatPacket *matPacket = lanternModel->getMatPacket(i);
    if (matPacket != nullptr) {
      matPacket->setUserArea(reinterpret_cast<uintptr_t>(info));
    }

    material->mTevBlock = freshBlocks[i];
    tevBlock = freshBlocks[i];

    J3DTexGenBlock *texGenBlock = material->getTexGenBlock();
    u32 texGenNum = texGenBlock->getTexGenNum();
    u8 tevStageNum = tevBlock->getTevStageNum();

    J3DTexCoord *coord = texGenBlock->getTexCoord(texGenNum);
    l_lanternTexCoordInfo.mTexGenMtx = texGenNum * 3 + GX_TEXMTX0;
    coord->setTexCoordInfo(l_lanternTexCoordInfo);
    coord->resetTexMtxReg();

    texGenBlock->setTexGenNum(texGenNum + 1);
    texGenBlock->setTexMtx(texGenNum, newTexMtx);

    l_lanternTevOrderInfo.mTexCoord = texGenNum;
    tevBlock->setTexNo(3, warpTexIdx);
    tevBlock->setTevOrder(tevStageNum, J3DTevOrder(l_lanternTevOrderInfo));
    tevBlock->setTevStage(tevStageNum, J3DTevStage(l_lanternTevStageInfo));
    tevBlock->setTevStageNum(tevStageNum + 1);

    J3DShape *shape = material->getShape();
    if (shape != nullptr && !isLanternShapeRetrofitted(shape)) {
      GXAttr attr = static_cast<GXAttr>(texGenNum + GX_VA_TEX0MTXIDX);
      shape->addTexMtxIndexInDL(attr, 0);
      shape->addTexMtxIndexInVcd(attr);
    }

    J3DPEBlock *peBlock = material->getPEBlock();
    peBlock->getAlphaComp()->setAlphaCompInfo(l_lanternAlphaCompInfo);
    peBlock->setZCompLoc(static_cast<u8>(0));
  }

  if (oldHeap != nullptr) {
    mDoExt_setCurrentHeap(oldHeap);
  }
  return true;
}

static bool ensureLanternWarpCapability(J3DModel *lanternModel) {
  if (!s_gearShaderUsable || lanternModel == nullptr) {
    return false;
  }
  J3DModelData *modelData = lanternModel->getModelData();
  if (modelData == nullptr) {
    return false;
  }
  if (modelData == s_lanternWarpModelData && s_gearShaderCapable[2]) {
    return true;
  }

  if (isLanternModelAlreadyUpgraded(lanternModel, modelData)) {
    J3DTevBlock4 dummy;
    void *currentVtable = *reinterpret_cast<void **>(&dummy);
    u16 materialNum = modelData->getMaterialNum();
    for (u16 i = 0; i < materialNum && i < 8; ++i) {
      J3DMaterial *material = modelData->getMaterialNodePointer(i);
      J3DMatPacket *matPacket = lanternModel->getMatPacket(i);
      if (material == nullptr || matPacket == nullptr) continue;

      LanternRetrofitInfo *info = reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea());
      if (info != nullptr && info->mMagic == 0x4C54524E && info->mFreshBlock != nullptr &&
          lanternInfoBelongsToModel(modelData, info)) {
        *reinterpret_cast<void **>(info->mFreshBlock) = currentVtable;
        material->mTevBlock = info->mFreshBlock;
      }

      J3DPEBlock *peBlock = material->getPEBlock();
      if (peBlock != nullptr && peBlock->getAlphaComp() != nullptr) {
        peBlock->getAlphaComp()->setAlphaCompInfo(l_lanternAlphaCompInfo);
        peBlock->setZCompLoc(static_cast<u8>(0));
      }
    }

    healLanternTextures(modelData);
    J3DShape::resetVcdVatCache();
    safeOffWarpMaterial(modelData);

    if ((lanternModel->mDiffFlag & 0x2000400) == 0) {
      JKRHeap *rootHeap = JKRHeap::getRootHeap();
      JKRHeap *oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;
      safeOnWarpMaterial(modelData);
      lanternModel->newDifferedDisplayList(0x11000084 | 0x2000400);
      safeOffWarpMaterial(modelData);
      if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    }

    s_gearShaderCapable[2] = true;
    s_gearModelWarpOn[2] = false;
    s_lanternWarpModelData = modelData;
    s_lanternWarp = {true, 1.0f};
    return true;
  }

  s_lanternWarpModelData = modelData;
  s_gearShaderCapable[2] = false;
  s_gearModelWarpOn[2] = false;

  if (!addDormantWarpStage(lanternModel, modelData)) {
    return false;
  }

  JKRHeap *rootHeap = JKRHeap::getRootHeap();
  JKRHeap *oldHeap = (rootHeap != nullptr) ? mDoExt_setCurrentHeap(rootHeap) : nullptr;

  u16 materialNum = modelData->getMaterialNum();
  for (u16 i = 0; i < materialNum && i < 8; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    if (s_vanillaSharedDLObj[i] == nullptr) {
      s_vanillaSharedDLObj[i] = material->mSharedDLObj;
    }
    J3DMatPacket *matPacket = lanternModel->getMatPacket(i);
    LanternRetrofitInfo *info =
        matPacket ? reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea()) : nullptr;
    if (info != nullptr && info->mMagic == 0x4C54524E &&
        lanternInfoBelongsToModel(modelData, info)) {
      info->mVanillaSharedDLObj = material->mSharedDLObj;
    }
    material->mSharedDLObj = nullptr;
  }

  if (modelData->newSharedDisplayList(J3DMdlFlag_UseSingleDL) != kJ3DError_Success) {
    for (u16 i = 0; i < materialNum && i < 8; ++i) {
      modelData->getMaterialNodePointer(i)->mSharedDLObj = s_vanillaSharedDLObj[i];
    }
    if (oldHeap != nullptr) mDoExt_setCurrentHeap(oldHeap);
    return false;
  }
  modelData->simpleCalcMaterial(const_cast<MtxP>(j3dDefaultMtx));
  modelData->makeSharedDL();

  for (u16 i = 0; i < materialNum; ++i) {
    J3DMaterial *material = modelData->getMaterialNodePointer(i);
    J3DMatPacket *matPacket = lanternModel->getMatPacket(i);
    if (matPacket != nullptr && material != nullptr) {
      matPacket->setDisplayListObj(material->getSharedDisplayListObj());
      LanternRetrofitInfo *info =
          reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea());
      if (info != nullptr && info->mMagic == 0x4C54524E &&
          lanternInfoBelongsToModel(modelData, info)) {
        info->mRetrofittedDLObj = material->getSharedDisplayListObj();
      }
    }
  }

  J3DShape::resetVcdVatCache();

  safeOnWarpMaterial(modelData);
  bool differedOk =
      lanternModel->newDifferedDisplayList(0x11000084 | 0x2000400) == kJ3DError_Success;
  safeOffWarpMaterial(modelData);

  if (oldHeap != nullptr) {
    mDoExt_setCurrentHeap(oldHeap);
  }

  if (!differedOk) {
    return false;
  }

  s_gearShaderCapable[2] = true;
  return true;
}

constexpr f32 kWipeYQuiverLantern = 4.6f - 0.48f * 5.1f;
constexpr f32 kWipeYBow           = 4.6f - 0.65f * 5.1f;

static f32 chase_scale(f32 value, f32 target, f32 rate) {
  value += (target - value) * rate;
  const f32 diff = value - target;
  if (diff > -0.01f && diff < 0.01f) {
    value = target;
  }
  return value;
}

static void gear_warp_step(GearWarpState &gear, f32 wipeY, f32 gearY,
                           bool arriving, bool inWarp) {
  if (!inWarp) {
    gear.visible = true;
    gear.scale = chase_scale(gear.scale, 1.0f, 0.3f);
    return;
  }

  if (arriving) {
    gear.visible = wipeY > gearY;
    gear.scale = chase_scale(gear.scale, gear.visible ? 1.0f : 0.0f, 0.22f);
  } else {
    gear.visible = wipeY > gearY + 0.35f;
    gear.scale = chase_scale(gear.scale, gear.visible ? 1.0f : 0.0f, 0.3f);
  }
}

static void sync_gear_to_warp(daAlink_c *alink, bool &bow, bool &quiver,
                              bool &lantern) {
  static f32 s_prevWipeY = 4.6f;
  const bool inWarp = alink != nullptr && alink->mProcID == daAlink_c::PROC_WARP;

  f32 wipeY = 4.6f;
  bool arriving = true;
  if (inWarp) {
    wipeY = alink->field_0x347c;
    arriving = wipeY >= s_prevWipeY;
    s_prevWipeY = wipeY;
  } else {
    s_prevWipeY = 4.6f;
  }

  gear_warp_step(s_bowWarp, wipeY, kWipeYBow, arriving, inWarp);
  gear_warp_step(s_quiverWarp, wipeY, kWipeYQuiverLantern, arriving, inWarp);
  gear_warp_step(s_lanternWarp, wipeY, kWipeYQuiverLantern, arriving, inWarp);

  if (bow && !s_bowWarp.visible) bow = false;
  if (quiver && !s_quiverWarp.visible) quiver = false;
  if (lantern && !s_lanternWarp.visible) lantern = false;
}

static void renderLantern(daAlink_c *alink) {
  if (alink == nullptr || isWolfOrTransforming(alink)) {
    return;
  }

  if (alink->checkNoResetFlg2(static_cast<daPy_py_c::daPy_FLG2>(0x1)) ||
      alink->checkNoResetFlg2(static_cast<daPy_py_c::daPy_FLG2>(0x20000)) ||
      alink->mEquipItem == dItemNo_KANTERA_e) {
    return;
  }

  if (alink->mpLinkModel == nullptr ||
      alink->getClothesChangeWaitTimer() != 0) {
    return;
  }

  J3DModel *model = alink->mpKanteraModel;
  if (model == nullptr) {
    return;
  }

  J3DModelData *modelData = alink->mpLinkModel->getModelData();
  MtxP beltMtx = (modelData != nullptr && modelData->getJointNum() > 0x10)
                     ? alink->mpLinkModel->getAnmMtx(0x10)
                     : getBoneMtx(alink, "waist");
  if (beltMtx != nullptr) {
    mDoMtx_stack_c::copy(beltMtx);
    const f32 lanternY =
        (dComIfGs_getSelectEquipClothes() == dItemNo_WEAR_CASUAL_e) ? 5.0f : 4.5f;
    mDoMtx_stack_c::transM(-1.0f, lanternY, 9.0f);
    mDoMtx_stack_c::XYZrotM(cM_deg2s(-75.0f), cM_deg2s(62.0f), cM_deg2s(89.0f));
    model->setBaseScale(
        cXyz(s_lanternWarp.scale, s_lanternWarp.scale, s_lanternWarp.scale));
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    cXyz &flamePos = alink->mKandelaarFlamePos;
    if (flamePos.abs2() < 1.0f) {
      mDoMtx_multVecZero(mDoMtx_stack_c::get(), &flamePos);
      flamePos.y -= 17.0f;
    }

    model->calc();

    g_env_light.settingTevStruct_colget_player(&alink->tevStr);
    g_env_light.setLightTevColorType_MAJI(model, &alink->tevStr);
    mDoExt_modelUpdateDL(model);
    addModelShadow(alink, model);
  }
}

static void renderBow(daAlink_c *alink, bool shouldShowEquipment,
                      bool isBowInHand) {
  if (!shouldShowEquipment || isBowInHand || s_customBowModel == nullptr) {
    return;
  }

  MtxP bowMtx = alink->mSheathModel->getBaseTRMtx();

  if (bowMtx != nullptr) {
    mDoMtx_stack_c::copy(bowMtx);
    if (g_configVisibleEquipMirrorBow) {
      mDoMtx_stack_c::transM(22.0f, 2.0f, -4.0f);
      mDoMtx_stack_c::XYZrotM(degToS16(90.0f), degToS16(-56.0f),
                              degToS16(0.0f));
    } else {
      mDoMtx_stack_c::transM(25.0f, 2.0f, -14.0f);
      mDoMtx_stack_c::XYZrotM(degToS16(90.0f), degToS16(56.0f),
                              degToS16(0.0f));
    }

    cXyz scale(0.75f * s_bowWarp.scale, 0.75f * s_bowWarp.scale,
               0.75f * s_bowWarp.scale);
    s_customBowModel->setBaseScale(scale);
    s_customBowModel->setBaseTRMtx(mDoMtx_stack_c::get());
    s_customBowModel->calc();

    g_env_light.settingTevStruct_colget_player(&alink->tevStr);
    g_env_light.setLightTevColorType_MAJI(s_customBowModel, &alink->tevStr);
    mDoExt_modelUpdateDL(s_customBowModel);
    addModelShadow(alink, s_customBowModel);
  }
}

static void renderQuiver(daAlink_c* alink, bool shouldShowEquipment) {
  if (!shouldShowEquipment || s_customQuiverModel == nullptr) {
    return;
  }

  MtxP quiverMtx = getBoneMtx(alink, "waist");

  if (quiverMtx != nullptr) {
    mDoMtx_stack_c::copy(quiverMtx);

    f32 quiverScale;
    if (g_configVisibleEquipQuiverOnBelt) {
      if (s_loadedQuiverType == 3) {
        mDoMtx_stack_c::transM(25.0f, 5.0f, 23.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(0.0f), degToS16(120.0f), degToS16(135.0f));

        quiverScale = 0.73f;
      } else if (s_loadedQuiverType == 2) {
        mDoMtx_stack_c::transM(25.0f, 5.0f, 23.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(0.0f), degToS16(-15.0f), degToS16(135.0f));

        quiverScale = 0.73f;
      } else {
        mDoMtx_stack_c::transM(25.0f, 5.0f, 24.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(45.0f), degToS16(-95.0f), degToS16(90.0f));

        quiverScale = 0.73f;
      }
    } else {
      if (s_loadedQuiverType == 3) {
        mDoMtx_stack_c::transM(25.0f, 15.0f, 5.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(-70.0f), degToS16(0.0f), degToS16(90.0f));

        quiverScale = 1.1f;
      } else if (s_loadedQuiverType == 2) {
        mDoMtx_stack_c::transM(25.0f, 15.0f, 5.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(-90.0f), degToS16(30.0f), degToS16(0.0f));

        quiverScale = 1.1f;
      } else {
        mDoMtx_stack_c::transM(25.0f, 15.0f, 5.0f);
        mDoMtx_stack_c::XYZrotM(degToS16(-90.0f), degToS16(30.0f), degToS16(0.0f));

        quiverScale = 1.0f;
      }
    }

    s_customQuiverModel->setBaseScale(cXyz(quiverScale * s_quiverWarp.scale,
                                           quiverScale * s_quiverWarp.scale,
                                           quiverScale * s_quiverWarp.scale));

    s_customQuiverModel->setBaseTRMtx(mDoMtx_stack_c::get());
    s_customQuiverModel->calc();

    g_env_light.settingTevStruct_colget_player(&alink->tevStr);
    g_env_light.setLightTevColorType_MAJI(s_customQuiverModel, &alink->tevStr);
    mDoExt_modelUpdateDL(s_customQuiverModel);
    addModelShadow(alink, s_customQuiverModel);
  }
}

static const char *s_cachedArcName = nullptr;
static void *s_cachedLinkInstance = nullptr;
static J3DModel *s_cachedLinkModel = nullptr;
static char s_cachedStageName[16] = {0};
static s32 s_cachedRoomNo = -1;

static void invalidateEquipmentModels() {
  s_customBowModel = nullptr;
  s_customQuiverModel = nullptr;
  s_loadedQuiverType = 0;

  freeAligned32(s_bowRawData);
  freeAligned32(s_quiverRawData);

  for (int slot = 0; slot < 2; ++slot) {
    s_gearShaderCapable[slot] = false;
    s_gearModelWarpOn[slot] = false;
  }
  s_bowWarp = {true, 1.0f};
  s_quiverWarp = {true, 1.0f};
  s_lanternWarp = {true, 1.0f};
}

static bool syncEquipmentModelCache(daAlink_c *alink) {
  if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0) {
    invalidateEquipmentModels();
    s_cachedLinkInstance = nullptr;
    s_cachedLinkModel = nullptr;
    s_cachedArcName = nullptr;
    s_cachedStageName[0] = '\0';
    s_cachedRoomNo = -1;
    return false;
  }

  const char *stageName = dComIfGp_getStartStageName();
  int roomNo = fopAcM_GetRoomNo(alink);

  if (s_cachedLinkInstance != alink ||
      s_cachedLinkModel != alink->mpLinkModel ||
      s_cachedArcName != alink->mArcName ||
      (stageName != nullptr && std::strcmp(s_cachedStageName, stageName) != 0)) {
    invalidateEquipmentModels();
    s_cachedLinkInstance = alink;
    s_cachedLinkModel = alink->mpLinkModel;
    s_cachedArcName = alink->mArcName;
    if (stageName != nullptr) {
      std::strncpy(s_cachedStageName, stageName, sizeof(s_cachedStageName) - 1);
      s_cachedStageName[sizeof(s_cachedStageName) - 1] = '\0';
    } else {
      s_cachedStageName[0] = '\0';
    }
    s_cachedRoomNo = roomNo;
  }

  return true;
}

static bool isTitleOrMainMenu() {
  const char *stageName = dComIfGp_getStartStageName();
  if (stageName != nullptr) {
    if (std::strcmp(stageName, "F_SP102") == 0 ||
        std::strcmp(stageName, "title") == 0) {
      return true;
    }
  }
  return false;
}

static bool isSceneLoadStable() {
  const char *cur = dComIfGp_getStartStageName();
  const char *next = dComIfGp_getNextStageName();
  if (dComIfGp_isEnableNextStage() && next != nullptr && next[0] != '\0' &&
      (cur == nullptr || std::strcmp(next, cur) != 0)) {
    return false;
  }

  static char s_stableStage[16] = {0};
  static s32 s_stableRoom = -1;
  static int s_stableFrames = 0;

  const char *stage = (cur != nullptr) ? cur : "";
  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));
  s32 room = (alink != nullptr) ? fopAcM_GetRoomNo(alink) : -1;

  if (std::strncmp(s_stableStage, stage, sizeof(s_stableStage) - 1) != 0 ||
      s_stableRoom != room) {
    std::strncpy(s_stableStage, stage, sizeof(s_stableStage) - 1);
    s_stableStage[sizeof(s_stableStage) - 1] = '\0';
    s_stableRoom = room;
    s_stableFrames = 0;
    return false;
  }
  if (s_stableFrames < 10) {
    s_stableFrames++;
    return false;
  }
  return true;
}

static void on_alink_draw_post_impl(ModContext *, void *, void *, void *);

static void on_alink_draw_post(ModContext *ctx, void *args, void *retval, void *user) {
  VE_SEH_TRY {
    on_alink_draw_post_impl(ctx, args, retval, user);
  } VE_SEH_EXCEPT {
  }
}

static void on_alink_draw_post_impl(ModContext *, void *, void *, void *) {
  if (!g_configVisibleEquipmentEnabled || isTitleOrMainMenu()) {
    return;
  }

  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));
  if (!alink || !alink->mpLinkModel || alink->mpLinkModel->getModelData() == nullptr ||
      isWolfOrTransforming(alink)) {
    return;
  }

  if (alink->checkPlayerNoDraw()) {
    return;
  }

  if (isInWarpVisual(alink)) {
    return;
  }

  if (!syncEquipmentModelCache(alink)) {
    return;
  }

  if (!isSceneLoadStable()) {
    return;
  }

  bool shouldShowBow = g_configVisibleEquipShowBow && checkShouldShowBow();
  bool shouldShowQuiver = shouldShowBow;
  bool shouldShowLantern = g_configVisibleEquipShowLantern && checkShouldShowLantern();

  if (shouldShowBow) {
    if (s_customBowModel == nullptr) {
      loadBowModel(s_logSvc, s_modCtx);
    }
    if (s_customQuiverModel == nullptr) {
      loadQuiverModel(s_logSvc, s_modCtx);
    }
  }

  const bool inWarp = alink->mProcID == daAlink_c::PROC_WARP;

  J3DModel *lanternModel = alink->mpKanteraModel;
  bool lanternShaderReady = ensureLanternWarpCapability(lanternModel);

  J3DModelData *capableData[3] = {
      (s_customBowModel != nullptr) ? s_customBowModel->getModelData() : nullptr,
      (s_customQuiverModel != nullptr) ? s_customQuiverModel->getModelData() : nullptr,
      lanternShaderReady ? lanternModel->getModelData() : nullptr,
  };

  for (int slot = 0; slot < 3; ++slot) {
    if (capableData[slot] == nullptr || !s_gearShaderCapable[slot]) {
      continue;
    }
    if (inWarp != s_gearModelWarpOn[slot]) {
      if (inWarp) {
        safeOnWarpMaterial(capableData[slot]);
      } else {
        safeOffWarpMaterial(capableData[slot]);
      }
      s_gearModelWarpOn[slot] = inWarp;
      static const char *const kGearSlotNames[3] = {"bow", "quiver", "lantern"};
    }
  }

  sync_gear_to_warp(alink, shouldShowBow, shouldShowQuiver, shouldShowLantern);

  if (inWarp) {
    if (s_gearShaderCapable[0]) {
      s_bowWarp = {true, 1.0f};
      shouldShowBow = g_configVisibleEquipShowBow && checkShouldShowBow();
      if (s_customBowModel != nullptr && s_customBowModel->getModelData() != nullptr) {
        applyWarpSRT(s_customBowModel->getModelData(), alink->current.pos,
                     alink->field_0x3478, alink->field_0x347c, "bow");
      }
    }
    if (s_gearShaderCapable[1]) {
      s_quiverWarp = {true, 1.0f};
      shouldShowQuiver = shouldShowBow;
      if (s_customQuiverModel != nullptr && s_customQuiverModel->getModelData() != nullptr) {
        applyWarpSRT(s_customQuiverModel->getModelData(), alink->current.pos,
                     alink->field_0x3478, alink->field_0x347c, "quiver");
      }
    }
    if (s_gearShaderCapable[2] && lanternModel != nullptr) {
      s_lanternWarp = {true, 1.0f};
      shouldShowLantern = g_configVisibleEquipShowLantern && checkShouldShowLantern();
      J3DModelData *lanternData = lanternModel->getModelData();
      if (lanternData != nullptr) {
        applyWarpSRT(lanternData, alink->current.pos,
                     alink->field_0x3478, alink->field_0x347c, "lantern");
      }
    }
  }

  bool isBowInHand = alink->checkBowAndSlingItem(alink->mEquipItem) ||
                   (alink->mEquipItem == dItemNo_BOW_e);

  const dKy_tevstr_c &tev = alink->tevStr;
  GXColor fogCol;
  fogCol.r = tev.FogCol.r < 0 ? 0 : (tev.FogCol.r > 255 ? 255 : tev.FogCol.r);
  fogCol.g = tev.FogCol.g < 0 ? 0 : (tev.FogCol.g > 255 ? 255 : tev.FogCol.g);
  fogCol.b = tev.FogCol.b < 0 ? 0 : (tev.FogCol.b > 255 ? 255 : tev.FogCol.b);
  fogCol.a = 255;
  f32 viewNearZ = 1.0f, viewFarZ = 100000.0f;
  camera_process_class *cam = dComIfGp_getCamera(0);
  if (cam != nullptr) {
    viewNearZ = cam->view.near_;
    viewFarZ = cam->view.far_;
  }
  GXSetFog(GX_FOG_PERSP_LIN, tev.mFogStartZ, tev.mFogEndZ, viewNearZ, viewFarZ, fogCol);

  if (shouldShowBow) {
    renderBow(alink, shouldShowBow, isBowInHand);
  }
  if (shouldShowQuiver) {
    renderQuiver(alink, shouldShowQuiver);
  }
  if (shouldShowLantern) {
    renderLantern(alink);
  }

  GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, fogCol);
}

static void on_alink_status_window_draw_post(ModContext *, void *, void *, void *) {
  if (!g_configVisibleEquipmentEnabled || isTitleOrMainMenu()) {
    return;
  }

  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));
  if (!alink || !alink->checkStatusWindowDraw() || !alink->mpLinkModel ||
      alink->mpLinkModel->getModelData() == nullptr || isWolfOrTransforming(alink)) {
    return;
  }
  if (!syncEquipmentModelCache(alink)) {
    return;
  }

  s_inStatusWindow = true;

  bool shouldShowBow = g_configVisibleEquipShowBow && checkShouldShowBow();
  bool isBowInHand = alink->checkBowAndSlingItem(alink->mEquipItem) ||
                     (alink->mEquipItem == dItemNo_BOW_e);
  if (shouldShowBow) {
    if (s_customBowModel == nullptr) {
      loadBowModel(s_logSvc, s_modCtx);
    }
    if (s_customQuiverModel == nullptr) {
      loadQuiverModel(s_logSvc, s_modCtx);
    }
    renderBow(alink, shouldShowBow, isBowInHand);
    renderQuiver(alink, shouldShowBow);
  }
  if (g_configVisibleEquipShowLantern && checkShouldShowLantern()) {
    renderLantern(alink);
  }

  s_inStatusWindow = false;
}

ModResult init_visible_equipment(const HookService *hook_svc, ModError *) {
  invalidateEquipmentModels();
  s_cachedArcName = nullptr;
  restampLanternVtables();

  if (hook_svc) {
    mods::hook::add_post<AlinkDrawHook>(hook_svc, on_alink_draw_post);
    mods::hook::add_post<AlinkStatusWindowDrawHook>(
        hook_svc, on_alink_status_window_draw_post);
  }
  return MOD_OK;
}

void update_visible_equipment(const LogService *log_svc, ModContext *mod_ctx) {
  s_logSvc = log_svc;
  s_modCtx = mod_ctx;

  if (!g_configVisibleEquipmentEnabled || isTitleOrMainMenu()) {
    return;
  }

  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));

  if (!syncEquipmentModelCache(alink)) {
    return;
  }

  if (!alink->mpLinkModel || isWolfOrTransforming(alink)) {
    return;
  }

  bool stable = isSceneLoadStable();
  bool shouldShow = g_configVisibleEquipShowBow && checkShouldShowBow();

  if (!stable) {
    return;
  }

  if (shouldShow) {
    loadBowModel(log_svc, mod_ctx);
    loadQuiverModel(log_svc, mod_ctx);
  }
}

void draw_visible_equipment(const LogService *, ModContext *) {}

void shutdown_visible_equipment() {
  const LogService *log_svc = s_logSvc;
  ModContext *mod_ctx = s_modCtx;
  daAlink_c *alink = static_cast<daAlink_c *>(dComIfGp_getPlayer(0));
  J3DModel *liveLanternModel = (alink != nullptr) ? alink->mpKanteraModel : nullptr;
  const bool lanternAlive = s_lanternWarpModelData != nullptr && liveLanternModel != nullptr &&
                            liveLanternModel->getModelData() == s_lanternWarpModelData;
  if (s_lanternWarpModelData != nullptr) {
    if (lanternAlive) {
      VE_SEH_TRY {
        restampLanternVtables();
      } VE_SEH_EXCEPT {
      }
      u16 matNum = s_lanternWarpModelData->getMaterialNum();
      for (u16 i = 0; i < matNum && i < 8; ++i) {
        VE_SEH_TRY {
          J3DMaterial *material = s_lanternWarpModelData->getMaterialNodePointer(i);
          J3DMatPacket *matPacket = (alink != nullptr && alink->mpKanteraModel != nullptr)
                                        ? alink->mpKanteraModel->getMatPacket(i)
                                        : nullptr;
          LanternRetrofitInfo *info =
              matPacket ? reinterpret_cast<LanternRetrofitInfo *>(matPacket->getUserArea()) : nullptr;
          bool viaInfo = info != nullptr && info->mMagic == 0x4C54524E &&
                         info->mFreshBlock != nullptr &&
                         lanternInfoBelongsToModel(s_lanternWarpModelData, info);
          if (viaInfo) {
            J3DTevBlock *tev = material->getTevBlock();
            if (tev != nullptr) {
              u8 tevStageNum = tev->getTevStageNum();
              if (tevStageNum > 1) {
                J3DTevOrder *tevorder = tev->getTevOrder(tevStageNum - 1);
                if (tevorder != nullptr && tevorder->getTexMap() == 3) {
                  tev->setTevStageNum(tevStageNum - 1);
                  J3DTexGenBlock *texGenBlock = material->getTexGenBlock();
                  if (texGenBlock != nullptr && texGenBlock->getTexGenNum() > 1) {
                    texGenBlock->setTexGenNum(texGenBlock->getTexGenNum() - 1);
                  }
                }
              }
            }
          }
        } VE_SEH_EXCEPT {
        }
      }
    }
    s_lanternWarpModelData = nullptr;
  }
  for (int i = 0; i < 8; ++i) {
    s_vanillaTevBlocks[i] = nullptr;
    s_vanillaSharedDLObj[i] = nullptr;
  }
  s_gearShaderCapable[2] = false;
  s_gearModelWarpOn[2] = false;
  invalidateEquipmentModels();
  s_cachedArcName = nullptr;
}
