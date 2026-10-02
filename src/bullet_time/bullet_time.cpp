#include "bullet_time.hpp"
#include "../flurry_rush/flurry_rush.hpp"
#include "../general/fast_forward_cutscenes.hpp"
#include "../stamina/stamina.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_arrow.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_drawlist.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAResource.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "SSystem/SComponent/c_math.h"
#include "mods/svc/hook.hpp"
#include "dusk/config_var.hpp"
#include "dusk/settings.h"
#include "dusk/game_clock.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string_view>

bool g_configBulletTimeEnabled = false;
bool g_configBulletTimeFirstPerson = true;

namespace {

constexpr f32 kTimeScale = 0.2f;
constexpr f32 kNoSlowScale = 0.99f;
constexpr f32 kMinLiveScale = 0.05f;
constexpr f32 kDrainPerStep = 0.35f;
constexpr int kMinCostPct = 5;
constexpr u8 kSlingArrowType = 4;
constexpr u32 kBowAimStatus = 0x1000;
constexpr u32 kHawkEyeStatus = 0x200000;
constexpr u32 kSubjectCameraStatus = 0x10;
constexpr f32 kStickTurnRate = 512.0f;
constexpr f32 kStickDeadValue = 0.05f;
constexpr f32 kMaxSampleGap = 0.25f;
constexpr f32 kMaxElevation = 1.55f;
constexpr f32 kAngleToRad = 3.14159265f / 32768.0f;
constexpr f32 kRadToAngle = 32768.0f / 3.14159265f;
constexpr f32 kStatsLogInterval = 1.0f;
constexpr f32 kGyroEmaAlphaMin = 0.05f;
constexpr f32 kGyroEmaAlphaMax = 1.0f;
constexpr f32 kGravityEmaAlpha = 0.1f;
constexpr f32 kMinGravityProjection = 0.2f;
constexpr f32 kRollAimBoostMax = 2.0f;

using LiveClock = std::chrono::steady_clock;

const HookService* s_hookSvc = nullptr;
const LogService* s_log = nullptr;
bool s_hooksInstalled = false;

bool s_active = false;
bool s_needLanding = false;
bool s_reaimBlock = false;
f32 s_linkScale = 1.0f;
f32 s_stepAccumulator = 0.0f;
bool s_aimStatusSet = false;
bool s_inExtraStep = false;
bool s_prevBowReady = false;
f32 s_cameraAccumulator = 0.0f;
bool s_inCameraExtra = false;
bool s_presentationHooked = false;
bool s_inSimTick = false;
bool s_presentationSeen = false;
bool s_subjectCameraOn = false;
bool s_liveAimWarned = false;
int s_activeTicks = 0;

using GetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
void* s_frameInterpVar = nullptr;
bool s_interpOverridden = false;

using GyroKeepAliveGetFn = bool (*)();
using GyroKeepAliveSetFn = void (*)(bool);
using GyroAimDeltasFn = void (*)(float&, float&);
using PadReadFn = u32 (*)(PADStatus*);
using PadClampFn = void (*)(PADStatus*);
using PadSensorFn = BOOL (*)(u32, PADSensorType, f32*, int);
GyroKeepAliveGetFn s_gyroKeepAliveGet = nullptr;
GyroKeepAliveSetFn s_gyroKeepAliveSet = nullptr;
GyroAimDeltasFn s_gyroAimDeltas = nullptr;
struct SettingVars {
    void* gyroAim = nullptr;
    void* gyroSensX = nullptr;
    void* gyroSensY = nullptr;
    void* gyroSmoothing = nullptr;
    void* gyroDeadband = nullptr;
    void* gyroInvertPitch = nullptr;
    void* gyroInvertYaw = nullptr;
    void* mirror = nullptr;
    void* invertX = nullptr;
    void* invertY = nullptr;
};
SettingVars s_settingVars;
PadReadFn s_padRead = nullptr;
PadClampFn s_padClamp = nullptr;
PadClampFn s_padClampCircle = nullptr;
PadSensorFn s_padSensor = nullptr;
bool s_ownsGyroKeepAlive = false;
bool s_ownGyroRead = false;

LiveClock::time_point s_lastSample{};
struct GyroFilter {
    f32 smoothX = 0.0f;
    f32 smoothY = 0.0f;
    f32 smoothZ = 0.0f;
    f32 gravityY = 0.0f;
    f32 gravityZ = 0.0f;
    f32 baselineY = 0.0f;
    f32 baselineZ = 0.0f;
    bool haveBaseline = false;
    bool wasAiming = false;
};
GyroFilter s_gyroFilter;
f32 s_pendingYaw = 0.0f;
f32 s_pendingPitch = 0.0f;
cXyz s_tickViewDir(0.0f, 0.0f, 0.0f);
bool s_tickViewDirValid = false;
s16 s_cameraYaw = 0;
s16 s_cameraPitch = 0;
bool s_appliedThisTick = false;
fpc_ProcID s_nockedArrowId = fpcM_ERROR_PROCESS_ID_e;
fpc_ProcID s_waitArrowId = fpcM_ERROR_PROCESS_ID_e;

constexpr int kMaxLinkModels = 24;
constexpr s16 kBombArrowHoldSteps = 8;
constexpr f32 kReloadAnimeSpeedUp = 2.0f;
J3DModel* s_linkModels[kMaxLinkModels] = {};
const char* s_linkModelNames[kMaxLinkModels] = {};
int s_linkModelSwaps[kMaxLinkModels] = {};
int s_linkModelCount = 0;
constexpr int kMaxLinkEmitters = 12;
JPABaseEmitter* s_linkEmitters[kMaxLinkEmitters] = {};
int s_linkEmitterCount = 0;
JPAEmitterWorkData* s_emitterSwapWork = nullptr;
Mtx s_savedEmitterPosCam;
Mtx s_savedEmitterYBBCam;
bool s_linkModelsLogged = false;
Mtx s_baseViewMtx;
Mtx s_liveViewMtx;
bool s_viewSplit = false;
J3DShapePacket* s_basePtrPacket = nullptr;
Mtx* s_savedBasePtr = nullptr;
Mtx s_viewCorrection;
Mtx s_correctedBaseMtx;
bool s_inShadowPass = false;
J3DModel* s_seenLinkModels[kMaxLinkModels] = {};
int s_seenLinkModelCount = 0;

enum class ViewSwap {
    None,
    Packet,
    Shape,
};
ViewSwap s_viewSwap = ViewSwap::None;

struct LiveStats {
    int ticks = 0;
    int linkSteps = 0;
    int cameraRuns = 0;
    int frames = 0;
    int previews = 0;
    int gyroReads = 0;
    int appliedYaw = 0;
    int appliedPitch = 0;
    int cameraFirst = 0;
    f32 camYawError = 0.0f;
    f32 camPitchError = 0.0f;
    f32 gyroYawTurn = 0.0f;
    f32 gyroPitchTurn = 0.0f;
    int arrowLaunches = 0;
    int arrowBoosts = 0;
    int linkPacketSwaps = 0;
    int linkShapeSwaps = 0;
    int packetDraws = 0;
    int basePtrSwaps = 0;
    int emitterSwaps = 0;
    int shapeDraws = 0;
    f32 stickTurn = 0.0f;
    f32 maxPending = 0.0f;
};
LiveStats s_stats;

struct NormalGyroStats {
    int calls = 0;
    f32 yawTurn = 0.0f;
    f32 pitchTurn = 0.0f;
};
NormalGyroStats s_normalGyro;
LiveClock::time_point s_lastStatsLog{};

struct LinkMoveScale {
    bool active = false;
    bool ownGravity = false;
    f32 factor = 1.0f;
    f32 normalSpeed = 0.0f;
    f32 gravity = 0.0f;
    f32 maxFallSpeed = 0.0f;
};
LinkMoveScale s_move;

struct ArrowMoveScale {
    daArrow_c* arrow = nullptr;
    f32 boost = 1.0f;
    f32 outLengthRate = 0.0f;
};
ArrowMoveScale s_arrowMove;

void bt_log(const char* fmt, ...) {
    if (s_log == nullptr || s_log->info == nullptr) return;
    char msg[384];
    const int prefix = std::snprintf(msg, sizeof(msg), "[bullet-time] ");
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg + prefix, sizeof(msg) - prefix, fmt, args);
    va_end(args);
    s_log->info(mod_ctx, msg);
}

const char* found(const void* ptr) {
    return ptr != nullptr ? "ok" : "MISSING";
}

