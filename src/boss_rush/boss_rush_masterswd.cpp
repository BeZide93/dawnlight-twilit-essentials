#include "boss_rush_masterswd.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush.hpp"
#include "boss_rush_texts.hpp"
#include "boss_rush_timer.hpp"
#include "../util.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle_name.h"
#include "SSystem/SComponent/c_math.h"
#include "m_Do/m_Do_lib.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"

#include <cstdio>
#include <cstring>

namespace {

static constexpr const char* kMasterSwordArc = "MstrSword";
static constexpr int kMasterSwordModelRes = 5;
static constexpr int kMasterSwordBtkRes = 11;
static constexpr int kMasterSwordBrkRes = 8;
static constexpr f32 kMasterSwordScale = 0.8f;
static constexpr f32 kMasterSwordRadius = 150.0f;
static constexpr f32 kMasterSwordLabelY = 200.0f;

static constexpr const char* kPedestalArcDir = "/res/Stage/F_SP117/";
static constexpr const char* kPedestalArc = "R02_00";
static constexpr const char* kPedestalModelRes = "model.bmd";
static constexpr const char* kPedestalMaterial = "bb_R02mastadaiza_v";
static constexpr f32 kPedestalBaseY = 1675.0f;
static constexpr f32 kPedestalTopY = 1700.0f;
static constexpr f32 kPedestalCenterX = -0.25f;
static constexpr f32 kPedestalCenterZ = -5435.25f;

static J3DModel* s_masterSwordModel = nullptr;
static mDoExt_btkAnm* s_masterSwordBtk = nullptr;
static mDoExt_brkAnm* s_masterSwordBrk = nullptr;
static bool s_masterSwordResolved = false;

static J3DModel* s_pedestalModel = nullptr;
static bool s_pedestalResolved = false;

static u32 s_masterSwordEfStar = 0;
static f32 s_labelFade = 0.0f;

J3DModel* create_pedestal_model() {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(kPedestalArc, kPedestalModelRes));
    if (modelData == nullptr || modelData->getMaterialName() == nullptr) {
        return nullptr;
    }

    bool found = false;
    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        J3DShape* shape = material != nullptr ? material->getShape() : nullptr;
        if (shape == nullptr) {
            continue;
        }
        const char* name = modelData->getMaterialName()->getName(i);
        if (name != nullptr && std::strcmp(name, kPedestalMaterial) == 0) {
            shape->show();
            found = true;
        } else {
            shape->hide();
        }
    }
    if (!found) {
        return nullptr;
    }

    return mDoExt_J3DModel__create(modelData, 0x80000, 0x11000284);
}

f32 master_sword_lift() {
    return s_pedestalModel != nullptr ? (kPedestalTopY - kPedestalBaseY) * kMasterSwordScale : 0.0f;
}

void stop_master_sword_emitter(u32& handle) {
    JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(handle);
    if (emitter != nullptr) {
        emitter->stopDrawParticle();
    }
    handle = 0;
}

}

void update_boss_rush_master_sword_effects() {
    if (s_masterSwordModel == nullptr || !is_in_boss_rush_chamber()) {
        return;
    }

    static constexpr int kTrailSweepFrames = 90;
    static int sweepFrame = 0;
    sweepFrame = (sweepFrame + 1) % kTrailSweepFrames;

    const f32 bottom = kBossChamberFloorY + master_sword_lift() + 15.0f;
    const f32 top = kBossChamberFloorY + master_sword_lift() + 140.0f;
    const f32 t = static_cast<f32>(sweepFrame) / static_cast<f32>(kTrailSweepFrames);
    cXyz trailPos(0.0f, bottom + (top - bottom) * t, 0.0f);

    daAlink_c* alink = daAlink_getAlinkActorClass();
    const dKy_tevstr_c* tev = (alink != nullptr) ? &alink->tevStr : nullptr;

    s_masterSwordEfStar = dComIfGp_particle_set(s_masterSwordEfStar, ID_ZF_J_FAIRY02_STAR, &trailPos, tev,
                                                nullptr, nullptr, 0xFF, nullptr, -1, nullptr, nullptr, nullptr);
}

