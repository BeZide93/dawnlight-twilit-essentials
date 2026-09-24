#include "boss_rush_gamemode.hpp"
#include "boss_rush.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush_save.hpp"
#include "../util.hpp"

#include "mods/hook.hpp"
#include "mods/svc/game_mode.h"

#include "d/actor/d_a_alink.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_stage.h"
#include "f_ap/f_ap_game.h"
#include "f_op/f_op_camera_mng.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_scene_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_Reset.h"
#include "SSystem/SComponent/c_math.h"

#include <cstring>

extern const HookService* svc_hook;

DEFINE_HOOK(&fapGm_Execute, BossRushGameModeExecuteHook);
DEFINE_HOOK(&dCamera_c::Run, BossRushPreviewCameraRunHook);

namespace {

constexpr const char* kGameModeId = "bossrush";
constexpr const char* kDawnlightModId = "dev.bezide.dawnlight";
constexpr int kPreviewFadeFrames = 6;
constexpr int kPlayFadeFrames = 5;
constexpr f32 kPreviewOrbitRadius = 900.0f;
constexpr f32 kPreviewEyeHeight = 320.0f;
constexpr f32 kPreviewCenterHeight = 120.0f;
constexpr s16 kPreviewOrbitSpeed = 0x18;
constexpr s16 kPreviewStartAngle = 0;
constexpr int kPreviewBlendFrames = 50;
constexpr f32 kLinkCamCenterAhead = 200.0f;
constexpr f32 kLinkCamCenterHeight = 100.0f;
constexpr f32 kLinkCamEyeBack = 420.0f;
constexpr f32 kLinkCamEyeHeight = 140.0f;

using IsPrelaunchOpenFn = bool (*)();
using ReturnToPrelaunchFn = void (*)();

ModContext* s_modCtx = nullptr;
bool s_registered = false;
IsPrelaunchOpenFn s_isPrelaunchOpen = nullptr;
ReturnToPrelaunchFn s_returnToPrelaunch = nullptr;
bool s_menuReturnPending = false;
fpc_ProcID s_reloadFromScene = fpc_ProcID(-1);
bool s_playPressed = false;
bool s_inChamber = false;
bool s_entering = false;
bool s_leaving = false;
bool s_pendingSaveInit = false;
fpc_ProcID s_chamberFromScene = fpc_ProcID(-1);
fpc_ProcID s_waitSceneId = fpc_ProcID(-1);
bool s_previewing = false;
s16 s_previewAngle = kPreviewStartAngle;
bool s_lastActive = false;
bool s_cardReattachPending = false;
cXyz s_previewCenter;
cXyz s_previewEye;
bool s_previewPinned = false;
bool s_blendInPending = false;
bool s_blendingIn = false;
bool s_blendInCaptured = false;
int s_blendInFrame = 0;
cXyz s_blendInCenter;
cXyz s_blendInEye;
s16 s_blendInAngle = 0;
bool s_blending = false;
int s_blendFrame = 0;

bool prelaunch_open() {
    if (s_isPrelaunchOpen != nullptr) {
        return s_isPrelaunchOpen();
    }
    return !s_playPressed;
}

scene_class* current_scene() {
    return fopScnM_SearchByID(dStage_roomControl_c::getProcID());
}

bool scene_change_allowed(scene_class* scene) {
    if (scene == nullptr || fopOvlpM_IsPeek() || mDoRst::isReset()) {
        return false;
    }
    if (s_waitSceneId != fpc_ProcID(-1)) {
        if (fpcM_GetID(scene) == s_waitSceneId) {
            return false;
        }
        s_waitSceneId = fpc_ProcID(-1);
    }
    return true;
}

void apply_chamber_save() {
    dComIfGs_init();
    dComIfGs_setNoFile(1);
    dComIfGs_setDataNum(0);
    boss_rush_begin_game_mode();
    dKy_clear_game_init();
    dComIfGs_resetDan();
}

bool screen_is_black() {
    JUTFader* fader = mDoGph_gInf_c::getFader();
    return fader != nullptr && fader->getStatus() == JUTFader::None;
}

bool enter_chamber_scene(scene_class* scene, int fadeFrames) {
    dComIfGp_offEnableNextStage();
    dComIfGp_setNextStage(kBossRushChamberStage, kBossRushChamberPoint, kBossRushChamberRoom,
                          kBossRushChamberLayer, 0.0f, 0, 1, 0, cM_deg2s(180.0f), 0, 0);
    g_dComIfG_gameInfo.play.mNextStage.getStartStage()->set(
        kBossRushChamberStage, kBossRushChamberRoom, kBossRushChamberPoint, kBossRushChamberLayer);

    mDoGph_gInf_c::setFadeColor(*(JUtility::TColor*)&g_blackColor);
    if (!fopScnM_ChangeReq(scene, fpcNm_PLAY_SCENE_e, 0, fadeFrames)) {
        return false;
    }
    mDoAud_bgmStop(fadeFrames);
    s_pendingSaveInit = true;
    return true;
}

bool leave_chamber_scene(scene_class* scene) {
    dComIfGp_offEnableNextStage();
    dComIfGp_setNextStage("F_SP102", 100, 0, 10);
    mDoAud_setSceneName(dComIfGp_getNextStageName(), dComIfGp_getNextStageRoomNo(),
                        dComIfGp_getNextStageLayer());
    dComIfGs_setRestartRoomParam(0);
    mDoGph_gInf_c::setFadeColor(*(JUtility::TColor*)&g_blackColor);
    if (!fopScnM_ChangeReq(scene, fpcNm_OPENING_SCENE_e, 0, kPreviewFadeFrames)) {
        return false;
    }
    fopScnM_ReRequest(fpcNm_OPENING_SCENE_e, 0);
    mDoAud_bgmStop(kPreviewFadeFrames);
    return true;
}

camera_process_class* player_camera() {
    return dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
}

void place_camera(camera_process_class* cam, const cXyz& center, const cXyz& eye, s16 angle) {
    cam->mCamera.Reset(center, eye);
    cam->mCamera.Start();
    cam->mCamera.SetTrimSize(0);
    fopCamM_SetAngleY(cam, angle);
}

void pin_preview_camera() {
    camera_process_class* cam = player_camera();
    if (cam == nullptr) {
        return;
    }
    if (s_blendingIn && !s_blendInCaptured) {
        s_blendInCenter = cam->mCamera.Center();
        s_blendInEye = cam->mCamera.Eye();
        s_blendInAngle = fopCamM_GetAngleY(cam);
        s_blendInCaptured = true;
    }
    s_previewAngle += kPreviewOrbitSpeed;
    const f32 fx = cM_ssin(s_previewAngle);
    const f32 fz = cM_scos(s_previewAngle);
    s_previewCenter.set(0.0f, kBossChamberFloorY + kPreviewCenterHeight, 0.0f);
    s_previewEye.set(fx * kPreviewOrbitRadius, kBossChamberFloorY + kPreviewEyeHeight,
                     fz * kPreviewOrbitRadius);
    const s16 orbitAngle = static_cast<s16>(s_previewAngle + cM_deg2s(180.0f));
    s_previewPinned = true;

    if (!s_blendingIn) {
        place_camera(cam, s_previewCenter, s_previewEye, orbitAngle);
        return;
    }

    ++s_blendInFrame;
    f32 t = static_cast<f32>(s_blendInFrame) / static_cast<f32>(kPreviewBlendFrames);
    if (t > 1.0f) {
        t = 1.0f;
    }
    const f32 k = t * t * (3.0f - 2.0f * t);
    const cXyz center(s_blendInCenter.x + (s_previewCenter.x - s_blendInCenter.x) * k,
                      s_blendInCenter.y + (s_previewCenter.y - s_blendInCenter.y) * k,
                      s_blendInCenter.z + (s_previewCenter.z - s_blendInCenter.z) * k);
    const cXyz eye(s_blendInEye.x + (s_previewEye.x - s_blendInEye.x) * k,
                   s_blendInEye.y + (s_previewEye.y - s_blendInEye.y) * k,
                   s_blendInEye.z + (s_previewEye.z - s_blendInEye.z) * k);
    const s16 angle =
        static_cast<s16>(s_blendInAngle + static_cast<s16>(orbitAngle - s_blendInAngle) * k);
    place_camera(cam, center, eye, angle);
    if (s_blendInFrame >= kPreviewBlendFrames) {
        s_blendingIn = false;
    }
}

void blend_to_link_camera() {
    camera_process_class* cam = player_camera();
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (cam == nullptr || link == nullptr) {
        s_blending = false;
        return;
    }
    ++s_blendFrame;
    f32 t = static_cast<f32>(s_blendFrame) / static_cast<f32>(kPreviewBlendFrames);
    if (t > 1.0f) {
        t = 1.0f;
    }
    const f32 k = t * t * (3.0f - 2.0f * t);

    const s16 linkAngle = link->shape_angle.y;
    const f32 fx = cM_ssin(linkAngle);
    const f32 fz = cM_scos(linkAngle);
    const cXyz& p = link->current.pos;
    const cXyz targetCenter(p.x + fx * kLinkCamCenterAhead, p.y + kLinkCamCenterHeight,
                            p.z + fz * kLinkCamCenterAhead);
    const cXyz targetEye(p.x - fx * kLinkCamEyeBack, p.y + kLinkCamEyeHeight,
                         p.z - fz * kLinkCamEyeBack);

    const cXyz center(s_previewCenter.x + (targetCenter.x - s_previewCenter.x) * k,
                      s_previewCenter.y + (targetCenter.y - s_previewCenter.y) * k,
                      s_previewCenter.z + (targetCenter.z - s_previewCenter.z) * k);
    const cXyz eye(s_previewEye.x + (targetEye.x - s_previewEye.x) * k,
                   s_previewEye.y + (targetEye.y - s_previewEye.y) * k,
                   s_previewEye.z + (targetEye.z - s_previewEye.z) * k);
    const s16 startAngle = static_cast<s16>(s_previewAngle + cM_deg2s(180.0f));
    const s16 angle = static_cast<s16>(startAngle + static_cast<s16>(linkAngle - startAngle) * k);
    place_camera(cam, center, eye, angle);

    if (s_blendFrame >= kPreviewBlendFrames) {
        s_blending = false;
    }
}

void open_prelaunch() {
    s_playPressed = false;
    if (s_returnToPrelaunch != nullptr && !prelaunch_open()) {
        s_returnToPrelaunch();
    }
}

void start_preview() {
    s_previewing = true;
    s_blending = false;
    s_previewPinned = false;
    s_previewAngle = kPreviewStartAngle;
    s_blendingIn = s_blendInPending;
    s_blendInPending = false;
    s_blendInCaptured = false;
    s_blendInFrame = 0;
}

void end_preview(bool blendToLink) {
    s_previewing = false;
    dComIfGp_2dShowOn();
    if (blendToLink && s_previewPinned) {
        s_blending = true;
        s_blendFrame = 0;
    } else if (blendToLink) {
        boss_rush_arm_chamber_camera(3);
    }
}

void update_card_reattach(bool active) {
    if (active != s_lastActive) {
        s_lastActive = active;
        s_cardReattachPending = true;
    }
    if (!s_cardReattachPending) {
        return;
    }
    if (!g_mDoMemCd_control.mInitialized) {
        s_cardReattachPending = false;
        return;
    }
    if (!g_mDoMemCd_control.isCardCommNone()) {
        return;
    }
    g_mDoMemCd_control.command_attach();
    if (g_mDoMemCd_control.mCardCommand == mDoMemCd_Ctrl_c::COMM_ATTACH_e) {
        s_cardReattachPending = false;
    }
}

void update_boss_rush_game_mode() {
    if (!s_registered) {
        return;
    }
    update_card_reattach(boss_rush_game_mode_is_active());
    scene_class* scene = current_scene();
    if (scene == nullptr) {
        return;
    }
    const s16 sceneName = fpcM_GetName(scene);
    const fpc_ProcID sceneId = fpcM_GetID(scene);
    if (sceneName == fpcNm_OPENING_SCENE_e && s_inChamber && sceneId != s_chamberFromScene) {
        s_inChamber = false;
        s_entering = false;
    }
    if (s_leaving && sceneName == fpcNm_OPENING_SCENE_e) {
        s_leaving = false;
    }
    if (s_entering && sceneName == fpcNm_PLAY_SCENE_e &&
        std::strcmp(dComIfGp_getStartStageName(), kBossRushChamberStage) == 0) {
        s_entering = false;
    }

    if (s_reloadFromScene != fpc_ProcID(-1) && sceneId != s_reloadFromScene) {
        s_reloadFromScene = fpc_ProcID(-1);
    }

    const bool active = boss_rush_game_mode_is_active();
    if (s_menuReturnPending && !active) {
        s_menuReturnPending = false;
    }
    if (s_menuReturnPending && sceneName == fpcNm_PLAY_SCENE_e && !s_pendingSaveInit &&
        scene_change_allowed(scene)) {
        if (enter_chamber_scene(scene, kPreviewFadeFrames)) {
            s_menuReturnPending = false;
            s_inChamber = true;
            s_entering = true;
            s_reloadFromScene = sceneId;
            s_waitSceneId = sceneId;
            open_prelaunch();
        }
    }

    const bool menuOpen = prelaunch_open();

    if (active && !s_inChamber && sceneName == fpcNm_OPENING_SCENE_e && scene_change_allowed(scene)) {
        if (enter_chamber_scene(scene, menuOpen ? kPreviewFadeFrames : kPlayFadeFrames)) {
            s_inChamber = true;
            s_entering = true;
            s_chamberFromScene = sceneId;
            s_waitSceneId = sceneId;
        }
    } else if (!active && s_inChamber && !s_pendingSaveInit && menuOpen &&
               sceneName == fpcNm_PLAY_SCENE_e && scene_change_allowed(scene)) {
        if (leave_chamber_scene(scene)) {
            s_inChamber = false;
            s_entering = false;
            s_leaving = true;
            s_waitSceneId = sceneId;
        }
    }

    const bool wantPreview = menuOpen && sceneName == fpcNm_PLAY_SCENE_e &&
                             sceneId != s_reloadFromScene &&
                             ((active && s_inChamber) || s_leaving);
    if (wantPreview) {
        if (!s_previewing) {
            start_preview();
        }
        dComIfGp_2dShowOff();
    } else if (s_previewing) {
        end_preview(!menuOpen && active && sceneName == fpcNm_PLAY_SCENE_e);
    }
}

HookAction on_game_execute_pre(ModContext*, void*, void*, void*) {
    if (s_pendingSaveInit && screen_is_black()) {
        s_pendingSaveInit = false;
        apply_chamber_save();
    }
    return HOOK_CONTINUE;
}

void on_game_execute_post(ModContext*, void*, void*, void*) {
    update_boss_rush_game_mode();
}

void on_camera_run_post(ModContext*, void*, void*, void*) {
    if (!is_in_boss_rush_chamber()) {
        s_blending = false;
        return;
    }
    if (s_previewing) {
        pin_preview_camera();
    } else if (s_blending) {
        blend_to_link_camera();
    }
}

ModResult on_play(void*, ModError*) {
    s_playPressed = true;
    return MOD_OK;
}

}

