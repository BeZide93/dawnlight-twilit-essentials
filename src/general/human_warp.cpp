#include "human_warp.hpp"

#include "mods/hook.hpp"
#include "mods/service.hpp"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_particle_name.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_math.h"

#include <cstring>

bool g_configGeneralHumanWarpAnimation = false;

DEFINE_HOOK(&daAlink_c::checkWarpStart, GeneralHumanWarpStartHook);
DEFINE_HOOK(&daAlink_c::skipPortalObjWarp, GeneralHumanWarpArrivalHook);
DEFINE_HOOK(&mDoCPd_c::read, GeneralHumanWarpPadReadHook);
DEFINE_HOOK(&dCamera_c::Run, GeneralHumanWarpCameraRunHook);

static bool s_humanWarpInFlight = false;

static bool s_humanWarpArrivalPending = false;
static int s_humanWarpArrivalFrames = 0;

static bool s_cineDeparture = false;
static bool s_cineArrival = false;

static int s_wolfArrivalFxFrames = 0;
static bool s_wolfArrivalMidnaPoked = false;

static constexpr int kArrivalGiveUpFrames = 600;

static void on_pad_read_post(ModContext*, void*, void*, void*) {
    if (!s_cineDeparture && !s_cineArrival) return;

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    pad.mCStickPosX = 0.0f;
    pad.mCStickPosY = 0.0f;
    pad.mCStickValue = 0.0f;
    pad.mCStickAngle = 0;
    pad.mButtonFlags &= ~PAD_TRIGGER_Z;
    pad.mPressedButtonFlags &= ~PAD_TRIGGER_Z;
}

static camera_process_class* human_warp_active_camera() {
    return dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
}

static void human_warp_pin_camera(bool i_arrival) {
    daAlink_c* link = daAlink_getAlinkActorClass();
    camera_process_class* cam = human_warp_active_camera();
    if (link == nullptr || cam == nullptr) return;

    const cXyz& p = link->current.pos;
    const s16 ang = link->shape_angle.y;

    cXyz center;
    cXyz eye;
    s16 camAng;
    if (i_arrival) {
        camAng = ang;
        const f32 fx = cM_ssin(ang), fz = cM_scos(ang);
        center.set(p.x + fx * 150.0f, p.y + 90.0f, p.z + fz * 150.0f);
        eye.set(p.x - fx * 300.0f, p.y + 140.0f, p.z - fz * 300.0f);
    } else {
        camAng = ang + cM_deg2s(35.0f);
        const f32 fx = cM_ssin(camAng), fz = cM_scos(camAng);
        center.set(p.x + fx * 60.0f, p.y + 80.0f, p.z + fz * 60.0f);
        eye.set(p.x + fx * 320.0f, p.y + 70.0f, p.z + fz * 320.0f);
    }

    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    fopCamM_SetAngleY(cam, camAng);
}

static void on_camera_run_post(ModContext*, void*, void*, void*) {
    if (s_cineDeparture) {
        dCam_getBody()->SetTrimTypeForce(2);
        dComIfGp_2dShowOff();
        human_warp_pin_camera(false);
        return;
    }

    if (s_cineArrival) {
        dCam_getBody()->SetTrimTypeForce(2);
        dComIfGp_2dShowOff();

        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr && link->mProcID == daAlink_c::PROC_WARP) {
            human_warp_pin_camera(true);
        }
    }
}