void draw_boss_rush_master_sword(float floorY) {
    if (boss_rush_scene_load_stable() && !s_masterSwordResolved) {
        const int swordArcStatus = loadObjectArchive(kMasterSwordArc);
        if (swordArcStatus != 1) {
            s_masterSwordResolved = true;
            if (swordArcStatus == 0) {
                s_masterSwordModel = loadBmdFromArcIdx(kMasterSwordArc, kMasterSwordModelRes);
                if (s_masterSwordModel != nullptr && s_masterSwordModel->getModelData() != nullptr) {
                    s_masterSwordBtk = loadBtkFromArcIdx(kMasterSwordArc, kMasterSwordBtkRes,
                                                         s_masterSwordModel->getModelData());
                    s_masterSwordBrk = loadBrkFromArcIdx(kMasterSwordArc, kMasterSwordBrkRes,
                                                         s_masterSwordModel->getModelData());
                }
            }
        }
    }

    if (boss_rush_scene_load_stable() && !s_pedestalResolved) {
        const int pedestalArcStatus = loadArchiveFromDir(kPedestalArcDir, kPedestalArc);
        if (pedestalArcStatus != 1) {
            s_pedestalResolved = true;
            if (pedestalArcStatus == 0) {
                s_pedestalModel = create_pedestal_model();
            }
        }
    }

    if (s_masterSwordModel == nullptr || !s_pedestalResolved) {
        return;
    }

    if (s_pedestalModel != nullptr) {
        cXyz pedestalPos(kPedestalCenterX * kMasterSwordScale, floorY - kPedestalBaseY * kMasterSwordScale,
                         kPedestalCenterZ * kMasterSwordScale);
        renderModelAt(s_pedestalModel, pedestalPos, csXyz(0, cM_deg2s(180.0f), 0),
                      cXyz(kMasterSwordScale, kMasterSwordScale, kMasterSwordScale));
    }

    if (s_masterSwordBrk != nullptr) {
        s_masterSwordBrk->play();
        s_masterSwordBrk->entry(s_masterSwordModel->getModelData());
    }
    if (s_masterSwordBtk != nullptr) {
        s_masterSwordBtk->play();
        s_masterSwordBtk->entry(s_masterSwordModel->getModelData());
    }
    cXyz swordPos(0.0f, floorY + master_sword_lift(), 0.0f);
    csXyz swordAngle(0, cM_deg2s(180.0f), 0);
    renderModelAt(s_masterSwordModel, swordPos, swordAngle,
                  cXyz(kMasterSwordScale, kMasterSwordScale, kMasterSwordScale));
}

bool boss_rush_master_sword_near() {
    if (!is_in_boss_rush_chamber()) {
        return false;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return false;
    }
    const f32 dx = link->current.pos.x;
    const f32 dz = link->current.pos.z;
    return dx * dx + dz * dz < kMasterSwordRadius * kMasterSwordRadius;
}

void boss_rush_master_sword_reset_fade() {
    s_labelFade = 0.0f;
}

void draw_boss_rush_master_sword_label(const daAlink_c* link, float floorY) {
    cXyz pos(0.0f, floorY + kMasterSwordLabelY, 0.0f);

    f32 target = 0.0f;
    if (link != nullptr) {
        const f32 dx = link->current.pos.x;
        const f32 dz = link->current.pos.z;
        if (dx * dx + dz * dz < kMasterSwordRadius * kMasterSwordRadius) target = 1.0f;
    }
    s_labelFade += (target - s_labelFade) * 0.12f;
    if (s_labelFade < 0.004f) {
        s_labelFade = 0.0f;
        return;
    }

    Vec screenPos;
    mDoLib_project(&pos, &screenPos);

    if (screenPos.z >= 400000.0f || screenPos.x < -80.0f || screenPos.x > 720.0f ||
        screenPos.y < -80.0f || screenPos.y > 500.0f) {
        return;
    }

    const f32 nameCharW = 22.0f, nameCharH = 26.0f;
    const f32 locCharW = 14.0f, locCharH = 17.0f;
    const u8 nameA = static_cast<u8>(255.0f * s_labelFade);
    const u8 tA = static_cast<u8>(200.0f * s_labelFade);

    static const char title[] = "Master Rush";
    const f32 nameW = boss_rush_texts_measure_width(title, nameCharW);
    boss_rush_texts_draw_label(title, screenPos.x - nameW * 0.5f, screenPos.y - nameCharH - locCharH,
                               nameCharW, nameCharH,
                               JUtility::TColor(255, 236, 170, 255), JUtility::TColor(255, 190, 60, 255), nameA);

    if (!g_configBossRushTimer) {
        return;
    }

    char timeBuf[24];
    u32 totalCs = 0;
    if (boss_rush_timer_chain_best_cs(&totalCs)) {
        char t[16];
        boss_rush_timer_format(totalCs, t, sizeof(t));
        std::snprintf(timeBuf, sizeof(timeBuf), "Best  %s", t);
    } else {
        std::snprintf(timeBuf, sizeof(timeBuf), "Best  --:--.--");
    }
    const f32 tW = boss_rush_texts_measure_width(timeBuf, locCharW);
    boss_rush_texts_draw_label(timeBuf, screenPos.x - tW * 0.5f, screenPos.y - locCharH,
                               locCharW, locCharH,
                               JUtility::TColor(255, 226, 140, 255), JUtility::TColor(230, 170, 70, 255), tA);
}

void unload_boss_rush_master_sword() {
    stop_master_sword_emitter(s_masterSwordEfStar);
    if (s_masterSwordBtk != nullptr) {
        JKR_DELETE(s_masterSwordBtk);
        s_masterSwordBtk = nullptr;
    }
    if (s_masterSwordBrk != nullptr) {
        JKR_DELETE(s_masterSwordBrk);
        s_masterSwordBrk = nullptr;
    }
    if (s_masterSwordModel != nullptr) {
        JKR_DELETE(s_masterSwordModel);
        s_masterSwordModel = nullptr;
    }
    unloadObjectArchive(kMasterSwordArc);
    s_masterSwordResolved = false;
    if (s_pedestalModel != nullptr) {
        JKR_DELETE(s_pedestalModel);
        s_pedestalModel = nullptr;
    }
    unloadObjectArchive(kPedestalArc);
    s_pedestalResolved = false;
    boss_rush_master_sword_reset_fade();
}
