#include "stamina_swordcharge.hpp"
#include "stamina.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_particle_name.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "JSystem/JParticle/JPAEmitter.h"

bool g_configStaminaSpinChargeLevels = true;

DEFINE_HOOK(&daAlink_c::execute, StaminaSwordChargeExecute);

namespace {

constexpr int kMaxStage = 3;
constexpr int kStageFrames[kMaxStage + 1] = {0, 0, 75, 120};
constexpr f32 kRadiusMul[kMaxStage + 1] = {1.0f, 1.0f, 1.4f, 1.8f};
constexpr f32 kEffectScale[kMaxStage + 1] = {1.0f, 1.0f, 1.35f, 1.7f};
constexpr f32 kDrainMul[kMaxStage + 1] = {1.0f, 1.0f, 2.0f, 3.0f};
constexpr f32 kSpinReadyFrame = 14.0f;
constexpr f32 kTurnStartRadius = 130.0f;
constexpr f32 kRadiusReachFrames = 8.0f;
constexpr int kTurnEmitterCount = 6;
constexpr int kBladeEmitters = 3;
constexpr f32 kBladeStartT = 0.15f;
constexpr f32 kBladeEndT = 0.95f;
constexpr int kSparkleMaxEmitters = 2;

struct SparkleEmitter {
    u16 id;
    GXColor color;
    f32 scale;
};

constexpr SparkleEmitter kStage2Sparkle[] = {
    {ID_ZF_J_FAIRY02_STAR, {255, 255, 255, 255}, 1.0f},
};

constexpr SparkleEmitter kStage3Sparkle[] = {
    {ID_ZF_J_FAIRY02_STAR, {255, 205, 80, 255}, 1.1f},
};

int s_chargeFrames = 0;
int s_stage = 0;
bool s_wasCharging = false;
int s_releaseStage = 0;
bool s_releaseApplied = false;
int s_sparkleStage = 0;
u32 s_sparkleHandles[kSparkleMaxEmitters][kBladeEmitters] = {};
u32 s_sparkleRng = 0x2545F491u;

bool feature_enabled() {
    return g_configStaminaSpinChargeLevels;
}

bool is_turn_charge(const daAlink_c* link) {
    if (link->mProcID != daAlink_c::PROC_CUT_TURN_CHARGE && link->mProcID != daAlink_c::PROC_CUT_TURN_MOVE) return false;
    return link->mProcVar2.field_0x300c == 0;
}

bool spin_ready(const daAlink_c* link) {
    if (link->mProcID != daAlink_c::PROC_CUT_TURN_MOVE) return false;
    if (link->m_nSwordBtk != nullptr && link->m_nSwordBtk->getFrame() >= kSpinReadyFrame) return true;
    return link->mProcVar0.field_0x3008 >= kSpinReadyFrame;
}

void sparkle_list(int stage, const SparkleEmitter*& list, int& count) {
    if (stage >= 3) {
        list = kStage3Sparkle;
        count = static_cast<int>(sizeof(kStage3Sparkle) / sizeof(kStage3Sparkle[0]));
    } else {
        list = kStage2Sparkle;
        count = static_cast<int>(sizeof(kStage2Sparkle) / sizeof(kStage2Sparkle[0]));
    }
}

void clear_sparkle() {
    s_sparkleStage = 0;
    for (auto& row : s_sparkleHandles) {
        for (u32& h : row) h = 0;
    }
}

f32 next_blade_t() {
    s_sparkleRng = s_sparkleRng * 1664525u + 1013904223u;
    const f32 r = static_cast<f32>(s_sparkleRng >> 8) / 16777216.0f;
    return kBladeStartT + (kBladeEndT - kBladeStartT) * r;
}

void play_level_up_se(daAlink_c* link) {
    Z2GetAudioMgr()->seStart(Z2SE_SWORD_POWER_COME, &link->mSwordTopPos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void tick_sparkle(daAlink_c* link, int stage) {
    if (stage < 2 || link->mSwordModel == nullptr) {
        if (s_sparkleStage != 0) clear_sparkle();
        return;
    }
    if (stage != s_sparkleStage) {
        clear_sparkle();
        s_sparkleStage = stage;
    }
    const SparkleEmitter* list = nullptr;
    int count = 0;
    sparkle_list(stage, list, count);
    const cXyz& base = link->field_0x3498;
    const cXyz& tip = link->mSwordTopPos;
    for (int i = 0; i < count && i < kSparkleMaxEmitters; i++) {
        for (int p = 0; p < kBladeEmitters; p++) {
            const f32 t = next_blade_t();
            const cXyz pos(base.x + (tip.x - base.x) * t, base.y + (tip.y - base.y) * t, base.z + (tip.z - base.z) * t);
            u32& handle = s_sparkleHandles[i][p];
            handle = dComIfGp_particle_set(handle, list[i].id, &pos, &link->tevStr,
                                           nullptr, nullptr, 0xFF, nullptr, -1, nullptr, nullptr, nullptr);
            JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(handle);
            if (emitter == nullptr) continue;
            emitter->setGlobalPrmColor(list[i].color.r, list[i].color.g, list[i].color.b);
            emitter->setGlobalParticleScale(list[i].scale, list[i].scale);
        }
    }
}

void update_stage(daAlink_c* link) {
    s_chargeFrames++;
    int stage = s_stage;
    if (stage < 1 && spin_ready(link)) stage = 1;
    if (stage >= 1 && stage < kMaxStage && s_chargeFrames >= kStageFrames[stage + 1]) stage++;
    if (stage == s_stage) return;
    s_stage = stage;
    if (stage >= 2) play_level_up_se(link);
}

void apply_release(daAlink_c* link) {
    if (s_releaseApplied || s_releaseStage < 2) return;
    s_releaseApplied = true;
    link->field_0x3478 *= kRadiusMul[s_releaseStage];
    const f32 accel = (link->field_0x3478 - kTurnStartRadius) / kRadiusReachFrames;
    if (accel > link->field_0x348c) link->field_0x348c = accel;
}

void scale_turn_effects(daAlink_c* link) {
    if (s_releaseStage < 2) return;
    const f32 s = kEffectScale[s_releaseStage];
    const JGeometry::TVec3<f32> scale(s, s, s);
    for (int i = 0; i < kTurnEmitterCount; i++) {
        JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(link->field_0x3204[i]);
        if (emitter != nullptr) emitter->setGlobalScale(scale);
    }
}

void reset_state() {
    s_chargeFrames = 0;
    s_stage = 0;
    s_wasCharging = false;
    s_releaseStage = 0;
    s_releaseApplied = false;
    clear_sparkle();
}

void on_execute_post(ModContext*, void* args, void*, void*) {
    if (!feature_enabled()) {
        if (s_wasCharging || s_releaseStage != 0 || s_sparkleStage != 0) reset_state();
        return;
    }
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link != daPy_getLinkPlayerActorClass()) return;

    int sparkleStage = 0;
    if (is_turn_charge(link)) {
        s_wasCharging = true;
        update_stage(link);
        sparkleStage = s_stage;
    } else {
        if (s_wasCharging) {
            s_releaseStage = link->mProcID == daAlink_c::PROC_CUT_TURN ? s_stage : 0;
            s_releaseApplied = false;
            s_chargeFrames = 0;
            s_stage = 0;
            s_wasCharging = false;
        }
        if (link->mProcID != daAlink_c::PROC_CUT_TURN) s_releaseStage = 0;
    }

    if (link->mProcID == daAlink_c::PROC_CUT_TURN) {
        apply_release(link);
        scale_turn_effects(link);
        sparkleStage = s_releaseStage;
    }
    tick_sparkle(link, sparkleStage);
}

}

void init_stamina_swordcharge(const HookService* hook_svc) {
    if (hook_svc == nullptr) return;
    mods::hook::add_post<StaminaSwordChargeExecute>(hook_svc, on_execute_post);
}

float stamina_swordcharge_drain_mul() {
    if (!feature_enabled() || !s_wasCharging) return 1.0f;
    return kDrainMul[s_stage];
}

void shutdown_stamina_swordcharge() {
    reset_state();
}