void override_frame_interp(bool enable) {
    if (s_frameInterpVar == nullptr) return;
    auto* var = static_cast<dusk::config::ConfigVar<dusk::FrameInterpMode>*>(s_frameInterpVar);
    if (enable) {
        var->setOverrideValue(dusk::FrameInterpMode::Unlimited);
        s_interpOverridden = true;
    } else if (s_interpOverridden) {
        var->clearOverride();
        s_interpOverridden = false;
    }
}

daAlink_c* player_link() {
    return static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
}

bool is_air_proc(u16 proc) {
    return proc == daAlink_c::PROC_SIDESTEP || proc == daAlink_c::PROC_AUTO_JUMP ||
           proc == daAlink_c::PROC_FALL;
}

bool is_bow_item(u16 item) {
    return item == dItemNo_BOW_e || item == dItemNo_BOMB_ARROW_e || item == dItemNo_HAWK_ARROW_e;
}

bool bow_aiming(daAlink_c* link) {
    return link->checkBowReloadAnime() || link->checkBowChargeWaitAnime() ||
           link->checkBowWaitAnime() || link->checkBowShootAnime();
}

bool airborne(daAlink_c* link) {
    return is_air_proc(link->mProcID) && !link->mLinkAcch.ChkGroundHit() &&
           !link->checkModeFlg(daAlink_c::MODE_SWIMMING);
}

bool air_bow_state(daAlink_c* link) {
    return link != nullptr && !link->checkWolf() && airborne(link) &&
           is_bow_item(link->mEquipItem) && bow_aiming(link);
}

bool stamina_blocks() {
    return g_configStaminaSrcBulletTime && stamina_is_exhausted();
}

f32 drain_per_step() {
    const int pct = g_configStaminaCostBulletTime < kMinCostPct ? kMinCostPct : g_configStaminaCostBulletTime;
    return kDrainPerStep * static_cast<f32>(pct) / 100.0f;
}

bool can_continue(daAlink_c* link) {
    return g_configBulletTimeEnabled && link != nullptr && !link->checkEventRun() &&
           dComIfGp_isPauseFlag() == 0 && !flurry_rush_is_rush_active() &&
           !stamina_blocks() && air_bow_state(link);
}

const char* stop_reason(daAlink_c* link) {
    if (!g_configBulletTimeEnabled) return "disabled";
    if (link == nullptr) return "no player";
    if (link->checkEventRun()) return "event";
    if (dComIfGp_isPauseFlag() != 0) return "pause";
    if (flurry_rush_is_rush_active()) return "flurry rush";
    if (stamina_blocks()) return "stamina empty";
    if (link->checkWolf()) return "wolf";
    if (!is_air_proc(link->mProcID)) return "left air proc";
    if (link->mLinkAcch.ChkGroundHit()) return "landed";
    if (link->checkModeFlg(daAlink_c::MODE_SWIMMING)) return "swimming";
    if (!is_bow_item(link->mEquipItem)) return "bow not equipped";
    if (!bow_aiming(link)) return "stopped aiming";
    return "unknown";
}

bool can_start(daAlink_c* link) {
    return !s_needLanding && !s_reaimBlock && general_timescale_available() && can_continue(link);
}

f32 live_scale() {
    const f32 live = general_get_aurora_timescale();
    return live > kMinLiveScale && live < kNoSlowScale ? live : 1.0f;
}

bool scaling_active() {
    return s_active && s_linkScale < kNoSlowScale && !flurry_rush_is_rush_active();
}

bool first_person_wanted(daAlink_c* link) {
    return s_active && g_configBulletTimeFirstPerson && link != nullptr && link == player_link() &&
           !flurry_rush_is_rush_active() && air_bow_state(link);
}

struct AimSettings {
    bool mirror = false;
    bool invertX = false;
    bool invertY = false;
};

bool bool_setting(void* var) {
    return var != nullptr && static_cast<dusk::config::ConfigVar<bool>*>(var)->getValue();
}

AimSettings aim_settings() {
    AimSettings settings;
    settings.mirror = bool_setting(s_settingVars.mirror);
    settings.invertX = bool_setting(s_settingVars.invertX);
    settings.invertY = bool_setting(s_settingVars.invertY);
    return settings;
}

bool gyro_aim_enabled() {
    return bool_setting(s_settingVars.gyroAim);
}

bool live_aim_active() {
    return s_presentationHooked && scaling_active() && first_person_wanted(player_link());
}

bool live_stick_ready() {
    return s_padRead != nullptr;
}

bool live_gyro_ready() {
    return s_padSensor != nullptr && gyro_aim_enabled();
}

void sync_gyro_keep_alive() {
    if (s_gyroKeepAliveGet == nullptr || s_gyroKeepAliveSet == nullptr) return;
    const bool wanted = s_active && gyro_aim_enabled();
    if (wanted && !s_gyroKeepAliveGet()) {
        s_gyroKeepAliveSet(true);
        s_ownsGyroKeepAlive = true;
        bt_log("gyro keep-alive on");
    } else if (!wanted && s_ownsGyroKeepAlive) {
        s_gyroKeepAliveSet(false);
        s_ownsGyroKeepAlive = false;
        bt_log("gyro keep-alive off");
    }
}

void clear_live_aim() {
    s_lastSample = {};
    s_gyroFilter = {};
    s_pendingYaw = 0.0f;
    s_pendingPitch = 0.0f;
    s_tickViewDirValid = false;
}

f32 aim_zoom_scale(daAlink_c* link) {
    if (!dComIfGp_checkPlayerStatus0(0, kHawkEyeStatus)) return 1.0f;
    const f32 zoom = dComIfGp_getCameraZoomScale(link->field_0x317c);
    return zoom > 0.0f ? 1.0f / zoom : 1.0f;
}

f32 main_stick_clamp() {
    switch (JUTGamePad::getClampMode()) {
    case JUTGamePad::EClampStick:
        return 54.0f;
    case JUTGamePad::EClampCircle:
        return 38.0f;
    default:
        return 69.0f;
    }
}

bool read_main_stick(f32& posX, f32& posY) {
    PADStatus status[4] = {};
    s_padRead(status);
    const int mode = JUTGamePad::getClampMode();
    if (mode == JUTGamePad::EClampStick && s_padClamp != nullptr) {
        s_padClamp(status);
    } else if (mode == JUTGamePad::EClampCircle && s_padClampCircle != nullptr) {
        s_padClampCircle(status);
    }
    if (status[0].err != 0) return false;
    const f32 clamp = main_stick_clamp();
    posX = static_cast<f32>(status[0].stickX) / clamp;
    posY = static_cast<f32>(status[0].stickY) / clamp;
    return true;
}

void sample_stick(f32 dt, f32 zoom) {
    f32 posX = 0.0f;
    f32 posY = 0.0f;
    if (!read_main_stick(posX, posY)) return;
    const f32 length = std::sqrt(posX * posX + posY * posY);
    const f32 value = length > 1.0f ? 1.0f : length;
    if (value <= kStickDeadValue) return;

    const f32 amount = kStickTurnRate * value * value * zoom * dt / dusk::game_clock::kSimPeriod;
    const AimSettings settings = aim_settings();
    f32 yawDir = -(posX / length);
    f32 pitchDir = posY / length;
    if (settings.mirror) yawDir = -yawDir;
    if (settings.invertX) yawDir = -yawDir;
    if (settings.invertY) pitchDir = -pitchDir;
    s_pendingYaw += amount * yawDir;
    s_pendingPitch += amount * pitchDir;
    s_stats.stickTurn += amount;
}

f32 float_setting(void* var, f32 fallback) {
    return var != nullptr ? static_cast<dusk::config::ConfigVar<float>*>(var)->getValue() : fallback;
}

f32 time_alpha(f32 tickAlpha, f32 dt) {
    if (tickAlpha >= 1.0f) return 1.0f;
    if (tickAlpha <= 0.0f) return 0.0f;
    return 1.0f - std::pow(1.0f - tickAlpha, dt / dusk::game_clock::kSimPeriod);
}

f32 apply_deadband(f32 value, f32 deadband) {
    return value > -deadband && value < deadband ? 0.0f : value;
}

f32 gravity_horizontal_rate(f32 dt, f32 yawRate, f32 rollRate) {
    GyroFilter& f = s_gyroFilter;
    f32 accel[3];
    if (!s_padSensor(PAD_CHAN0, PAD_SENSOR_ACCEL, accel, 3)) return yawRate;
    if (!f.haveBaseline) {
        f.gravityY = accel[1];
        f.gravityZ = accel[2];
    } else {
        const f32 alpha = time_alpha(kGravityEmaAlpha, dt);
        f.gravityY += alpha * (accel[1] - f.gravityY);
        f.gravityZ += alpha * (accel[2] - f.gravityZ);
    }
    const f32 length = std::sqrt(f.gravityY * f.gravityY + f.gravityZ * f.gravityZ);
    if (length < kMinGravityProjection) return yawRate;
    const f32 currentY = f.gravityY / length;
    const f32 currentZ = f.gravityZ / length;
    if (!f.haveBaseline) {
        f.baselineY = currentY;
        f.baselineZ = currentZ;
        f.haveBaseline = true;
    }
    const f32 yawWeight = f.baselineY * currentY + f.baselineZ * currentZ;
    const f32 rollWeight = f.baselineY * currentZ - f.baselineZ * currentY;
    const f32 rollBoost = 1.0f + std::fabs(rollWeight) * (kRollAimBoostMax - 1.0f);
    return yawRate * yawWeight + rollRate * rollWeight * rollBoost;
}

