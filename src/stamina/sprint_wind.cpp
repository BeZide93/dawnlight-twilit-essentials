#include "sprint_wind.hpp"

#include "stamina_internal.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

#include "d/d_com_inf_game.h"
#include "d/d_particle.h"
#include "d/d_stage.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAResource.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "JSystem/JParticle/JPATexture.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#define SPRINT_WIND_LOG 0

extern const LogService* svc_log;

DEFINE_HOOK(&dPa_control_c::createScene, SprintWindSceneCreate);

namespace {

constexpr u16 kHorseSpeedEmitter = 0x8657;
constexpr u32 kBlockTdb1 = 0x54444231u;
constexpr u32 kCaptureHeapNeed = 256u * 1024u;
constexpr const char* kHorseParticlePath = "/res/Particle/Pscene001.jpc";
constexpr const char* kFrameBufferTexName = "dummy";

constexpr int kInjectResCapacity = 1024;
constexpr int kInjectTexCapacity = 2048;
constexpr int kInjectTdb1Capacity = 256;

struct Capture {
    bool built;
    JPAResourceManager* manager;
    JPAResource resource;
};

Capture s_capture{};

bool s_loadFailed = false;
u32 s_sceneCreateCount = 0;

u8* s_packBlob = nullptr;

JPAResource* s_injectRes = nullptr;
BE(u16)* s_injectTdb1 = nullptr;
JPAResource** s_resAry = nullptr;
JPATexture** s_texAry = nullptr;

void wind_log(const char* fmt, ...) {
#if SPRINT_WIND_LOG
    if (svc_log == nullptr || svc_log->info == nullptr) return;
    char msg[320];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    svc_log->info(mod_ctx, msg);
#else
    (void)fmt;
#endif
}

const char* current_stage_name() {
    const char* name = dComIfGp_getStartStageName();
    return name != nullptr ? name : "?";
}

void ensure_inject_storage() {
    if (s_injectRes == nullptr) {
        s_injectRes = new JPAResource();
        s_injectTdb1 = new BE(u16)[kInjectTdb1Capacity]{};
        s_resAry = new JPAResource*[kInjectResCapacity]{};
        s_texAry = new JPATexture*[kInjectTexCapacity]{};
    }
}

JKRHeap* pick_capture_heap() {
    JKRHeap* candidates[] = {JKRHeap::getSystemHeap(), JKRHeap::getRootHeap(),
                             mDoExt_getArchiveHeap()};
    const char* names[] = {"system", "root", "archive"};
    for (int i = 0; i < 3; ++i) {
        JKRHeap* heap = candidates[i];
        const u32 freeSize = heap != nullptr ? static_cast<u32>(heap->getFreeSize()) : 0;
        wind_log("[sprint-wind] heap %s=%p free=%u", names[i], heap, freeSize);
        if (heap != nullptr && freeSize >= kCaptureHeapNeed) {
            wind_log("[sprint-wind] using %s heap", names[i]);
            return heap;
        }
    }
    return nullptr;
}

void swap_frame_buffer_texture() {
    if (s_capture.manager == nullptr) return;
    ResTIMG* fb = mDoGph_gInf_c::getFrameBufferTimg();
    if (fb == nullptr) {
        wind_log("[sprint-wind] frame buffer timg is null, dummy texture not swapped");
        return;
    }
    const ResTIMG* old = s_capture.manager->swapTexture(fb, kFrameBufferTexName);
    wind_log("[sprint-wind] swapTexture(dummy -> framebuffer %p) %s", fb,
             old != nullptr ? "ok" : "NOT FOUND");
}

bool find_horse_resource(const u8* src, u32 texSection, const u8** outStart, u32* outSize,
                         u32* outTdb1Off, u32* outTexNum) {
    const u16 resCount = *(const BE<u16>*)(src + 8);
    u32 off = 0x10;
    for (u16 i = 0; i < resCount && off < texSection; ++i) {
        const u8* hdr = src + off;
        const u16 usrIdx = *(const BE<u16>*)(hdr);
        const u16 blockNum = *(const BE<u16>*)(hdr + 2);
        const u32 texNum = hdr[6];
        u32 body = off + 8;
        u32 tdb1Off = 0;
        for (u16 j = 0; j < blockNum; ++j) {
            const u32 blockSize = *(const BE<u32>*)(src + body + 4);
            if (blockSize < 8 || body + blockSize > texSection) {
                return false;
            }
            if (*(const BE<u32>*)(src + body) == kBlockTdb1) {
                tdb1Off = body - off;
            }
            body += blockSize;
        }
        if (usrIdx == kHorseSpeedEmitter) {
            if (texNum > 0 && tdb1Off == 0) {
                return false;
            }
            *outStart = hdr;
            *outSize = body - off;
            *outTdb1Off = tdb1Off;
            *outTexNum = texNum;
            return true;
        }
        off = body;
    }
    return false;
}

bool build_capture(const u8* src, u32 srcSize, JKRHeap* heap) {
    if (srcSize < 0x10) {
        wind_log("[sprint-wind] build: file too small (%u)", srcSize);
        return false;
    }

    const u32 texCount = *(const BE<u16>*)(src + 0xA);
    const u32 texSection = *(const BE<u32>*)(src + 0xC);
    wind_log("[sprint-wind] build: magic=%.8s res=%u tex=%u texSection=0x%X size=%u",
             reinterpret_cast<const char*>(src), static_cast<u32>(*(const BE<u16>*)(src + 8)),
             texCount, texSection, srcSize);
    if (texSection < 0x10 || texSection > srcSize) {
        wind_log("[sprint-wind] build: bad texSection");
        return false;
    }

    const u8* resStart = nullptr;
    u32 resSize = 0;
    u32 tdb1Off = 0;
    u32 texNum = 0;
    if (!find_horse_resource(src, texSection, &resStart, &resSize, &tdb1Off, &texNum)) {
        wind_log("[sprint-wind] build: 0x8657 not found in file");
        return false;
    }
    wind_log("[sprint-wind] build: found 0x8657 size=%u texNum=%u tdb1Off=%u", resSize, texNum,
             tdb1Off);
    if (texNum == 0 || texNum > kInjectTdb1Capacity) {
        wind_log("[sprint-wind] build: bad texNum");
        return false;
    }

    const BE(u16)* tdb1 = (const BE(u16)*)(resStart + tdb1Off + 8);

    u16* oldToNew = new u16[texCount];
    for (u32 i = 0; i < texCount; ++i) {
        oldToNew[i] = 0xFFFF;
    }
    u16 newIndices[kInjectTdb1Capacity];
    u16 needed[kInjectTdb1Capacity];
    u16 mapped = 0;
    for (u32 i = 0; i < texNum; ++i) {
        const u16 oldIdx = tdb1[i];
        if (oldIdx >= texCount) {
            delete[] oldToNew;
            wind_log("[sprint-wind] build: tdb1[%u]=%u out of range", i,
                     static_cast<u32>(oldIdx));
            return false;
        }
        if (oldToNew[oldIdx] == 0xFFFF) {
            oldToNew[oldIdx] = mapped;
            needed[mapped] = oldIdx;
            ++mapped;
        }
        newIndices[i] = oldToNew[oldIdx];
    }
    if (mapped == 0) {
        delete[] oldToNew;
        wind_log("[sprint-wind] build: no textures mapped");
        return false;
    }

    u32 texOffsets[kInjectTdb1Capacity];
    u32 texSizes[kInjectTdb1Capacity];
    u32 texBytes = 0;
    u32 texOff = texSection;
    u32 found = 0;
    for (u32 idx = 0; idx < texCount && found < mapped; ++idx) {
        if (texOff + 8 > srcSize) {
            delete[] oldToNew;
            wind_log("[sprint-wind] build: texture %u past end of file", idx);
            return false;
        }
        const u32 blockSize = *(const BE<u32>*)(src + texOff + 4);
        if (blockSize < 8 || texOff + blockSize > srcSize) {
            delete[] oldToNew;
            wind_log("[sprint-wind] build: texture %u bad size %u", idx, blockSize);
            return false;
        }
        for (u16 n = 0; n < mapped; ++n) {
            if (needed[n] == idx) {
                texOffsets[n] = texOff;
                texSizes[n] = blockSize;
                texBytes += blockSize;
                ++found;
                break;
            }
        }
        texOff += blockSize;
    }
    delete[] oldToNew;
    if (found != mapped) {
        wind_log("[sprint-wind] build: textures found %u of %u", found, static_cast<u32>(mapped));
        return false;
    }

    u8* blob = new u8[0x10 + resSize + texBytes];
    std::memcpy(blob, src, 0x10);
    *(BE<u16>*)(blob + 8) = static_cast<u16>(1);
    *(BE<u16>*)(blob + 0xA) = mapped;
    *(BE<u32>*)(blob + 0xC) = 0x10 + resSize;
    std::memcpy(blob + 0x10, resStart, resSize);
    BE(u16)* patchedTdb1 = (BE(u16)*)(blob + 0x10 + tdb1Off + 8);
    for (u32 i = 0; i < texNum; ++i) {
        patchedTdb1[i] = newIndices[i];
    }
    u32 out = 0x10 + resSize;
    for (u16 n = 0; n < mapped; ++n) {
        std::memcpy(blob + out, src + texOffsets[n], texSizes[n]);
        out += texSizes[n];
    }

    JPAResourceManager* manager = new JPAResourceManager(blob, heap);
    JPAResource* source = manager->getResource(kHorseSpeedEmitter);
    if (source == nullptr) {
        wind_log("[sprint-wind] build: manager has no 0x8657 (resRegNum=%d texRegNum=%d)",
                 static_cast<int>(manager->resRegNum), static_cast<int>(manager->texRegNum));
        return false;
    }

    s_packBlob = blob;
    s_capture.manager = manager;
    s_capture.resource = *source;
    s_capture.built = true;
    for (int i = 0; i < manager->texRegNum; ++i) {
        wind_log("[sprint-wind] build: tex[%d]=%s", i, manager->pTexAry[i]->getName());
    }
    wind_log("[sprint-wind] build: OK (res texNum=%d, mapped tex=%u)",
             static_cast<int>(s_capture.resource.texNum), static_cast<u32>(mapped));
    return true;
}

bool load_horse_speed_resource() {
    if (s_capture.built) {
        return true;
    }
    if (s_loadFailed) {
        return false;
    }

    JKRHeap* heap = pick_capture_heap();
    if (heap == nullptr) {
        wind_log("[sprint-wind] load: no heap with %u bytes free, retry next scene",
                 kCaptureHeapNeed);
        return false;
    }

    u32 fileSize = 0;
    void* file = JKRDvdRipper::loadToMainRAM(kHorseParticlePath, nullptr, EXPAND_SWITCH_UNKNOWN1, 0,
                                             heap, JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0,
                                             nullptr, &fileSize);
    wind_log("[sprint-wind] load: %s -> %p (%u bytes)", kHorseParticlePath, file, fileSize);
    if (file == nullptr) {
        return false;
    }
    const bool built = build_capture(static_cast<const u8*>(file), fileSize, heap);
    JKRFree(file);
    if (!built) {
        s_loadFailed = true;
        wind_log("[sprint-wind] load: build FAILED, giving up");
    }
    return built;
}

void inject_into_scene(JPAResourceManager* sceneRM) {
    const int texNum = s_capture.resource.texNum;
    if (sceneRM->resRegNum + 1 > kInjectResCapacity ||
        sceneRM->texRegNum + texNum > kInjectTexCapacity || texNum > kInjectTdb1Capacity) {
        wind_log("[sprint-wind] inject: capacity exceeded (res=%d tex=%d texNum=%d)",
                 static_cast<int>(sceneRM->resRegNum), static_cast<int>(sceneRM->texRegNum),
                 texNum);
        return;
    }

    ensure_inject_storage();

    for (int i = 0; i < sceneRM->resRegNum; ++i) {
        s_resAry[i] = sceneRM->pResAry[i];
    }

    const u16 texBase = sceneRM->texRegNum;
    *s_injectRes = s_capture.resource;
    for (int i = 0; i < s_capture.resource.texNum; ++i) {
        const u16 srcIdx = s_capture.resource.mpTDB1[i];
        s_texAry[texBase + i] = s_capture.manager->pTexAry[srcIdx];
        s_injectTdb1[i] = texBase + i;
    }
    s_injectRes->mpTDB1 = s_injectTdb1;
    s_resAry[sceneRM->resRegNum] = s_injectRes;

    for (int i = 0; i < sceneRM->texRegNum; ++i) {
        s_texAry[i] = sceneRM->pTexAry[i];
    }

    sceneRM->pResAry = s_resAry;
    sceneRM->resMaxNum = kInjectResCapacity;
    sceneRM->resRegNum += 1;
    sceneRM->pTexAry = s_texAry;
    sceneRM->texMaxNum = kInjectTexCapacity;
    sceneRM->texRegNum = texBase + texNum;

    wind_log("[sprint-wind] injected: resRegNum=%d texRegNum=%d found=%d",
             static_cast<int>(sceneRM->resRegNum), static_cast<int>(sceneRM->texRegNum),
             sceneRM->checkUserIndexDuplication(kHorseSpeedEmitter) ? 1 : 0);
}

void scene_create_post(ModContext*, void*, void*, void*) {
    ++s_sceneCreateCount;
    dPa_control_c* particle = g_dComIfG_gameInfo.play.getParticle();
    JPAEmitterManager* emitterMgr = dPa_control_c::getEmitterManager();
    wind_log("[sprint-wind] createScene #%u stage=%s particle=%p emitterMgr=%p",
             s_sceneCreateCount, current_stage_name(), particle, emitterMgr);
    if (particle == nullptr || emitterMgr == nullptr) {
        return;
    }

    if (!load_horse_speed_resource()) {
        return;
    }

    swap_frame_buffer_texture();

    JPAResourceManager* sceneRM = emitterMgr->getResourceManager(static_cast<u8>(1));
    if (sceneRM == nullptr) {
        emitterMgr->entryResourceManager(s_capture.manager, 1);
        wind_log("[sprint-wind] scene has no particle RM, registered ours as RM 1");
        return;
    }

    if (sceneRM->checkUserIndexDuplication(kHorseSpeedEmitter)) {
        wind_log("[sprint-wind] scene RM already has 0x8657 natively (res=%d tex=%d)",
                 static_cast<int>(sceneRM->resRegNum), static_cast<int>(sceneRM->texRegNum));
        return;
    }
    inject_into_scene(sceneRM);
}

bool compute_sprint_wind_allowed(int* outType) {
    *outType = -1;
    stage_stag_info_class* stagInfo = dComIfGp_getStageStagInfo();
    if (stagInfo == nullptr) {
        return true;
    }

    const u32 stageType = dStage_stagInfo_GetSTType(stagInfo);
    *outType = static_cast<int>(stageType);
    if (stageType != ST_DUNGEON && stageType != ST_BOSS_ROOM) {
        return true;
    }

    const char* stageName = dComIfGp_getStartStageName();
    return stageName != nullptr && std::memcmp(stageName, "D_MN07", 6) == 0;
}

#if SPRINT_WIND_LOG
constexpr size_t kStageNameLen = 8;

char s_lastAllowedStage[kStageNameLen] = {};
int s_lastAllowed = -1;
char s_lastReportStage[kStageNameLen] = {};
int s_lastReport = -1;

bool stage_changed(char* last, const char* now) {
    if (std::strncmp(last, now, kStageNameLen - 1) == 0) return false;
    std::strncpy(last, now, kStageNameLen - 1);
    last[kStageNameLen - 1] = '\0';
    return true;
}
#endif

} // namespace