static bool human_warp_scene_load_stable() {
    const char* cur = dComIfGp_getStartStageName();
    const char* next = dComIfGp_getNextStageName();
    if (next != nullptr && next[0] != '\0' &&
        (cur == nullptr || std::strcmp(next, cur) != 0)) {
        return false;
    }

    static char s_stableStage[16] = {0};
    static s32 s_stableRoom = -1;
    static int s_stableFrames = 0;

    const char* stage = (cur != nullptr) ? cur : "";
    daAlink_c* link = daAlink_getAlinkActorClass();
    s32 room = (link != nullptr) ? fopAcM_GetRoomNo(link) : -1;

    if (std::strncmp(s_stableStage, stage, sizeof(s_stableStage) - 1) != 0 ||
        s_stableRoom != room) {
        std::strncpy(s_stableStage, stage, sizeof(s_stableStage) - 1);
        s_stableStage[sizeof(s_stableStage) - 1] = '\0';
        s_stableRoom = room;
        s_stableFrames = 0;
        return false;
    }
    if (s_stableFrames < 2) {
        s_stableFrames++;
        return false;
    }
    return true;
}

static HookAction on_check_warp_start_pre(ModContext*, void*, void*, void*) {
    if (!g_configGeneralHumanWarpAnimation) return HOOK_CONTINUE;

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr || link->checkWolf()) return HOOK_CONTINUE;

    if (g_meter2_info.getWarpStatus() != WARP_STATUS_DECIDED_e) return HOOK_CONTINUE;

    link->allUnequip(0);
    link->mNormalSpeed = 0.0f;
    link->speed.set(0.0f, 0.0f, 0.0f);

    if (link->procCoWarpInit(0, 1)) {
        link->field_0x347c = 4.6f;
        g_meter2_info.resetWarpStatus();
        s_humanWarpInFlight = true;

        s_cineDeparture = true;
        return HOOK_SKIP_ORIGINAL;
    }

    return HOOK_CONTINUE;
}

static HookAction on_skip_portal_obj_warp_pre(ModContext*, void*, void*, void*) {
    if (!s_humanWarpInFlight) return HOOK_CONTINUE;
    s_humanWarpInFlight = false;
    s_cineDeparture = false;

    dComIfGp_setNextStage(g_meter2_info.getWarpStageName(),
                          static_cast<s16>(g_meter2_info.getWarpPlayerNo()),
                          static_cast<s8>(g_meter2_info.getWarpRoomNo()), -1,
                          0.0f, 0, 1, 0, 0, 1, 0);
    s_humanWarpArrivalPending = true;
    s_humanWarpArrivalFrames = 0;
    return HOOK_SKIP_ORIGINAL;
}

static void human_warp_wolf_arrival_emitters(daAlink_c* link) {
    static u16 const effName[] = {
        ID_ZI_J_WL_WARP_APP_A, ID_ZI_J_WL_WARP_APP_B, ID_ZI_J_WL_WARP_APP_C,
        ID_ZI_J_WL_WARP_APP_D, ID_ZI_J_WL_WARP_APP_E, ID_ZI_J_WL_WARP_APP_F,
    };
    for (int i = 0; i < 6; i++) {
        link->setEmitter(&link->field_0x3240[i], effName[i], &link->current.pos,
                         &link->shape_angle);
    }
}