void sample_gyro(f32 dt, f32 zoom, bool aimActive) {
    GyroFilter& f = s_gyroFilter;
    f32 gyro[3];
    if (!s_padSensor(PAD_CHAN0, PAD_SENSOR_GYRO, gyro, 3)) return;

    const f32 smoothing = float_setting(s_settingVars.gyroSmoothing, 0.65f);
    const f32 alpha = time_alpha(kGyroEmaAlphaMax + smoothing * (kGyroEmaAlphaMin - kGyroEmaAlphaMax), dt);
    f.smoothX += alpha * (gyro[0] - f.smoothX);
    f.smoothY += alpha * (gyro[1] - f.smoothY);
    f.smoothZ += alpha * (gyro[2] - f.smoothZ);

    const f32 deadband = float_setting(s_settingVars.gyroDeadband, 0.04f);
    const f32 pitchRate = apply_deadband(f.smoothX, deadband);
    const f32 yawRate = apply_deadband(f.smoothY, deadband);
    const f32 rollRate = apply_deadband(f.smoothZ, deadband);

    if (aimActive != f.wasAiming) {
        f.gravityY = 0.0f;
        f.gravityZ = 0.0f;
        f.baselineY = 0.0f;
        f.baselineZ = 0.0f;
        f.haveBaseline = false;
        f.wasAiming = aimActive;
    }
    const f32 horizontalRate = aimActive ? gravity_horizontal_rate(dt, yawRate, rollRate) : yawRate;

    f32 pitch = -pitchRate * dt * float_setting(s_settingVars.gyroSensY, 1.0f);
    f32 yaw = horizontalRate * dt * float_setting(s_settingVars.gyroSensX, 1.0f);
    if (bool_setting(s_settingVars.gyroInvertPitch)) pitch = -pitch;
    if (bool_setting(s_settingVars.gyroInvertYaw)) yaw = -yaw;
    if (bool_setting(s_settingVars.mirror)) yaw = -yaw;

    s_pendingYaw += yaw * kRadToAngle * zoom;
    s_pendingPitch += pitch * kRadToAngle * zoom;
    s_stats.gyroYawTurn += std::fabs(yaw * kRadToAngle * zoom);
    s_stats.gyroPitchTurn += std::fabs(pitch * kRadToAngle * zoom);
    ++s_stats.gyroReads;
}

void sample_live_aim() {
    if (!live_aim_active()) {
        clear_live_aim();
        return;
    }
    const LiveClock::time_point now = LiveClock::now();
    f32 dt = 0.0f;
    if (s_lastSample != LiveClock::time_point{}) {
        dt = std::chrono::duration<f32>(now - s_lastSample).count();
        if (dt > kMaxSampleGap) dt = kMaxSampleGap;
    }
    s_lastSample = now;
    if (dt <= 0.0f) return;

    ++s_stats.frames;
    const f32 zoom = aim_zoom_scale(player_link());
    if (live_stick_ready()) sample_stick(dt, zoom);
    if (live_gyro_ready()) {
        daAlink_c* link = player_link();
        const bool aimActive = link != nullptr &&
                               dComIfGp_checkCameraAttentionStatus(link->field_0x317c, kSubjectCameraStatus);
        sample_gyro(dt, zoom, aimActive);
    }
}

int clamp_body_x(daAlink_c* link, int bodyX) {
    const int upLimit = link->mpHIO->mItem.m.mItemFPUpMaxUnk;
    const int downLimit = link->mpHIO->mItem.m.mItemFPMaxUnk;
    if (bodyX < upLimit) return upLimit;
    if (bodyX > downLimit) return downLimit;
    return bodyX;
}

bool preview_live_aim(view_class& view) {
    daAlink_c* link = player_link();
    if (link == nullptr || !s_tickViewDirValid || !live_aim_active()) return false;
    if (!dComIfGp_checkCameraAttentionStatus(link->field_0x317c, kSubjectCameraStatus)) return false;

    const f32 length = s_tickViewDir.abs();
    if (length < 1.0f) return false;
    const int pitchAngle = clamp_body_x(link, link->field_0x310a + static_cast<int>(s_pendingPitch));
    const f32 yaw = (static_cast<f32>(link->field_0x310c) + s_pendingYaw) * kAngleToRad;
    f32 elevation = -static_cast<f32>(pitchAngle) * kAngleToRad;
    if (elevation > kMaxElevation) elevation = kMaxElevation;
    if (elevation < -kMaxElevation) elevation = -kMaxElevation;

    const f32 horizontal = std::cos(elevation) * length;
    view.lookat.center = cXyz(view.lookat.eye.x + horizontal * std::sin(yaw),
                              view.lookat.eye.y + std::sin(elevation) * length,
                              view.lookat.eye.z + horizontal * std::cos(yaw));
    ++s_stats.previews;
    return true;
}

void add_link_model(J3DModel* model, const char* name) {
    if (model == nullptr || s_linkModelCount >= kMaxLinkModels) return;
    s_linkModelNames[s_linkModelCount] = name;
    s_linkModels[s_linkModelCount++] = model;
}

void note_link_model_seen(J3DModel* model) {
    for (int i = 0; i < s_seenLinkModelCount; i++) {
        if (s_seenLinkModels[i] == model) return;
    }
    if (s_seenLinkModelCount < kMaxLinkModels) s_seenLinkModels[s_seenLinkModelCount++] = model;
}

bool link_model_seen(J3DModel* model) {
    for (int i = 0; i < s_seenLinkModelCount; i++) {
        if (s_seenLinkModels[i] == model) return true;
    }
    return false;
}

void add_link_emitter(u32 id) {
    if (id == 0 || s_linkEmitterCount >= kMaxLinkEmitters) return;
    JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(id);
    if (emitter != nullptr) s_linkEmitters[s_linkEmitterCount++] = emitter;
}

void collect_arrow_emitters(daArrow_c* arrow) {
    add_link_emitter(arrow->field_0x964);
    add_link_emitter(arrow->field_0x968);
    for (u32 id : arrow->field_0x96c) add_link_emitter(id);
    for (u32 id : arrow->field_0x97c) add_link_emitter(id);
}

bool is_link_emitter(JPABaseEmitter* emitter) {
    if (emitter == nullptr) return false;
    for (int i = 0; i < s_linkEmitterCount; i++) {
        if (s_linkEmitters[i] == emitter) return true;
    }
    return false;
}

void collect_link_models(daAlink_c* link) {
    s_linkModelCount = 0;
    s_linkEmitterCount = 0;
    add_link_model(link->mpLinkModel, "body");
    add_link_model(link->mpLinkFaceModel, "face");
    add_link_model(link->mpLinkHatModel, "hat");
    add_link_model(link->mpLinkHandModel, "hand");
    add_link_model(link->mpLinkBootModels[0], "bootL");
    add_link_model(link->mpLinkBootModels[1], "bootR");
    add_link_model(link->mHeldItemModel, "heldItem");
    add_link_model(link->mpHookTipModel, "hookTip");
    add_link_model(link->field_0x0710, "item0710");
    add_link_model(link->field_0x0714, "item0714");
    add_link_model(link->mSwordModel, "sword");
    add_link_model(link->mSheathModel, "sheath");
    add_link_model(link->mShieldModel, "shield");
    add_link_model(link->mpSwAModel, "swA");
    add_link_model(link->mpSwASheathModel, "swASheath");
    add_link_model(link->mpSwMModel, "swM");
    add_link_model(link->mpSwMSheathModel, "swMSheath");
    add_link_model(link->mWoodSwordModel, "woodSword");
    add_link_model(link->mpKanteraModel, "lantern");
    add_link_model(link->mpKanteraGlowModel, "lanternGlow");
    fopAc_ac_c* held = link->mItemAcKeep.getActor();
    if (held != nullptr && fopAcM_GetName(held) == fpcNm_ARROW_e) {
        add_link_model(static_cast<daArrow_c*>(held)->mpModel, "arrow");
        collect_arrow_emitters(static_cast<daArrow_c*>(held));
    }
    if (s_linkModelsLogged) return;
    s_linkModelsLogged = true;
    for (int i = 0; i < s_linkModelCount; i++) {
        J3DModel* model = s_linkModels[i];
        J3DModelData* data = model->getModelData();
        bt_log("link model %s=%p calcMode=%u dataFlag10=%d cpuSkin=%d", s_linkModelNames[i],
               static_cast<void*>(model), static_cast<unsigned>(model->getMtxCalcMode()),
               data != nullptr && data->checkFlag(0x10) ? 1 : 0, model->isCpuSkinningOn() ? 1 : 0);
    }
}