bool stamina_impl::sprint_wind_allowed() {
    int stageType = -1;
    const bool allowed = compute_sprint_wind_allowed(&stageType);
#if SPRINT_WIND_LOG
    const bool stageNew = stage_changed(s_lastAllowedStage, current_stage_name());
    if (stageNew || s_lastAllowed != (allowed ? 1 : 0)) {
        s_lastAllowed = allowed ? 1 : 0;
        wind_log("[sprint-wind] allowed=%d stage=%s type=%d", s_lastAllowed, s_lastAllowedStage,
                 stageType);
    }
#endif
    return allowed;
}

void stamina_impl::sprint_wind_report(unsigned int emitterId) {
#if SPRINT_WIND_LOG
    const int ok = emitterId != 0 ? 1 : 0;
    const bool stageNew = stage_changed(s_lastReportStage, current_stage_name());
    if (!stageNew && s_lastReport == ok) return;
    s_lastReport = ok;

    JPAEmitterManager* emitterMgr = dPa_control_c::getEmitterManager();
    JPAResourceManager* rm =
        emitterMgr != nullptr ? emitterMgr->getResourceManager(static_cast<u8>(1)) : nullptr;
    wind_log("[sprint-wind] spawn %s id=%u stage=%s rm1=%p has8657=%d captured=%d",
             ok ? "OK" : "FAILED", emitterId, s_lastReportStage, rm,
             rm != nullptr && rm->checkUserIndexDuplication(kHorseSpeedEmitter) ? 1 : 0,
             s_capture.built ? 1 : 0);
#else
    (void)emitterId;
#endif
}

ModResult init_sprint_wind(const HookService* hook_svc) {
    if (hook_svc == nullptr) {
        wind_log("[sprint-wind] no hook service");
        return MOD_OK;
    }
    if (mods::hook::add_post<SprintWindSceneCreate>(hook_svc, scene_create_post) != MOD_OK) {
        wind_log("[sprint-wind] createScene hook install FAILED");
        return MOD_ERROR;
    }
    wind_log("[sprint-wind] createScene hook installed");
    return MOD_OK;
}

void shutdown_sprint_wind() {
    s_capture = Capture{};
    s_loadFailed = false;
    s_sceneCreateCount = 0;
}