ModResult init_boss_rush_gamemode(ModContext* mod_ctx) {
    if (svc_game_mode == nullptr || !boss_rush_save_preset_available()) {
        return MOD_ERROR;
    }
    s_modCtx = mod_ctx;

    if (svc_hook != nullptr) {
        void* addr = nullptr;
        if (svc_hook->resolve(mod_ctx, "dusk::ui::is_prelaunch_open", &addr, nullptr) == MOD_OK) {
            s_isPrelaunchOpen = reinterpret_cast<IsPrelaunchOpenFn>(addr);
        }
        addr = nullptr;
        if (svc_hook->resolve(mod_ctx, "dusk::ui::return_to_prelaunch", &addr, nullptr) == MOD_OK) {
            s_returnToPrelaunch = reinterpret_cast<ReturnToPrelaunchFn>(addr);
        }
        mods::hook::add_pre<BossRushGameModeExecuteHook>(svc_hook, on_game_execute_pre);
        mods::hook::add_post<BossRushGameModeExecuteHook>(svc_hook, on_game_execute_post);
        mods::hook::add_post<BossRushPreviewCameraRunHook>(svc_hook, on_camera_run_post);
    }

    const char* fullName =
        is_mod_enabled(kDawnlightModId) ? "Bossrush [TE]" : "Bossrush";
    const GameModeDesc desc = {
        .struct_size = sizeof(GameModeDesc),
        .game_mode_id = kGameModeId,
        .full_name = fullName,
        .save_name = "twilit-bossrush",
        .user_data = nullptr,
        .on_activated = nullptr,
        .on_deactivated = nullptr,
        .on_play = on_play,
        .on_save_loaded = nullptr,
        .on_new_save = nullptr,
        .on_new_save_select = nullptr,
        .on_game_reset = nullptr,
        .on_tick = nullptr,
    };
    const ModResult result = svc_game_mode->register_game_mode(mod_ctx, &desc);
    s_registered = result == MOD_OK;
    return result;
}