int link_model_index(J3DModel* model) {
    if (model == nullptr) return -1;
    for (int i = 0; i < s_linkModelCount; i++) {
        if (s_linkModels[i] == model) return i;
    }
    return -1;
}

void log_link_model_swaps() {
    char line[320];
    int used = std::snprintf(line, sizeof(line), "link model swaps:");
    for (int i = 0; i < s_linkModelCount && used > 0 && used < static_cast<int>(sizeof(line)); i++) {
        used += std::snprintf(line + used, sizeof(line) - used, " %s=%d", s_linkModelNames[i], s_linkModelSwaps[i]);
        s_linkModelSwaps[i] = 0;
    }
    bt_log("%s", line);
}

void refresh_view_matrices(view_class& view) {
    mDoMtx_lookAt(view.viewMtx, &view.lookat.eye, &view.lookat.center, &view.lookat.up, view.bank);
    j3dSys.setViewMtx(view.viewMtx);
    cMtx_inverse(view.viewMtx, view.invViewMtx);
    MTXCopy(view.viewMtx, view.viewMtxNoTrans);
    view.viewMtxNoTrans[0][3] = 0.0f;
    view.viewMtxNoTrans[1][3] = 0.0f;
    view.viewMtxNoTrans[2][3] = 0.0f;
    cMtx_concatProjView(view.projMtx, view.viewMtx, view.projViewMtx);
}

void turn_link_aim(daAlink_c* link, int yaw, int pitch) {
    if (yaw == 0 && pitch == 0) return;
    link->shape_angle.y = static_cast<s16>(link->shape_angle.y + yaw);
    const int bodyX = clamp_body_x(link, link->mBodyAngle.x + pitch);
    link->mBodyAngle.x = link->checkBodyAngleX(static_cast<s16>(bodyX));
    link->field_0x310a = link->mBodyAngle.x;
    link->field_0x310c = link->shape_angle.y;
}

void apply_live_aim(daAlink_c* link) {
    if (live_aim_active()) {
        const f32 pending = std::fabs(s_pendingYaw) > std::fabs(s_pendingPitch) ? std::fabs(s_pendingYaw)
                                                                                  : std::fabs(s_pendingPitch);
        if (pending > s_stats.maxPending) s_stats.maxPending = pending;
        const int yawStep = static_cast<int>(s_pendingYaw);
        const int pitchStep = static_cast<int>(s_pendingPitch);
        s_pendingYaw -= static_cast<f32>(yawStep);
        s_pendingPitch -= static_cast<f32>(pitchStep);
        s_stats.appliedYaw += yawStep < 0 ? -yawStep : yawStep;
        s_stats.appliedPitch += pitchStep < 0 ? -pitchStep : pitchStep;
        s_appliedThisTick = true;
        turn_link_aim(link, yawStep, pitchStep);
        return;
    }

    if (s_gyroAimDeltas == nullptr || !gyro_aim_enabled()) return;
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
    s_ownGyroRead = true;
    s_gyroAimDeltas(yaw, pitch);
    s_ownGyroRead = false;
    const f32 scale = kRadToAngle * aim_zoom_scale(link) / s_linkScale;
    turn_link_aim(link, static_cast<int>(yaw * scale), static_cast<int>(pitch * scale));
}

void release_aim_status() {
    if (!s_aimStatusSet) return;
    s_aimStatusSet = false;
    daAlink_c* link = player_link();
    if (link != nullptr && (link->mProcID == daAlink_c::PROC_BOW_SUBJECT ||
                            link->mProcID == daAlink_c::PROC_BOW_MOVE ||
                            link->checkModeFlg(daAlink_c::MODE_RIDING)))
    {
        return;
    }
    dComIfGp_clearPlayerStatus0(0, kBowAimStatus);
}

void reset_stats() {
    s_stats = {};
    s_lastStatsLog = LiveClock::now();
}

void log_stats() {
    const LiveClock::time_point now = LiveClock::now();
    if (std::chrono::duration<f32>(now - s_lastStatsLog).count() < kStatsLogInterval) return;
    s_lastStatsLog = now;
    bt_log("stats: ticks=%d linkSteps=%d camRuns=%d frames=%d previews=%d stickTurn=%.0f gyroReads=%d "
           "appliedYaw=%d appliedPitch=%d maxPending=%.0f camFirst=%d camErr=%.1f/%.1f gyroTurn=%.0f/%.0f arrowLaunches=%d arrowBoosts=%d linkPacketSwaps=%d linkShapeSwaps=%d packetDraws=%d shapeDraws=%d basePtrSwaps=%d emitterSwaps=%d scale=%.2f live=%d subjectCam=%d",
           s_stats.ticks, s_stats.linkSteps, s_stats.cameraRuns, s_stats.frames, s_stats.previews,
           s_stats.stickTurn, s_stats.gyroReads, s_stats.appliedYaw, s_stats.appliedPitch,
           s_stats.maxPending, s_stats.cameraFirst, s_stats.camYawError, s_stats.camPitchError,
           s_stats.gyroYawTurn, s_stats.gyroPitchTurn, s_stats.arrowLaunches, s_stats.arrowBoosts, s_stats.linkPacketSwaps, s_stats.linkShapeSwaps, s_stats.packetDraws, s_stats.shapeDraws, s_stats.basePtrSwaps, s_stats.emitterSwaps, s_linkScale, live_aim_active() ? 1 : 0, s_subjectCameraOn ? 1 : 0);
    log_link_model_swaps();
    s_stats = {};
}

void start_bullet_time(daAlink_c* link) {
    s_active = true;
    s_activeTicks = 0;
    s_stepAccumulator = 0.0f;
    s_cameraAccumulator = 0.0f;
    s_subjectCameraOn = false;
    s_liveAimWarned = false;
    s_presentationSeen = false;
    s_linkModelsLogged = false;
    s_seenLinkModelCount = 0;
    clear_live_aim();
    reset_stats();
    general_set_slow_motion(kTimeScale);
    override_frame_interp(true);
    s_linkScale = live_scale();
    sync_gyro_keep_alive();
    bt_log("start: proc=%u item=%u scale=%.2f fp=%d staminaSrc=%d gyro=%d gyroSmoothing=%.2f gyroSens=%.2f/%.2f "
           "gyroDeadband=%.3f",
           static_cast<unsigned>(link->mProcID), static_cast<unsigned>(link->mEquipItem), s_linkScale,
           g_configBulletTimeFirstPerson ? 1 : 0, g_configStaminaSrcBulletTime ? 1 : 0,
           gyro_aim_enabled() ? 1 : 0, float_setting(s_settingVars.gyroSmoothing, -1.0f),
           float_setting(s_settingVars.gyroSensX, -1.0f), float_setting(s_settingVars.gyroSensY, -1.0f),
           float_setting(s_settingVars.gyroDeadband, -1.0f));
}

void stop_bullet_time(const char* reason) {
    if (!s_active) return;
    daAlink_c* link = player_link();
    bt_log("stop: %s (proc=%u ticks=%d)", reason,
           link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u, s_activeTicks);
    s_active = false;
    release_aim_status();
    sync_gyro_keep_alive();
    clear_live_aim();
    s_subjectCameraOn = false;
    s_linkScale = 1.0f;
    s_stepAccumulator = 0.0f;
    s_cameraAccumulator = 0.0f;
    if (flurry_rush_is_rush_active()) {
        s_interpOverridden = false;
        return;
    }
    general_set_slow_motion(1.0f);
    override_frame_interp(false);
}

void stop_if_needed(daAlink_c* link) {
    if (!s_active || can_continue(link)) return;
    if (stamina_blocks()) s_needLanding = true;
    stop_bullet_time(stop_reason(link));
}

