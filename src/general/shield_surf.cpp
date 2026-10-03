#include "shield_surf.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_bg_w_base.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_obj_iceleaf.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_mtx.h"
#include "SSystem/SComponent/c_math.h"

#include <cmath>

bool g_configShieldSurf = true;

DEFINE_HOOK(&daAlink_c::execute, ShieldSurfExecuteHook);
DEFINE_HOOK(&daAlink_c::boardCommon, ShieldSurfBoardCommonHook);
DEFINE_HOOK(&daAlink_c::setItemMatrix, ShieldSurfItemMatrixHook);
DEFINE_HOOK(&daAlink_c::rideGetOff, ShieldSurfRideGetOffHook);
DEFINE_HOOK(&daAlink_c::startRestartRoom, ShieldSurfRestartRoomHook);

static constexpr u32 kLeafParams = 0x0001FFFF;
static constexpr int kPendingTimeout = 60;
static constexpr f32 kHopSpeed = 16.0f;
static constexpr f32 kMinStartSpeed = 8.0f;
static constexpr f32 kGroundFriction = 0.2f;
static constexpr s16 kFlatSlope = 1400;
static constexpr f32 kStopSpeed = 1.0f;
static constexpr int kStopFrames = 30;
static constexpr f32 kRollOffSpeed = 12.0f;
static constexpr f32 kShieldLift = 9.0f;
static constexpr f32 kShieldCenterX = 14.7f;
static constexpr f32 kShieldCenterY = 3.3f;

enum SurfState {
    SURF_IDLE,
    SURF_PENDING,
    SURF_ACTIVE,
};

static SurfState s_state = SURF_IDLE;
static fpc_ProcID s_leafId = fpcM_ERROR_PROCESS_ID_e;
static int s_pendingFrames = 0;
static int s_stopFrames = 0;
static bool s_lockArmed = false;
static int s_procBefore = -1;
static bool s_codeSpoofed = false;
static u8 s_savedCode = 0;

static bool grounded(daAlink_c* link) {
    return link->mLinkAcch.ChkGroundHit() && !link->checkModeFlg(daAlink_c::MODE_JUMP);
}

static bool is_ground_start_proc(int proc) {
    switch (proc) {
    case daAlink_c::PROC_WAIT:
    case daAlink_c::PROC_MOVE:
    case daAlink_c::PROC_ATN_MOVE:
    case daAlink_c::PROC_ATN_ACTOR_WAIT:
    case daAlink_c::PROC_ATN_ACTOR_MOVE:
    case daAlink_c::PROC_WAIT_TURN:
    case daAlink_c::PROC_MOVE_TURN:
        return true;
    default:
        return false;
    }
}

static bool is_air_start_proc(int proc) {
    switch (proc) {
    case daAlink_c::PROC_SIDESTEP:
    case daAlink_c::PROC_BACK_JUMP:
    case daAlink_c::PROC_AUTO_JUMP:
    case daAlink_c::PROC_FALL:
    case daAlink_c::PROC_SMALL_JUMP:
        return true;
    default:
        return false;
    }
}

static bool is_landing_proc(int proc) {
    return proc == daAlink_c::PROC_LAND || proc == daAlink_c::PROC_SIDESTEP_LAND ||
           proc == daAlink_c::PROC_BACK_JUMP_LAND;
}

static bool in_water(daAlink_c* link) {
    return link->checkNoResetFlg0(daPy_py_c::FLG0_UNK_80) &&
           link->mWaterY - link->current.pos.y > link->mpHIO->mSwim.m.mStartHeight;
}

static bool can_surf(daAlink_c* link) {
    if (!g_configShieldSurf || link == nullptr) return false;
    if (link->checkWolf() || !daPy_py_c::checkShieldGet()) return false;
    if (link->checkEventRun() || link->checkRideOn()) return false;
    if (link->checkEquipHeavyBoots()) return false;
    if (link->checkModeFlg(daAlink_c::MODE_SWIMMING) || in_water(link)) return false;
    return link->mpLinkModel != nullptr && link->mShieldModel != nullptr;
}

static bool owns_ride(daAlink_c* link) {
    return link != nullptr && s_leafId != fpcM_ERROR_PROCESS_ID_e &&
           link->mRideStatus == daAlink_c::RIDETYPE_BOARD &&
           link->mRideAcKeep.mID == s_leafId;
}

static void release_leaf(daAlink_c* link) {
    if (s_leafId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* leaf = fopAcM_SearchByID(s_leafId);
        if (leaf != nullptr) fopAcM_delete(leaf);
        if (link != nullptr && link->mRideAcKeep.mID == s_leafId) {
            link->mRideAcKeep.mID = fpcM_ERROR_PROCESS_ID_e;
            link->mRideAcKeep.mActor = nullptr;
        }
    }
    s_leafId = fpcM_ERROR_PROCESS_ID_e;
    s_state = SURF_IDLE;
    s_pendingFrames = 0;
    s_stopFrames = 0;
    s_lockArmed = false;
}