bool boss_rush_game_mode_is_active() {
    if (!s_registered || svc_game_mode == nullptr || s_modCtx == nullptr) {
        return false;
    }
    bool active = false;
    return svc_game_mode->is_active(s_modCtx, kGameModeId, &active) == MOD_OK && active;
}

void boss_rush_game_mode_return_to_menu() {
    if (boss_rush_game_mode_is_active()) {
        s_menuReturnPending = true;
    }
}

void boss_rush_game_mode_return_to_menu_smooth() {
    if (!boss_rush_game_mode_is_active() || !s_inChamber || s_returnToPrelaunch == nullptr) {
        boss_rush_game_mode_return_to_menu();
        return;
    }
    s_blendInPending = true;
    open_prelaunch();
}

bool boss_rush_game_mode_entering() {
    return s_entering || s_leaving || s_pendingSaveInit;
}

void shutdown_boss_rush_gamemode() {
    if (s_previewing) {
        dComIfGp_2dShowOn();
    }
    s_registered = false;
    s_previewing = false;
    s_inChamber = false;
    s_entering = false;
    s_leaving = false;
    s_pendingSaveInit = false;
    s_isPrelaunchOpen = nullptr;
    s_returnToPrelaunch = nullptr;
    s_menuReturnPending = false;
    s_reloadFromScene = fpc_ProcID(-1);
    s_modCtx = nullptr;
}