DEFINE_HOOK(&daAlink_c::execute, BulletTimeExecuteHook);
DEFINE_HOOK(&daAlink_c::posMove, BulletTimePosMoveHook);
DEFINE_HOOK(&daArrow_c::procMove, BulletTimeArrowMoveHook);
DEFINE_HOOK(&daArrow_c::procWait, BulletTimeArrowWaitHook);
DEFINE_HOOK(&daAlink_c::setBodyAngleXReadyAnime, BulletTimeReadyBodyAngleHook);
DEFINE_HOOK(&daAlink_c::checkAimContext, BulletTimeAimContextHook);
DEFINE_HOOK(&daAlink_c::checkAimInputContext, BulletTimeAimInputContextHook);
DEFINE_HOOK(&dCamera_c::Run, BulletTimeCameraRunHook);
DEFINE_HOOK(&fpcM_Management, BulletTimeManagementHook);
DEFINE_HOOK(&fpcM_DrawIterater, BulletTimeDrawIteraterHook);
DEFINE_HOOK(&dDlst_shadowControl_c::imageDraw, BulletTimeShadowImageHook);
DEFINE_HOOK_SYMBOL("J3DShapePacket::draw", void(J3DShapePacket*), BulletTimePacketDrawHook);
DEFINE_HOOK_SYMBOL("J3DShapePacket::drawFast", void(J3DShapePacket*), BulletTimePacketDrawFastHook);
DEFINE_HOOK_SYMBOL("J3DShape::draw", void(const J3DShape*), BulletTimeShapeDrawHook);
DEFINE_HOOK_SYMBOL("J3DShape::drawFast", void(const J3DShape*), BulletTimeShapeDrawFastHook);
DEFINE_HOOK_SYMBOL("JPAResource::draw", void(JPAResource*, JPAEmitterWorkData*, JPABaseEmitter*), BulletTimeParticleDrawHook);
DEFINE_HOOK_SYMBOL("dusk::gyro::getAimDeltas", void(float&, float&), BulletTimeGyroDeltasHook);
DEFINE_HOOK_SYMBOL("dusk::mouse::get_camera_deltas", void(float&, float&), BulletTimeMouseCameraHook);

int extra_steps_for_tick(f32& accumulator) {
    if (s_linkScale >= kNoSlowScale) return 0;
    accumulator += 1.0f / s_linkScale;
    int total = static_cast<int>(accumulator);
    if (total < 1) total = 1;
    accumulator -= static_cast<f32>(total);
    return total - 1;
}

HookAction on_management_pre(ModContext*, void*, void*, void*) {
    s_inSimTick = true;
    s_appliedThisTick = false;
    s_viewSplit = false;
    return HOOK_CONTINUE;
}

void on_management_post(ModContext*, void*, void*, void*) {
    s_inSimTick = false;
}

HookAction on_draw_iterater_pre(ModContext*, void*, void*, void*) {
    if (s_inSimTick) return HOOK_CONTINUE;
    if (!s_presentationSeen && live_aim_active()) {
        s_presentationSeen = true;
        bt_log("live aim running on presentation frames");
    }
    sample_live_aim();
    s_viewSplit = false;
    view_class* view = dComIfGd_getView();
    if (view == nullptr) return HOOK_CONTINUE;
    MTXCopy(view->viewMtx, s_baseViewMtx);
    if (!preview_live_aim(*view)) return HOOK_CONTINUE;
    refresh_view_matrices(*view);
    MTXCopy(view->viewMtx, s_liveViewMtx);
    Mtx liveInverse;
    cMtx_inverse(s_liveViewMtx, liveInverse);
    MTXConcat(s_baseViewMtx, liveInverse, s_viewCorrection);
    daAlink_c* link = player_link();
    if (link != nullptr) {
        collect_link_models(link);
        s_viewSplit = true;
    }
    return HOOK_CONTINUE;
}

bool begin_link_view(J3DShapePacket* packet, ViewSwap owner) {
    if (!s_viewSplit || s_inSimTick || s_inShadowPass || s_viewSwap != ViewSwap::None) return false;
    if (packet == nullptr) return false;
    const int index = link_model_index(packet->getModel());
    if (index < 0) return false;
    ++s_linkModelSwaps[index];
    note_link_model_seen(packet->getModel());
    s_viewSwap = owner;
    Mtx* base = packet->getBaseMtxPtr();
    if (base != nullptr) {
        MTXConcat(s_viewCorrection, *base, s_correctedBaseMtx);
        s_basePtrPacket = packet;
        s_savedBasePtr = base;
        packet->setBaseMtxPtr(&s_correctedBaseMtx);
        ++s_stats.basePtrSwaps;
    }
    return true;
}

void end_link_view(ViewSwap owner) {
    if (s_viewSwap != owner) return;
    s_viewSwap = ViewSwap::None;
    if (s_basePtrPacket != nullptr) {
        s_basePtrPacket->setBaseMtxPtr(s_savedBasePtr);
        s_basePtrPacket = nullptr;
        s_savedBasePtr = nullptr;
    }
}

HookAction on_packet_draw_pre(ModContext*, void* args, void*, void*) {
    if (s_viewSplit && !s_inSimTick) ++s_stats.packetDraws;
    if (begin_link_view(mods::arg<J3DShapePacket*>(args, 0), ViewSwap::Packet)) ++s_stats.linkPacketSwaps;
    return HOOK_CONTINUE;
}

void on_packet_draw_post(ModContext*, void*, void*, void*) {
    end_link_view(ViewSwap::Packet);
}

void calc_ybb_cam(JPAEmitterWorkData* work) {
    f32 y = work->mPosCamMtx[1][1];
    f32 z = work->mPosCamMtx[2][1];
    const f32 length = std::sqrt(y * y + z * z);
    if (length > 0.0f) {
        y /= length;
        z /= length;
    }
    work->mYBBCamMtx[0][0] = 1.0f;
    work->mYBBCamMtx[0][1] = 0.0f;
    work->mYBBCamMtx[0][2] = 0.0f;
    work->mYBBCamMtx[0][3] = work->mPosCamMtx[0][3];
    work->mYBBCamMtx[1][0] = 0.0f;
    work->mYBBCamMtx[1][1] = y;
    work->mYBBCamMtx[1][2] = -z;
    work->mYBBCamMtx[1][3] = work->mPosCamMtx[1][3];
    work->mYBBCamMtx[2][0] = 0.0f;
    work->mYBBCamMtx[2][1] = z;
    work->mYBBCamMtx[2][2] = y;
    work->mYBBCamMtx[2][3] = work->mPosCamMtx[2][3];
}

HookAction on_particle_draw_pre(ModContext*, void* args, void*, void*) {
    if (!s_viewSplit || s_inSimTick || s_emitterSwapWork != nullptr) return HOOK_CONTINUE;
    JPAEmitterWorkData* work = mods::arg<JPAEmitterWorkData*>(args, 1);
    if (work == nullptr || !is_link_emitter(mods::arg<JPABaseEmitter*>(args, 2))) return HOOK_CONTINUE;
    MTXCopy(work->mPosCamMtx, s_savedEmitterPosCam);
    MTXCopy(work->mYBBCamMtx, s_savedEmitterYBBCam);
    MTXConcat(s_viewCorrection, s_savedEmitterPosCam, work->mPosCamMtx);
    calc_ybb_cam(work);
    s_emitterSwapWork = work;
    ++s_stats.emitterSwaps;
    return HOOK_CONTINUE;
}

void on_particle_draw_post(ModContext*, void*, void*, void*) {
    if (s_emitterSwapWork == nullptr) return;
    MTXCopy(s_savedEmitterPosCam, s_emitterSwapWork->mPosCamMtx);
    MTXCopy(s_savedEmitterYBBCam, s_emitterSwapWork->mYBBCamMtx);
    s_emitterSwapWork = nullptr;
}

HookAction on_shadow_image_pre(ModContext*, void*, void*, void*) {
    s_inShadowPass = true;
    return HOOK_CONTINUE;
}

void on_shadow_image_post(ModContext*, void*, void*, void*) {
    s_inShadowPass = false;
}

HookAction on_shape_draw_pre(ModContext*, void*, void*, void*) {
    if (s_viewSplit && !s_inSimTick) ++s_stats.shapeDraws;
    if (begin_link_view(j3dSys.getShapePacket(), ViewSwap::Shape)) ++s_stats.linkShapeSwaps;
    return HOOK_CONTINUE;
}

void on_shape_draw_post(ModContext*, void*, void*, void*) {
    end_link_view(ViewSwap::Shape);
}

void on_gyro_deltas_post(ModContext*, void* args, void*, void*) {
    if (s_ownGyroRead) return;
    if (!s_active) {
        ++s_normalGyro.calls;
        s_normalGyro.yawTurn += std::fabs(mods::arg_ref<float>(args, 0) * kRadToAngle);
        s_normalGyro.pitchTurn += std::fabs(mods::arg_ref<float>(args, 1) * kRadToAngle);
        return;
    }
    if (!first_person_wanted(player_link())) return;
    mods::arg_ref<float>(args, 0) = 0.0f;
    mods::arg_ref<float>(args, 1) = 0.0f;
}

