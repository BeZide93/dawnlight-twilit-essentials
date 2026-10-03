#include "zora_swim.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_particle.h"
#include "d/d_particle_name.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2AudioMgr.h"

bool g_configModernZoraSwim = true;

DEFINE_HOOK(&daAlink_c::procSwimMove, ZoraSwimMoveHook);
DEFINE_HOOK(&daAlink_c::setSpeedAndAngleSwim, ZoraSwimSpeedAngleHook);
DEFINE_HOOK(&daAlink_c::setSwimMoveAnime, ZoraSwimMoveAnimeHook);
DEFINE_HOOK(&daAlink_c::setMatrix, ZoraSwimSetMatrixHook);

static constexpr u8 kFreeSwimMode = 4;
static constexpr u8 kSurfaceSwimMode = 0;
static constexpr s16 kFreeSwimHoldFrames = 30;

static constexpr f32 kCruiseSpeed = 18.0f;
static constexpr f32 kBurstSpeed = 28.0f;
static constexpr f32 kBurstKick = 24.0f;
static constexpr int kBurstFrames = 20;
static constexpr int kHoldRepeatFrames = 12;
static constexpr f32 kSurfaceSpeedMul = 1.2f;

static constexpr f32 kTurnBoost = 1.6f;
static constexpr f32 kPitchBoost = 1.8f;
static constexpr s16 kPitchLimit = 13653;

static constexpr f32 kBankPerYaw = 7.0f;
static constexpr f32 kBankLimit = 9000.0f;
static constexpr f32 kBankEase = 0.15f;
static constexpr int kBarrelFrames = 24;
static constexpr int kHoldRollMinBursts = 8;
static constexpr f32 kHoldRollExtraBursts = 5.0f;

static constexpr int kTrailEmitters = 2;
static constexpr f32 kTrailBackOffset = 50.0f;
static constexpr f32 kTrailSideOffset = 14.0f;
static constexpr f32 kTrailRate = 2.0f;
static constexpr f32 kTrailBelowSurface = 10.0f;

static int s_burstFrames = 0;
static bool s_yawActive = false;
static s16 s_yawBefore = 0;
static bool s_pitchActive = false;
static s16 s_pitchBefore = 0;
static bool s_burstTrigger = false;
static int s_holdFrames = 0;
static bool s_injectedTrigger = false;

static s16 s_yawRate = 0;
static bool s_freeSwimFrame = false;
static f32 s_bank = 0.0f;
static int s_barrelFrame = 0;
static int s_barrelDir = 1;
static int s_trailFrames = 0;
static u32 s_trailHandles[kTrailEmitters] = {};
static int s_holdBursts = 0;
static int s_nextHoldRoll = kHoldRollMinBursts;

static void roll_next_hold_target() {
    s_holdBursts = 0;
    s_nextHoldRoll = kHoldRollMinBursts + static_cast<int>(cM_rndF(kHoldRollExtraBursts));
}

static bool zora_swim_active(daAlink_c* link) {
    if (!g_configModernZoraSwim || link == nullptr) return false;
    if (!link->checkZoraWearAbility()) return false;
    if (link->checkBootsOrArmorHeavy()) return false;
    if (link->checkEventRun()) return false;
    return true;
}

static bool underwater_free_control(daAlink_c* link) {
    if (link->checkNoResetFlg0(daPy_py_c::FLG0_SWIM_UP)) return false;
    if (link->checkHookshotAnime()) return false;
    if (link->checkAttentionLock() || link->mTargetedActor != nullptr) return false;
    return true;
}

static void stop_trail() {
    s_trailFrames = 0;
    for (u32& handle : s_trailHandles) {
        if (handle == 0) continue;
        JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(handle);
        if (emitter != nullptr) emitter->becomeInvalidEmitter();
        handle = 0;
    }
}

static void tick_trail(daAlink_c* link) {
    if (s_trailFrames <= 0 || link->mpLinkModel == nullptr) {
        if (s_trailFrames <= 0) stop_trail();
        return;
    }
    s_trailFrames--;

    MtxP root = link->mpLinkModel->getAnmMtx(0);
    const s16 yaw = link->shape_angle.y;
    const s16 pitch = link->field_0x3080;
    const f32 cosPitch = cM_scos(pitch);
    const cXyz forward(cM_ssin(yaw) * cosPitch, -cM_ssin(pitch), cM_scos(yaw) * cosPitch);
    const cXyz side(cM_scos(yaw), 0.0f, -cM_ssin(yaw));

    for (int i = 0; i < kTrailEmitters; i++) {
        const f32 sideSign = i == 0 ? 1.0f : -1.0f;
        cXyz pos(root[0][3] - forward.x * kTrailBackOffset + side.x * kTrailSideOffset * sideSign,
                 root[1][3] - forward.y * kTrailBackOffset,
                 root[2][3] - forward.z * kTrailBackOffset + side.z * kTrailSideOffset * sideSign);
        if (pos.y > link->mWaterY - kTrailBelowSurface) continue;
        u32& handle = s_trailHandles[i];
        handle = dComIfGp_particle_set(handle, ID_ZI_J_LK_ABUKU_A, &pos, &link->tevStr,
                                       &link->shape_angle, nullptr, 0xFF, nullptr, -1,
                                       nullptr, nullptr, nullptr);
        JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(handle);
        if (emitter == nullptr) continue;
        emitter->setParticleCallBackPtr(dPa_control_c::getWaterBubblePcallBack());
        emitter->setRate(kTrailRate);
    }
}