static void dismount(daAlink_c* link) {
    if (!grounded(link)) {
        link->procFallInit(1, link->mpHIO->mAutoJump.m.mFallInterpolation);
    } else if (link->mNormalSpeed > kRollOffSpeed) {
        link->procFrontRollInit();
    } else {
        link->procWaitInit();
    }
    if (owns_ride(link)) link->rideGetOff();
    release_leaf(link);
}

static bool in_start_window(daAlink_c* link) {
    if (!link->checkAttentionLock() && !link->checkPlayerGuard()) return false;
    return is_air_start_proc(link->mProcID) && !link->mLinkAcch.ChkGroundHit();
}

static bool start_requested(daAlink_c* link) {
    if (!link->doTrigger()) return false;
    if (static_cast<int>(link->mProcID) != s_procBefore) return false;
    return in_start_window(link);
}

bool shield_surf_wants_a() {
    if (!g_configShieldSurf) return false;
    if (s_state != SURF_IDLE) return true;
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    return can_surf(link) && in_start_window(link);
}

static void spawn_leaf(daAlink_c* link) {
    static const cXyz kLeafScale(0.001f, 0.001f, 0.001f);
    s_leafId = fopAcM_create(fpcNm_Obj_IceLeaf_e, kLeafParams, &link->current.pos,
                             fopAcM_GetRoomNo(link), &link->shape_angle, &kLeafScale, -1);
    if (s_leafId == fpcM_ERROR_PROCESS_ID_e) return;
    s_state = SURF_PENDING;
    s_pendingFrames = 0;
}

static void begin_ride(daAlink_c* link, daObjIceLeaf_c* leaf) {
    leaf->setMode(daObjIceLeaf_c::MODE_RIDE_e);

    const bool air = !link->mLinkAcch.ChkGroundHit() || link->checkModeFlg(daAlink_c::MODE_JUMP);
    const f32 vx = link->speed.x;
    const f32 vz = link->speed.z;
    const f32 vy = link->speed.y;
    const f32 hspeed = std::sqrt(vx * vx + vz * vz);
    if (hspeed > 1.0f) {
        link->shape_angle.y = cM_atan2s(vx, vz);
        link->current.angle.y = link->shape_angle.y;
    }

    if (!link->procBoardWaitInit(leaf) || !owns_ride(link)) {
        release_leaf(link);
        return;
    }

    f32 start = hspeed > kMinStartSpeed ? hspeed : kMinStartSpeed;
    if (start > link->mMaxSpeed) start = link->mMaxSpeed;
    link->mNormalSpeed = start;

    link->procBoardJumpInit(0.0f, TRUE);
    if (air) {
        link->speed.y = vy;
    } else {
        link->speed.y = kHopSpeed;
        link->mLinkAcch.ClrGroundHit();
    }

    s_state = SURF_ACTIVE;
    s_stopFrames = 0;
    s_lockArmed = !link->checkAttentionLock();
}

static void tick_ride(daAlink_c* link, daObjIceLeaf_c* leaf) {
    leaf->setMode(daObjIceLeaf_c::MODE_RIDE_e);

    if (link->checkEventRun()) return;

    if (in_water(link) || link->doTrigger()) {
        dismount(link);
        return;
    }

    const bool lock = link->checkAttentionLock();
    if (!lock) {
        s_lockArmed = true;
    } else if (s_lockArmed) {
        dismount(link);
        return;
    }

    if (link->mProcID == daAlink_c::PROC_BOARD_WAIT && grounded(link) &&
        link->mNormalSpeed < kStopSpeed && !link->checkInputOnR()) {
        if (++s_stopFrames >= kStopFrames) {
            dismount(link);
            return;
        }
    } else {
        s_stopFrames = 0;
    }
}

static HookAction execute_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    s_procBefore = link != nullptr ? static_cast<int>(link->mProcID) : -1;
    return HOOK_CONTINUE;
}

static void execute_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;

    switch (s_state) {
    case SURF_IDLE:
        if (can_surf(link) && start_requested(link)) spawn_leaf(link);
        break;

    case SURF_PENDING: {
        fopAc_ac_c* leaf = fopAcM_SearchByID(s_leafId);
        const int proc = link->mProcID;
        const bool procOk = is_ground_start_proc(proc) || is_air_start_proc(proc) ||
                            is_landing_proc(proc);
        if (!can_surf(link) || !procOk) {
            release_leaf(link);
        } else if (leaf != nullptr) {
            begin_ride(link, static_cast<daObjIceLeaf_c*>(leaf));
        } else if (++s_pendingFrames > kPendingTimeout) {
            release_leaf(link);
        }
        break;
    }

    case SURF_ACTIVE: {
        if (!owns_ride(link)) {
            release_leaf(link);
            break;
        }
        fopAc_ac_c* leaf = fopAcM_SearchByID(s_leafId);
        if (!g_configShieldSurf || leaf == nullptr) {
            dismount(link);
            break;
        }
        tick_ride(link, static_cast<daObjIceLeaf_c*>(leaf));
        break;
    }
    }
}