void log_normal_gyro() {
    const LiveClock::time_point now = LiveClock::now();
    if (std::chrono::duration<f32>(now - s_lastStatsLog).count() < kStatsLogInterval) return;
    s_lastStatsLog = now;
    if (s_normalGyro.calls > 0 && (s_normalGyro.yawTurn > 0.0f || s_normalGyro.pitchTurn > 0.0f)) {
        bt_log("normal gyro (outside bullet time): calls=%d yawTurn=%.0f pitchTurn=%.0f", s_normalGyro.calls,
               s_normalGyro.yawTurn, s_normalGyro.pitchTurn);
    }
    s_normalGyro = {};
}

void on_mouse_camera_post(ModContext*, void* args, void*, void*) {
    if (!s_inCameraExtra) return;
    mods::arg_ref<float>(args, 0) = 0.0f;
    mods::arg_ref<float>(args, 1) = 0.0f;
}

void track_camera_error(daAlink_c* link) {
    if (!dComIfGp_checkCameraAttentionStatus(link->field_0x317c, kSubjectCameraStatus)) return;
    const cXyz& dir = s_tickViewDir;
    const f32 flat = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    if (flat < 0.001f && std::fabs(dir.y) < 0.001f) return;
    constexpr f32 kRadToDeg = 57.2957795f;
    f32 yawError = std::atan2(dir.x, dir.z) - static_cast<f32>(s_cameraYaw) * kAngleToRad;
    while (yawError > 3.14159265f) yawError -= 6.2831853f;
    while (yawError < -3.14159265f) yawError += 6.2831853f;
    const f32 pitchError = std::atan2(dir.y, flat) + static_cast<f32>(s_cameraPitch) * kAngleToRad;
    const f32 yawDeg = std::fabs(yawError) * kRadToDeg;
    const f32 pitchDeg = std::fabs(pitchError) * kRadToDeg;
    if (yawDeg > s_stats.camYawError) s_stats.camYawError = yawDeg;
    if (pitchDeg > s_stats.camPitchError) s_stats.camPitchError = pitchDeg;
}

void on_camera_run_post(ModContext*, void* args, void*, void*) {
    if (s_inCameraExtra || !scaling_active()) return;
    dCamera_c* camera = mods::arg<dCamera_c*>(args, 0);
    if (camera == nullptr || camera->CameraID() != 0) return;
    const int extra = extra_steps_for_tick(s_cameraAccumulator);
    s_stats.cameraRuns += 1 + (extra > 0 ? extra : 0);
    if (extra > 0) {
        interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
        const u16 pressed = pad.mPressedButtonFlags;
        pad.mPressedButtonFlags = 0;
        s_inCameraExtra = true;
        for (int i = 0; i < extra; i++) {
            camera->Run();
        }
        s_inCameraExtra = false;
        pad.mPressedButtonFlags = pressed;
    }
    daAlink_c* link = player_link();
    if (link == nullptr) return;
    s_tickViewDir = camera->Center() - camera->Eye();
    s_cameraYaw = link->field_0x310c;
    s_cameraPitch = link->field_0x310a;
    s_tickViewDirValid = true;
    track_camera_error(link);
    if (!s_appliedThisTick) ++s_stats.cameraFirst;
}

void on_aim_context_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || s_inExtraStep || *static_cast<bool*>(retval)) return;
    if (first_person_wanted(mods::arg<daAlink_c*>(args, 0))) *static_cast<bool*>(retval) = true;
}

void track_subject_camera(bool cameraReady) {
    if (cameraReady == s_subjectCameraOn) return;
    s_subjectCameraOn = cameraReady;
    bt_log("first-person camera %s after %d ticks", cameraReady ? "on" : "off", s_activeTicks);
}

HookAction on_ready_body_angle_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!first_person_wanted(link)) return HOOK_CONTINUE;
    dComIfGp_setPlayerStatus0(0, kBowAimStatus);
    s_aimStatusSet = true;

    const bool liveStick = live_aim_active() && live_stick_ready();
    const f32 moveValue = link->mMoveValue;
    if (liveStick) link->mMoveValue = 0.0f;
    const bool cameraReady = link->setBodyAngleToCamera();
    link->mMoveValue = moveValue;

    if (!s_inExtraStep) {
        apply_live_aim(link);
        track_subject_camera(cameraReady);
        if (!live_aim_active() && !s_liveAimWarned) {
            s_liveAimWarned = true;
            bt_log("live aim unavailable: presentation=%s padRead=%s scaling=%d", s_presentationHooked ? "ok" : "MISSING",
                   found(reinterpret_cast<const void*>(s_padRead)), scaling_active() ? 1 : 0);
        }
    }
    if (!cameraReady) return HOOK_CONTINUE;
    link->setBowSight();
    return HOOK_SKIP_ORIGINAL;
}

enum class BowPhase { None, Charge, Shoot, Wait, Reload };

BowPhase s_bowPhase = BowPhase::None;
bool s_reloadTracking = false;
int s_reloadSteps = 0;
int s_reloadExtraSteps = 0;
LiveClock::time_point s_reloadStart;

BowPhase bow_phase(daAlink_c* link) {
    if (link->checkBowChargeWaitAnime()) return BowPhase::Charge;
    if (link->checkBowShootAnime()) return BowPhase::Shoot;
    if (link->checkBowWaitAnime()) return BowPhase::Wait;
    if (link->checkBowReloadAnime()) return BowPhase::Reload;
    return BowPhase::None;
}

const char* bow_phase_name(BowPhase phase) {
    switch (phase) {
    case BowPhase::Charge: return "charge";
    case BowPhase::Shoot: return "shoot";
    case BowPhase::Wait: return "wait";
    case BowPhase::Reload: return "reload";
    default: return "none";
    }
}

void track_reload(daAlink_c* link, bool extraStep) {
    const BowPhase phase = link->checkWolf() || !is_bow_item(link->mEquipItem) ? BowPhase::None : bow_phase(link);
    if (s_reloadTracking) {
        ++s_reloadSteps;
        if (extraStep) ++s_reloadExtraSteps;
    }
    if (phase == s_bowPhase) return;
    const BowPhase previous = s_bowPhase;
    s_bowPhase = phase;
    if (previous == BowPhase::Charge && phase == BowPhase::Shoot) {
        s_reloadTracking = true;
        s_reloadSteps = 0;
        s_reloadExtraSteps = 0;
        s_reloadStart = LiveClock::now();
        const s16 holdTimer = link->field_0x30a4;
        if (s_active && link->field_0x30a4 > kBombArrowHoldSteps) link->field_0x30a4 = kBombArrowHoldSteps;
        bt_log("reload: shot (bt=%d holdTimer=%d->%d itemMode=%d bombSel=%d)", s_active ? 1 : 0,
               static_cast<int>(holdTimer), static_cast<int>(link->field_0x30a4),
               static_cast<int>(link->mItemMode), static_cast<int>(link->field_0x301e));
        return;
    }
    if (s_active && phase == BowPhase::Reload) {
        daPy_frameCtrl_c& frameCtrl = link->mUpperFrameCtrl[2];
        frameCtrl.setRate(frameCtrl.getRate() * kReloadAnimeSpeedUp);
    }
    if (!s_reloadTracking) return;
    const double ms = std::chrono::duration<double, std::milli>(LiveClock::now() - s_reloadStart).count();
    bt_log("reload: %s -> %s after %d steps (%d extra) %.0f ms (bt=%d holdTimer=%d itemMode=%d)",
           bow_phase_name(previous), bow_phase_name(phase), s_reloadSteps, s_reloadExtraSteps, ms,
           s_active ? 1 : 0, static_cast<int>(link->field_0x30a4), static_cast<int>(link->mItemMode));
    if (phase == BowPhase::Charge || phase == BowPhase::None) s_reloadTracking = false;
}

void release_reaim_block(daAlink_c* link) {
    if (!s_reaimBlock) return;
    const bool itemPressed = (link->mItemTrigger & (1 << link->mSelectItemId)) != 0;
    if (!itemPressed && bow_aiming(link) && airborne(link)) return;
    s_reaimBlock = false;
    bt_log("re-aim unlocked (%s)", itemPressed ? "item pressed" : "aim ended");
}