static void start_burst_flair(daAlink_c* link) {
    if (s_barrelFrame == 0) {
        s_barrelFrame = 1;
        s_barrelDir = s_bank > 0.0f ? 1 : -1;
    }
    dComIfGp_getVibration().StartShock(VIBMODE_S_POWER2, 1, cXyz(0.0f, 1.0f, 0.0f));
    Z2GetAudioMgr()->seStart(Z2SE_AL_WATER_STROKE_FAST, &link->current.pos, 0, 0, 1.0f, 1.0f,
                             -1.0f, -1.0f, 0);
}

static HookAction swim_move_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!zora_swim_active(link) || !underwater_free_control(link)) return HOOK_CONTINUE;
    if (link->checkInputOnR() && link->field_0x3000 < kFreeSwimHoldFrames) {
        link->field_0x3000 = kFreeSwimHoldFrames;
    }
    return HOOK_CONTINUE;
}

static HookAction speed_angle_pre(ModContext*, void* args, void*, void*) {
    s_yawActive = false;
    s_yawRate = 0;
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!zora_swim_active(link) || !underwater_free_control(link)) return HOOK_CONTINUE;
    if (link->field_0x3000 == 0 || !link->checkInputOnR()) return HOOK_CONTINUE;
    s_yawActive = true;
    s_yawBefore = link->shape_angle.y;
    return HOOK_CONTINUE;
}

static void speed_angle_post(ModContext*, void* args, void*, void*) {
    if (!s_yawActive || !args) return;
    s_yawActive = false;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    const s16 delta = static_cast<s16>(link->shape_angle.y - s_yawBefore);
    s_yawRate = static_cast<s16>(delta * kTurnBoost);
    link->shape_angle.y = static_cast<s16>(s_yawBefore + s_yawRate);
    link->current.angle.y = link->shape_angle.y;
}

static HookAction move_anime_pre(ModContext*, void* args, void*, void*) {
    s_pitchActive = false;
    s_burstTrigger = false;
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!zora_swim_active(link)) return HOOK_CONTINUE;
    if (link->checkNoResetFlg0(daPy_py_c::FLG0_SWIM_UP)) {
        s_holdFrames = 0;
        return HOOK_CONTINUE;
    }
    if (link->doTrigger() || !link->doButton() || link->field_0x2f98 != kFreeSwimMode) {
        s_holdFrames = 0;
    } else if (++s_holdFrames >= kHoldRepeatFrames) {
        s_holdFrames = 0;
        link->mItemTrigger |= daAlink_c::BTN_A;
        s_injectedTrigger = true;
    }
    s_burstTrigger = link->doTrigger() && link->field_0x3000 != 0;
    if (link->mProcVar4.field_0x3010 == 0 && underwater_free_control(link)) {
        s_pitchActive = true;
        s_pitchBefore = link->field_0x3080;
    }
    return HOOK_CONTINUE;
}