void update_human_warp(const LogService*, ModContext*) {
    if (s_cineArrival) {
        if (++s_humanWarpArrivalFrames > kArrivalGiveUpFrames) {
            s_cineArrival = false;
            s_wolfArrivalFxFrames = 0;
            if (s_wolfArrivalMidnaPoked) {
                s_wolfArrivalMidnaPoked = false;
                daMidna_c* midna = daPy_py_c::getMidnaActor();
                if (midna != nullptr) {
                    midna->offStateFlg0(static_cast<daMidna_c::daMidna_FLG0>(
                        daMidna_c::FLG0_PORTAL_OBJ_CALL | daMidna_c::FLG0_TAG_WAIT |
                        daMidna_c::FLG0_UNK_200));
                    midna->changeDemoMode(0);
                }
            }
            dCam_getBody()->SetTrimTypeForce(0);
            dComIfGp_2dShowOn();
            return;
        }

        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link == nullptr) return;

        if (link->mProcID == daAlink_c::PROC_WARP) {
            if (s_wolfArrivalFxFrames > 0) {
                --s_wolfArrivalFxFrames;
                if (daPy_py_c::checkNowWolf()) {
                    human_warp_wolf_arrival_emitters(link);
                }
            }
            return;
        }

        s_wolfArrivalFxFrames = 0;
        if (s_wolfArrivalMidnaPoked) {
            s_wolfArrivalMidnaPoked = false;
            daMidna_c* midna = daPy_py_c::getMidnaActor();
            if (midna != nullptr) {
                midna->offStateFlg0(static_cast<daMidna_c::daMidna_FLG0>(
                    daMidna_c::FLG0_PORTAL_OBJ_CALL | daMidna_c::FLG0_TAG_WAIT |
                    daMidna_c::FLG0_UNK_200));
                midna->changeDemoMode(0);
            }
        }
        s_cineArrival = false;
        dCam_getBody()->SetTrimTypeForce(0);
        dComIfGp_2dShowOn();
        return;
    }

    if (!s_humanWarpArrivalPending) return;

    if (!human_warp_scene_load_stable()) return;

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return;

    if (link->mProcID == daAlink_c::PROC_WARP) {
        link->offPlayerNoDraw();
        s_humanWarpArrivalPending = false;
        return;
    }

    if (++s_humanWarpArrivalFrames > kArrivalGiveUpFrames) {
        link->offPlayerNoDraw();
        s_humanWarpArrivalPending = false;
        return;
    }

    if (link->mAnmHeap3.mAnimeHeap != nullptr && link->getClothesChangeWaitTimer() == 0) {
        s_humanWarpArrivalPending = false;
        link->mNormalSpeed = 0.0f;
        link->speed.set(0.0f, 0.0f, 0.0f);
        if (link->procCoWarpInit(1, 0)) {
            if (daPy_py_c::checkNowWolf()) {
                link->mProcVar5.field_0x3012 = 1;
                s_wolfArrivalFxFrames = 150;
                human_warp_wolf_arrival_emitters(link);
                daMidna_c* midna = daPy_py_c::getMidnaActor();
                if (midna != nullptr) {
                    cXyz tagPos = link->current.pos + cXyz(0.0f, 250.0f, 0.0f);
                    midna->onTagWaitPosPortalObj(&tagPos);
                    midna->changeDemoMode(9);
                    s_wolfArrivalMidnaPoked = true;
                }
            }
            link->offPlayerNoDraw();
            s_cineArrival = true;
            s_humanWarpArrivalFrames = 0;
        }
    } else {
        link->onPlayerNoDraw();
    }
}

ModResult init_human_warp(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    mods::hook::add_pre<GeneralHumanWarpStartHook>(hook_svc, on_check_warp_start_pre);
    mods::hook::add_pre<GeneralHumanWarpArrivalHook>(hook_svc, on_skip_portal_obj_warp_pre);
    mods::hook::add_post<GeneralHumanWarpPadReadHook>(hook_svc, on_pad_read_post);
    mods::hook::add_post<GeneralHumanWarpCameraRunHook>(hook_svc, on_camera_run_post);
    return MOD_OK;
}

void human_warp_cinematic_departure() {
    s_cineDeparture = true;
    s_cineArrival = false;
}

void human_warp_cinematic_arrival() {
    s_cineDeparture = false;
    s_cineArrival = true;
    s_humanWarpArrivalFrames = 0;
}

void human_warp_cinematic_end() {
    s_cineDeparture = false;
    s_cineArrival = false;
    dCam_getBody()->SetTrimTypeForce(0);
    dComIfGp_2dShowOn();
}

void human_warp_arm_arrival_replay() {
    s_humanWarpArrivalPending = true;
    s_humanWarpArrivalFrames = 0;
}

void shutdown_human_warp() {
    s_humanWarpInFlight = false;
    s_humanWarpArrivalPending = false;
    s_humanWarpArrivalFrames = 0;
    s_cineDeparture = false;
    s_cineArrival = false;
}
