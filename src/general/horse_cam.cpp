#include "horse_cam.hpp"

#include "../util.hpp"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"

bool g_configHorseCamNoRecenter = false;

void update_horse_cam() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    camera_process_class* cam = dComIfGp_getCamera(0);
    if (link == nullptr || cam == nullptr) return;

    dCamParam_c& prm = cam->mCamera.mCamParam;
    const bool riding = link->checkHorseRide() != 0;
    const bool active = g_configHorseCamNoRecenter && riding &&
                        prm.Algorythmn() == 8 && prm.mCurrentStyle != nullptr;

    const int style = cam->mCamera.mCamStyle;
    static int s_patchedStyle = -1;
    static f32 s_savedRate[3] = {0.0f, 0.0f, 0.0f};

    if (!active) {
        if (s_patchedStyle >= 0 && prm.mStyleID == s_patchedStyle) {
            prm.SetVal(s_patchedStyle, 22, s_savedRate[0]);
            prm.SetVal(s_patchedStyle, 24, s_savedRate[1]);
            prm.SetVal(s_patchedStyle, 27, s_savedRate[2]);
        }
        s_patchedStyle = -1;
        return;
    }

    const f32 stickX = cam->mCamera.mPadInfo.mCStick.mLastPosX;
    const f32 stickY = cam->mCamera.mPadInfo.mCStick.mLastPosY;
    const bool stickActive =
        (stickX > 0.15f || stickX < -0.15f || stickY > 0.15f || stickY < -0.15f);
    if (stickActive) {
        if (s_patchedStyle >= 0 && prm.mStyleID == s_patchedStyle) {
            prm.SetVal(s_patchedStyle, 22, s_savedRate[0]);
            prm.SetVal(s_patchedStyle, 24, s_savedRate[1]);
            prm.SetVal(s_patchedStyle, 27, s_savedRate[2]);
        }
        s_patchedStyle = -1;
        return;
    }

    if (s_patchedStyle != style) {
        s_savedRate[0] = prm.Val(style, 22);
        s_savedRate[1] = prm.Val(style, 24);
        s_savedRate[2] = prm.Val(style, 27);
        s_patchedStyle = style;
    }
    prm.SetVal(style, 22, 0.0f);
    prm.SetVal(style, 24, 0.0f);
    prm.SetVal(style, 27, 0.0f);
}