int run_extra_steps(daAlink_c* link) {
    const int extra = extra_steps_for_tick(s_stepAccumulator);
    if (extra <= 0) return 0;

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const u16 pressed = pad.mPressedButtonFlags;
    pad.mPressedButtonFlags = 0;
    s_inExtraStep = true;
    int done = 0;
    while (done < extra && air_bow_state(link)) {
        BulletTimeExecuteHook::g_orig(link);
        ++done;
        track_reload(link, true);
    }
    if (done < extra) bt_log("extra steps cut: %d/%d (proc=%u)", done, extra, static_cast<unsigned>(link->mProcID));
    s_inExtraStep = false;
    pad.mPressedButtonFlags = pressed;
    return done;
}

void auto_aim_after_air_equip(daAlink_c* link) {
    const bool ready = !link->checkWolf() && is_bow_item(link->mEquipItem) &&
                       !link->checkEquipAnime() && link->checkReadyItem();
    const bool fresh = ready && !s_prevBowReady;
    s_prevBowReady = ready;
    if (!fresh || !g_configBulletTimeEnabled || !airborne(link) || bow_aiming(link)) return;
    link->setBowReadyAnime();
    link->mItemMode = 0;
    bt_log("auto-aim after air equip (item=%u proc=%u)", static_cast<unsigned>(link->mEquipItem),
           static_cast<unsigned>(link->mProcID));
}

fpc_ProcID nocked_arrow_id(daAlink_c* link) {
    fopAc_ac_c* held = link->mItemAcKeep.getActor();
    if (held == nullptr || fopAcM_GetName(held) != fpcNm_ARROW_e) return fpcM_ERROR_PROCESS_ID_e;
    return fopAcM_GetID(held);
}

void launch_released_arrow(fpc_ProcID id) {
    if (id == fpcM_ERROR_PROCESS_ID_e) return;
    auto* arrow = static_cast<daArrow_c*>(fopAcM_SearchByID(id));
    if (arrow == nullptr || fopAcM_GetName(arrow) != fpcNm_ARROW_e) return;
    const u32 param = fopAcM_GetParam(arrow);
    if ((param != 1 && param != 2) || s_waitArrowId != id) return;
    arrow->execute();
    if (s_waitArrowId == id && fopAcM_SearchByID(id) == arrow) arrow->execute();
    ++s_stats.arrowLaunches;
    bt_log("arrow launched in release tick (param=%u tick=%d)", static_cast<unsigned>(param), s_activeTicks);
}

void on_link_execute_post(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link != player_link()) return;
    track_reload(link, false);
    release_reaim_block(link);
    auto_aim_after_air_equip(link);
    if (!s_active) {
        s_nockedArrowId = nocked_arrow_id(link);
        return;
    }
    if (!can_continue(link)) {
        stop_if_needed(link);
        return;
    }
    const fpc_ProcID heldBefore = nocked_arrow_id(link);
    const int extra = run_extra_steps(link);
    launch_released_arrow(s_nockedArrowId);
    if (heldBefore != s_nockedArrowId) launch_released_arrow(heldBefore);
    s_nockedArrowId = nocked_arrow_id(link);
    ++s_activeTicks;
    ++s_stats.ticks;
    s_stats.linkSteps += 1 + extra;
    if (g_configStaminaSrcBulletTime) stamina_add_drain(drain_per_step() * static_cast<f32>(1 + extra));
    stop_if_needed(link);
}

HookAction on_pos_move_pre(ModContext*, void* args, void*, void*) {
    s_move.active = false;
    if (!scaling_active()) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link != player_link() || link->checkWolf()) return HOOK_CONTINUE;
    if (!is_air_proc(link->mProcID)) return HOOK_CONTINUE;
    if (link->checkModeFlg(daAlink_c::MODE_SWIMMING) || link->checkWaterInMove()) return HOOK_CONTINUE;

    const f32 f = s_linkScale;
    s_move.ownGravity = link->checkNoResetFlg3(daPy_py_c::FLG3_UNK_4000) == 0;
    if (s_move.ownGravity) {
        link->gravity = link->mpHIO->mAutoJump.m.mGravity;
        link->maxFallSpeed = link->mpHIO->mAutoJump.m.mMaxFallSpeed;
        link->onNoResetFlg3(daPy_py_c::FLG3_UNK_4000);
    }
    s_move.factor = f;
    s_move.gravity = link->gravity;
    s_move.maxFallSpeed = link->maxFallSpeed;
    s_move.normalSpeed = link->mNormalSpeed;
    link->gravity *= f * f;
    link->maxFallSpeed *= f;
    link->mNormalSpeed *= f;
    link->speed.y *= f;
    s_move.active = true;
    return HOOK_CONTINUE;
}

void on_pos_move_post(ModContext*, void* args, void*, void*) {
    if (!s_move.active) return;
    s_move.active = false;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    const f32 f = s_move.factor;
    const f32 scaledNormal = s_move.normalSpeed * f;
    link->mNormalSpeed = link->mNormalSpeed == scaledNormal ? s_move.normalSpeed : link->mNormalSpeed / f;
    link->speed.x /= f;
    link->speed.y /= f;
    link->speed.z /= f;
    link->speedF /= f;
    link->gravity = s_move.gravity;
    link->maxFallSpeed = s_move.maxFallSpeed;
    if (s_move.ownGravity) link->offNoResetFlg3(daPy_py_c::FLG3_UNK_4000);
}

bool boost_arrow(daArrow_c* arrow) {
    if (arrow->mArrowType == kSlingArrowType || arrow->field_0x945 != 0) return false;
    const u32 param = fopAcM_GetParam(arrow);
    return param == 1 || param == 2;
}

HookAction on_arrow_wait_pre(ModContext*, void* args, void*, void*) {
    daArrow_c* arrow = mods::arg<daArrow_c*>(args, 0);
    if (arrow != nullptr) s_waitArrowId = fopAcM_GetID(arrow);
    return HOOK_CONTINUE;
}

HookAction on_arrow_move_pre(ModContext*, void* args, void*, void*) {
    s_arrowMove.arrow = nullptr;
    daArrow_c* arrow = mods::arg<daArrow_c*>(args, 0);
    if (arrow != nullptr && fopAcM_GetID(arrow) == s_waitArrowId) s_waitArrowId = fpcM_ERROR_PROCESS_ID_e;
    if (!scaling_active()) return HOOK_CONTINUE;
    if (arrow == nullptr || !boost_arrow(arrow)) return HOOK_CONTINUE;
    const f32 boost = 1.0f / s_linkScale;
    s_arrowMove.arrow = arrow;
    s_arrowMove.boost = boost;
    s_arrowMove.outLengthRate = arrow->mOutLengthRate;
    arrow->speed *= boost;
    arrow->mOutLengthRate /= boost;
    ++s_stats.arrowBoosts;
    return HOOK_CONTINUE;
}

void on_arrow_move_post(ModContext*, void* args, void*, void*) {
    daArrow_c* arrow = s_arrowMove.arrow;
    s_arrowMove.arrow = nullptr;
    if (arrow == nullptr || arrow != mods::arg<daArrow_c*>(args, 0)) return;
    if (arrow->field_0x945 != 0) return;
    const f32 boost = s_arrowMove.boost;
    arrow->speed /= boost;
    if (arrow->gravity < 0.0f) arrow->speed.y += arrow->gravity * (boost - 1.0f / boost);
    arrow->mOutLengthRate = s_arrowMove.outLengthRate;
}

template <class Entry>
ModResult add_pre_logged(const char* name, HookPreFn callback) {
    const ModResult result = mods::hook::add_pre<Entry>(s_hookSvc, callback);
    bt_log("hook pre  %s -> %s (%d)", name, result == MOD_OK ? "ok" : "FAILED", static_cast<int>(result));
    return result;
}

template <class Entry>
ModResult add_post_logged(const char* name, HookPostFn callback) {
    const ModResult result = mods::hook::add_post<Entry>(s_hookSvc, callback);
    bt_log("hook post %s -> %s (%d)", name, result == MOD_OK ? "ok" : "FAILED", static_cast<int>(result));
    return result;
}