static HookAction board_common_pre(ModContext*, void* args, void*, void*) {
    s_codeSpoofed = false;
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!owns_ride(link)) return HOOK_CONTINUE;
    const u8 code = link->mGndPolySpecialCode;
    if (code == dBgW_SPCODE_LIGHT_SNOW || code == dBgW_SPCODE_HEAVY_SNOW) return HOOK_CONTINUE;
    s_savedCode = code;
    link->mGndPolySpecialCode = dBgW_SPCODE_LIGHT_SNOW;
    s_codeSpoofed = true;
    return HOOK_CONTINUE;
}

static void board_common_post(ModContext*, void* args, void*, void*) {
    if (!s_codeSpoofed || !args) return;
    s_codeSpoofed = false;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr) return;
    link->mGndPolySpecialCode = s_savedCode;
    if (!owns_ride(link) || !grounded(link)) return;
    if (link->mProcVar3.field_0x300e > kFlatSlope) return;
    link->mNormalSpeed -= kGroundFriction;
    if (link->mNormalSpeed < 0.0f) link->mNormalSpeed = 0.0f;
}

static void item_matrix_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!owns_ride(link) || link->mShieldModel == nullptr || link->mpLinkModel == nullptr) return;
    if (link->mShieldChangeWaitTimer != 0 || link->mClothesChangeWaitTimer != 0) return;
    if (link->checkStatusWindowDraw()) return;

    s16 footAngle = 0;
    if (!link->checkBoardNoFootAngle()) {
        Vec right;
        Vec left;
        mDoMtx_multVec(link->mInvMtx, link->getRightFootPosP(), &right);
        mDoMtx_multVec(link->mInvMtx, link->getLeftFootPosP(), &left);
        footAngle = cM_atan2s(left.y - right.y, right.z - left.z);
    }

    mDoMtx_stack_c::copy(link->mpLinkModel->getBaseTRMtx());
    mDoMtx_stack_c::ZXYrotM(footAngle, link->getBoardCutTurnOffsetAngleY(), 0);
    mDoMtx_stack_c::transM(0.0f, kShieldLift, 0.0f);
    mDoMtx_stack_c::XrotM(0x4000);
    mDoMtx_stack_c::transM(kShieldCenterX, kShieldCenterY, 0.0f);
    link->mShieldModel->setBaseTRMtx(mDoMtx_stack_c::get());
    link->mShieldModel->calc();
}

static HookAction ride_get_off_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!owns_ride(link)) return HOOK_CONTINUE;
    release_leaf(link);
    link->mRideStatus = 0;
    link->offNoResetFlg1(daPy_py_c::daPy_FLG1(daPy_py_c::FLG1_UNK_1000 | daPy_py_c::FLG1_UNK_800));
    link->attention_info.field_0xa = 10;
    link->shape_angle.x = 0;
    link->shape_angle.z = 0;
    return HOOK_SKIP_ORIGINAL;
}

static HookAction restart_room_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (owns_ride(link)) {
        link->rideGetOff();
    } else if (s_state == SURF_PENDING) {
        release_leaf(link);
    }
    return HOOK_CONTINUE;
}

ModResult init_shield_surf(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<ShieldSurfExecuteHook>(hook_svc, execute_pre);
    mods::hook::add_post<ShieldSurfExecuteHook>(hook_svc, execute_post);
    mods::hook::add_pre<ShieldSurfBoardCommonHook>(hook_svc, board_common_pre);
    mods::hook::add_post<ShieldSurfBoardCommonHook>(hook_svc, board_common_post);
    mods::hook::add_post<ShieldSurfItemMatrixHook>(hook_svc, item_matrix_post);
    mods::hook::add_pre<ShieldSurfRideGetOffHook>(hook_svc, ride_get_off_pre);
    mods::hook::add_pre<ShieldSurfRestartRoomHook>(hook_svc, restart_room_pre);
    return MOD_OK;
}

void shutdown_shield_surf() {
    daAlink_c* link = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (s_state == SURF_ACTIVE && owns_ride(link)) {
        dismount(link);
    } else {
        release_leaf(nullptr);
    }
    s_procBefore = -1;
    s_codeSpoofed = false;
}