static void move_anime_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const bool injected = s_injectedTrigger;
    if (s_injectedTrigger && link != nullptr) link->mItemTrigger &= ~daAlink_c::BTN_A;
    s_injectedTrigger = false;
    if (!zora_swim_active(link)) {
        s_burstFrames = 0;
        stop_trail();
        return;
    }

    if (link->checkNoResetFlg0(daPy_py_c::FLG0_SWIM_UP)) {
        s_burstFrames = 0;
        stop_trail();
        if (link->field_0x2f98 == kSurfaceSwimMode) {
            link->mMaxSpeed *= kSurfaceSpeedMul;
        }
        return;
    }

    if (link->field_0x2f98 != kFreeSwimMode) {
        s_burstFrames = 0;
        stop_trail();
        return;
    }

    s_freeSwimFrame = true;

    if (s_burstTrigger) {
        s_burstTrigger = false;
        s_burstFrames = kBurstFrames;
        s_trailFrames = kBurstFrames;
        if (link->mNormalSpeed < kBurstKick) link->mNormalSpeed = kBurstKick;
        if (!injected) {
            roll_next_hold_target();
            start_burst_flair(link);
        } else if (++s_holdBursts >= s_nextHoldRoll && s_barrelFrame == 0) {
            roll_next_hold_target();
            start_burst_flair(link);
        }
    }

    if (s_burstFrames > 0) {
        s_burstFrames--;
        link->mMaxSpeed = kBurstSpeed;
    } else {
        link->mMaxSpeed = kCruiseSpeed;
    }

    if (s_pitchActive && link->mProcVar4.field_0x3010 == 0) {
        const s16 delta = static_cast<s16>(link->field_0x3080 - s_pitchBefore);
        int pitch = s_pitchBefore + static_cast<int>(delta * kPitchBoost);
        if (pitch > kPitchLimit) pitch = kPitchLimit;
        if (pitch < -kPitchLimit) pitch = -kPitchLimit;
        link->field_0x3080 = static_cast<s16>(pitch);
    }
    s_pitchActive = false;

    tick_trail(link);
}

static void reset_roll() {
    s_bank = 0.0f;
    s_barrelFrame = 0;
    s_freeSwimFrame = false;
}

static void set_matrix_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    if (!g_configModernZoraSwim || link->checkWolf() || !link->checkZoraWearAbility() ||
        !link->checkModeFlg(daAlink_c::MODE_SWIMMING)) {
        reset_roll();
        s_burstFrames = 0;
        stop_trail();
        return;
    }

    const bool freeSwim = s_freeSwimFrame;
    s_freeSwimFrame = false;
    if (!freeSwim) {
        s_burstFrames = 0;
        stop_trail();
    }

    f32 target = 0.0f;
    if (freeSwim) {
        target = -static_cast<f32>(s_yawRate) * kBankPerYaw;
        if (target > kBankLimit) target = kBankLimit;
        if (target < -kBankLimit) target = -kBankLimit;
    }
    s_bank += (target - s_bank) * kBankEase;

    int roll = static_cast<int>(s_bank);
    if (s_barrelFrame > 0) {
        const f32 t = static_cast<f32>(s_barrelFrame) / static_cast<f32>(kBarrelFrames);
        const f32 eased = t * t * (3.0f - 2.0f * t);
        roll += static_cast<int>(static_cast<f32>(s_barrelDir) * 65536.0f * eased);
        if (++s_barrelFrame > kBarrelFrames) s_barrelFrame = 0;
    }

    const s16 rollAngle = static_cast<s16>(roll);
    if (rollAngle == 0 || link->mpLinkModel == nullptr || link->field_0x2060 == nullptr) return;

    const auto& pivot = link->field_0x2060->getOldFrameTransInfo(0)->mTranslate;
    const s16 bodyPitch = link->field_0x3080;
    mDoMtx_stack_c::copy(link->mpLinkModel->getBaseTRMtx());
    mDoMtx_stack_c::transM(pivot.x, pivot.y, pivot.z);
    mDoMtx_stack_c::XrotM(bodyPitch);
    mDoMtx_stack_c::ZrotM(rollAngle);
    mDoMtx_stack_c::XrotM(static_cast<s16>(-bodyPitch));
    mDoMtx_stack_c::transM(-pivot.x, -pivot.y, -pivot.z);
    link->mpLinkModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

float zora_swim_fov_kick() {
    return (g_configModernZoraSwim && s_burstFrames > 0) ? 1.0f : 0.0f;
}

ModResult init_zora_swim(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<ZoraSwimMoveHook>(hook_svc, swim_move_pre);
    mods::hook::add_pre<ZoraSwimSpeedAngleHook>(hook_svc, speed_angle_pre);
    mods::hook::add_post<ZoraSwimSpeedAngleHook>(hook_svc, speed_angle_post);
    mods::hook::add_pre<ZoraSwimMoveAnimeHook>(hook_svc, move_anime_pre);
    mods::hook::add_post<ZoraSwimMoveAnimeHook>(hook_svc, move_anime_post);
    mods::hook::add_post<ZoraSwimSetMatrixHook>(hook_svc, set_matrix_post);
    return MOD_OK;
}

void shutdown_zora_swim() {
    s_burstFrames = 0;
    s_yawActive = false;
    s_pitchActive = false;
    s_burstTrigger = false;
    s_holdFrames = 0;
    s_injectedTrigger = false;
    s_yawRate = 0;
    s_holdBursts = 0;
    s_nextHoldRoll = kHoldRollMinBursts;
    reset_roll();
    stop_trail();
}