void install_hooks() {
    if (s_hooksInstalled || s_hookSvc == nullptr) return;
    add_post_logged<BulletTimeExecuteHook>("daAlink_c::execute", on_link_execute_post);
    add_pre_logged<BulletTimePosMoveHook>("daAlink_c::posMove", on_pos_move_pre);
    add_post_logged<BulletTimePosMoveHook>("daAlink_c::posMove", on_pos_move_post);
    add_pre_logged<BulletTimeArrowMoveHook>("daArrow_c::procMove", on_arrow_move_pre);
    add_post_logged<BulletTimeArrowMoveHook>("daArrow_c::procMove", on_arrow_move_post);
    add_pre_logged<BulletTimeArrowWaitHook>("daArrow_c::procWait", on_arrow_wait_pre);
    add_pre_logged<BulletTimeReadyBodyAngleHook>("daAlink_c::setBodyAngleXReadyAnime", on_ready_body_angle_pre);
    add_post_logged<BulletTimeAimContextHook>("daAlink_c::checkAimContext", on_aim_context_post);
    add_post_logged<BulletTimeAimInputContextHook>("daAlink_c::checkAimInputContext", on_aim_context_post);
    add_post_logged<BulletTimeCameraRunHook>("dCamera_c::Run", on_camera_run_post);
    const bool managementPre = add_pre_logged<BulletTimeManagementHook>("fpcM_Management", on_management_pre) == MOD_OK;
    const bool managementPost = add_post_logged<BulletTimeManagementHook>("fpcM_Management", on_management_post) == MOD_OK;
    const bool drawIterater = add_pre_logged<BulletTimeDrawIteraterHook>("fpcM_DrawIterater", on_draw_iterater_pre) == MOD_OK;
    s_presentationHooked = managementPre && managementPost && drawIterater;
    add_pre_logged<BulletTimeShadowImageHook>("dDlst_shadowControl_c::imageDraw", on_shadow_image_pre);
    add_post_logged<BulletTimeShadowImageHook>("dDlst_shadowControl_c::imageDraw", on_shadow_image_post);
    add_pre_logged<BulletTimePacketDrawHook>("J3DShapePacket::draw", on_packet_draw_pre);
    add_post_logged<BulletTimePacketDrawHook>("J3DShapePacket::draw", on_packet_draw_post);
    add_pre_logged<BulletTimePacketDrawFastHook>("J3DShapePacket::drawFast", on_packet_draw_pre);
    add_post_logged<BulletTimePacketDrawFastHook>("J3DShapePacket::drawFast", on_packet_draw_post);
    add_pre_logged<BulletTimeShapeDrawHook>("J3DShape::draw", on_shape_draw_pre);
    add_post_logged<BulletTimeShapeDrawHook>("J3DShape::draw", on_shape_draw_post);
    add_pre_logged<BulletTimeShapeDrawFastHook>("J3DShape::drawFast", on_shape_draw_pre);
    add_post_logged<BulletTimeShapeDrawFastHook>("J3DShape::drawFast", on_shape_draw_post);
    add_pre_logged<BulletTimeParticleDrawHook>("JPAResource::draw", on_particle_draw_pre);
    add_post_logged<BulletTimeParticleDrawHook>("JPAResource::draw", on_particle_draw_post);
    add_post_logged<BulletTimeGyroDeltasHook>("dusk::gyro::getAimDeltas", on_gyro_deltas_post);
    add_post_logged<BulletTimeMouseCameraHook>("dusk::mouse::get_camera_deltas", on_mouse_camera_post);
    s_hooksInstalled = true;
}

template <class Fn>
void resolve_function(const HookService* hook_svc, const char* symbol, Fn& out) {
    void* address = nullptr;
    if (hook_svc->resolve(mod_ctx, symbol, &address, nullptr) != MOD_OK) address = nullptr;
    out = reinterpret_cast<Fn>(address);
    bt_log("resolve %s -> %s", symbol, found(address));
}

void resolve_functions(const HookService* hook_svc) {
    resolve_function(hook_svc, "PADGetSensorData", s_padSensor);
    resolve_function(hook_svc, "dusk::gyro::get_sensor_keep_alive", s_gyroKeepAliveGet);
    resolve_function(hook_svc, "dusk::gyro::set_sensor_keep_alive", s_gyroKeepAliveSet);
    resolve_function(hook_svc, "dusk::gyro::getAimDeltas", s_gyroAimDeltas);
    resolve_function(hook_svc, "PADRead", s_padRead);
    resolve_function(hook_svc, "PADClamp", s_padClamp);
    resolve_function(hook_svc, "PADClampCircle", s_padClampCircle);
}

}

void bullet_time_apply_enabled() {
    if (s_hookSvc == nullptr) return;

    bt_log("enabled=%d timescale=%s", g_configBulletTimeEnabled ? 1 : 0,
           general_timescale_available() ? "ok" : "unavailable");
    if (g_configBulletTimeEnabled && general_timescale_available()) {
        install_hooks();
    } else {
        stop_bullet_time("disabled");
    }
}

ModResult init_bullet_time(const HookService* hook_svc, const LogService* log_svc, ModError*) {
    if (hook_svc == nullptr) return MOD_ERROR;
    s_hookSvc = hook_svc;
    s_log = log_svc;

    GetConfigVarFn getConfigVar = nullptr;
    if (hook_svc->resolve(mod_ctx, "dusk::config::GetConfigVar",
                          reinterpret_cast<void**>(&getConfigVar), nullptr) == MOD_OK &&
        getConfigVar != nullptr)
    {
        s_frameInterpVar = getConfigVar("game.enableFrameInterpolation");
        s_settingVars.gyroAim = getConfigVar("game.enableGyroAim");
        s_settingVars.gyroSensX = getConfigVar("game.gyroSensitivityX");
        s_settingVars.gyroSensY = getConfigVar("game.gyroSensitivityY");
        s_settingVars.gyroSmoothing = getConfigVar("game.gyroSmoothing");
        s_settingVars.gyroDeadband = getConfigVar("game.gyroDeadband");
        s_settingVars.gyroInvertPitch = getConfigVar("game.gyroInvertPitch");
        s_settingVars.gyroInvertYaw = getConfigVar("game.gyroInvertYaw");
        s_settingVars.mirror = getConfigVar("game.enableMirrorMode");
        s_settingVars.invertX = getConfigVar("game.invertFirstPersonXAxis");
        s_settingVars.invertY = getConfigVar("game.invertFirstPersonYAxis");
    }
    bt_log("init: frameInterp=%s gyroAim=%s gyroSens=%s/%s gyroSmoothing=%s gyroDeadband=%s gyroInvert=%s/%s "
           "mirror=%s invertX=%s invertY=%s",
           found(s_frameInterpVar), found(s_settingVars.gyroAim), found(s_settingVars.gyroSensX),
           found(s_settingVars.gyroSensY), found(s_settingVars.gyroSmoothing), found(s_settingVars.gyroDeadband),
           found(s_settingVars.gyroInvertPitch), found(s_settingVars.gyroInvertYaw),
           found(s_settingVars.mirror), found(s_settingVars.invertX), found(s_settingVars.invertY));
    resolve_functions(hook_svc);

    bullet_time_apply_enabled();
    return MOD_OK;
}

void update_bullet_time(const LogService*, ModContext*) {
    if (s_hookSvc == nullptr) return;
    if (g_configBulletTimeEnabled && !s_hooksInstalled && general_timescale_available()) {
        install_hooks();
    }

    daAlink_c* link = player_link();
    if (s_needLanding && (link == nullptr || !airborne(link))) s_needLanding = false;

    if (s_active) {
        interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
        if ((pad.mPressedButtonFlags & PAD_BUTTON_A) != 0) {
            pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
            s_reaimBlock = true;
            stop_bullet_time("A pressed");
        } else {
            stop_if_needed(link);
        }
    } else if (s_hooksInstalled && can_start(link)) {
        start_bullet_time(link);
    }
    if (s_active) {
        s_linkScale = live_scale();
        log_stats();
    } else {
        log_normal_gyro();
    }
    if (!first_person_wanted(link)) release_aim_status();
    sync_gyro_keep_alive();
}

void shutdown_bullet_time() {
    stop_bullet_time("shutdown");
    release_aim_status();
    sync_gyro_keep_alive();
    clear_live_aim();
    s_needLanding = false;
    s_reaimBlock = false;
    s_move.active = false;
    s_arrowMove.arrow = nullptr;
    s_presentationHooked = false;
    s_inSimTick = false;
    s_nockedArrowId = fpcM_ERROR_PROCESS_ID_e;
    s_waitArrowId = fpcM_ERROR_PROCESS_ID_e;
    s_viewSplit = false;
    s_viewSwap = ViewSwap::None;
    s_linkModelCount = 0;
    s_linkEmitterCount = 0;
    s_emitterSwapWork = nullptr;
    s_seenLinkModelCount = 0;
    s_basePtrPacket = nullptr;
    s_savedBasePtr = nullptr;
    s_inShadowPass = false;
    s_hookSvc = nullptr;
    s_frameInterpVar = nullptr;
    s_padSensor = nullptr;
    s_gyroKeepAliveGet = nullptr;
    s_gyroKeepAliveSet = nullptr;
    s_gyroAimDeltas = nullptr;
    s_settingVars = {};
    s_padRead = nullptr;
    s_padClamp = nullptr;
    s_padClampCircle = nullptr;
    s_ownGyroRead = false;
    s_inCameraExtra = false;
    s_log = nullptr;
}
