#include "faster_transitions.hpp"

#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_window.h"
#include "d/d_ovlp_fade.h"
#include "d/d_ovlp_fade2.h"
#include "d/d_ovlp_fade3.h"
#include "d/d_s_play.h"
#include "angle_utils.h"
#include "f_op/f_op_overlap_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_graphic.h"
#include "Z2AudioLib/Z2SceneMgr.h"
#include "JSystem/JFramework/JFWDisplay.h"
#include "JSystem/JUtility/JUTFader.h"
#include "SSystem/SComponent/c_lib.h"

bool g_configFasterTransitions = true;

static const HookService* s_hookSvc = nullptr;
static bool s_hooksInstalled = false;

static int64_t s_mapFrames = 10;
static int64_t s_specialFrames = 10;
static int64_t s_doorFrames = 10;
static int64_t s_wipeFrames = 0;
static int64_t s_pauseMenuFrames = 8;
static int64_t s_whiteHoldFrames = 6;

static inline bool is_in_game() {
    return fpcM_SearchByName(fpcNm_PLAY_SCENE_e) != nullptr;
}

static bool s_transitionActive = false;
static int s_fd3WhiteWait = 0;

static inline bool in_transition() {
    return g_configFasterTransitions && (is_in_game() || s_transitionActive);
}

static inline bool can_start_transition() {
    if (g_configFasterTransitions && is_in_game()) {
        s_transitionActive = true;
        return true;
    }
    return false;
}

class overlap1_class : public overlap_task_class {
public:
 int field_0xcc;
 int field_0xd0;
 int field_0xd4;
};

DEFINE_HOOK(&dOvlpFd3_c::execFirstSnap, OvlpFd3ExecFirstSnap);
DEFINE_HOOK(&dOvlpFd3_c::execFadeOut, OvlpFd3ExecFadeOut);
DEFINE_HOOK(&dOvlpFd3_c::execNextSnap, OvlpFd3ExecNextSnap);
DEFINE_HOOK(&dOvlpFd3_c::execFadeIn, OvlpFd3ExecFadeIn);

DEFINE_HOOK(&dOvlpFd2_c::execFirstSnap, OvlpFd2ExecFirstSnap);
DEFINE_HOOK(&dOvlpFd2_c::execFadeOut, OvlpFd2ExecFadeOut);
DEFINE_HOOK(&dOvlpFd2_c::execNextSnap, OvlpFd2ExecNextSnap);
DEFINE_HOOK(&dOvlpFd2_c::execFadeIn, OvlpFd2ExecFadeIn);

DEFINE_HOOK(&Z2SceneMgr::setFadeOutStart, Z2SceneMgrSetFadeOutStart);
DEFINE_HOOK(&Z2SceneMgr::load1stDynamicWave, Z2SceneMgrLoad1stDynamicWave);

DEFINE_HOOK_SYMBOL("src/d/d_ovlp_fade.cpp#dOvlpFd_FadeIn", int(overlap1_class*), OvlpFdFadeIn);
DEFINE_HOOK_SYMBOL("src/d/d_ovlp_fade.cpp#dOvlpFd_FadeOut", int(overlap1_class*), OvlpFdFadeOut);

DEFINE_HOOK(&dMw_c::dMw_fade_out, MwFadeOut);
DEFINE_HOOK(&dMw_c::dMw_fade_in, MwFadeIn);

DEFINE_HOOK(&JUTFader::startFadeOut, JUTFaderStartFadeOut);
DEFINE_HOOK(&JUTFader::startFadeIn, JUTFaderStartFadeIn);

static void on_exec_first_snap3_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd3_c* self = mods::arg<dOvlpFd3_c*>(args, 0);
    if (!can_start_transition()) {
        OvlpFd3ExecFirstSnap::g_orig(self);
        return;
    }

    if (self->field_0x11f > (u8)s_wipeFrames) {
        self->field_0x11f = (u8)s_wipeFrames;
        self->mTimer = (s_wipeFrames > 0 ? 2 : 0);
    }

    if (cLib_calcTimer(&self->field_0x11f) == 0 && self->field_0x11c != 0) {
        if (cLib_calcTimer(&self->mTimer) == 0) {
            self->setExecute(&dOvlpFd3_c::execFadeOut);
            fopOvlpM_Done(self);
            self->mTimer = 0xFF;
        }
        dComIfGp_setWindowNum(0);
    }
}

static void on_exec_fade_out3_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd3_c* self = mods::arg<dOvlpFd3_c*>(args, 0);
    if (!in_transition()) {
        OvlpFd3ExecFadeOut::g_orig(self);
        return;
    }

    dComIfGp_setWindowNum(0);

    if (self->mTimer == 0) {
        JUTFader* fader = JFWDisplay::getManager()->getFader();
        const bool fullWhite = fader == nullptr || fader->getStatus() == JUTFader::None;
        if (fullWhite || s_fd3WhiteWait >= 45) {
            s_fd3WhiteWait = 0;
            if (fopOvlpM_IsOutReq(self)) {
                fopOvlpM_SceneIsStart();
                self->setExecute(&dOvlpFd3_c::execNextSnap);
                self->field_0x110 = -0x4000;
                self->mTimer = 1;
            }
        } else {
            s_fd3WhiteWait++;
        }
    }

    if (self->mTimer < 0) {
        if (++self->mTimer == 0) {
            JUTFader* fader = JFWDisplay::getManager()->getFader();
            const int status = fader != nullptr ? fader->getStatus() : -1;
            const bool needsRestart =
                status == JUTFader::Wait || status == JUTFader::FadeIn ||
                (status == JUTFader::FadeOut && fader->mDuration > (int)s_mapFrames);
            if (needsRestart) {
                fader->setStatus(JUTFader::Wait, 0);
                mDoGph_gInf_c::startFadeOut((int)s_mapFrames);
            }
            int64_t waitFrames = s_mapFrames + s_whiteHoldFrames;
            if (waitFrames > 120) {
                waitFrames = 120;
            }
            self->mTimer = (s8)waitFrames;
            mDoAud_setFadeOutStart(0);
            s_fd3WhiteWait = 0;
        }
    } else {
        cLib_calcTimer(&self->mTimer);
    }
}

static void on_exec_next_snap3_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd3_c* self = mods::arg<dOvlpFd3_c*>(args, 0);
    if (!in_transition()) {
        OvlpFd3ExecNextSnap::g_orig(self);
        return;
    }

    if (cLib_calcTimer(&self->mTimer) == 0) {
        if (!JFWDisplay::getManager()->getFader()->startFadeIn((int)s_mapFrames)) {
            mDoAud_setFadeInStart(0);
            self->field_0x110 += self->field_0x112;

            dComIfGp_setWindowNum(1);
            self->setExecute(&dOvlpFd3_c::execFadeIn);
        }
    }
}

static void on_exec_first_snap2_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd2_c* self = mods::arg<dOvlpFd2_c*>(args, 0);
    if (!can_start_transition()) {
        OvlpFd2ExecFirstSnap::g_orig(self);
        return;
    }

    if (self->field_0x11c != 0) {
        if (cLib_calcTimer<s8>(&self->mTimer) == 0) {
            self->setExecute(&dOvlpFd2_c::execFadeOut);
            fopOvlpM_Done(self);
            self->mTimer = -1;
        }
        dComIfGp_setWindowNum(0);
    }
}

static void on_exec_fade_out2_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd2_c* self = mods::arg<dOvlpFd2_c*>(args, 0);
    if (!in_transition()) {
        OvlpFd2ExecFadeOut::g_orig(self);
        return;
    }

    dComIfGp_setWindowNum(0);
    cLib_chaseAngleS(&self->field_0x112, 2000, 100);

    s16 temp_r5 = ((((self->field_0x110 + 0x4000) & 0x8000) | 0x4000) - self->field_0x112);
    self->field_0x110 += self->field_0x112;

    if (self->field_0x112 * (s16)(temp_r5 - self->field_0x110) < 0) {
        if (self->mTimer == 0) {
            if (fopOvlpM_IsOutReq(self)) {
                fopOvlpM_SceneIsStart();
                self->setExecute(&dOvlpFd2_c::execNextSnap);
                self->field_0x110 = -0x4000;
                self->mTimer = (s8)s_specialFrames;
            }
        }
    }

    if (self->mTimer < 0) {
        if (++self->mTimer == 0) {
            mDoGph_gInf_c::startFadeOut((int)s_specialFrames);
            self->mTimer = (s8)s_specialFrames;
        }
    } else {
        cLib_calcTimer<s8>(&self->mTimer);
    }

    ANGLE_ADD(self->field_0x114, TREG_S(0) + 0x800);
    cLib_addCalc2(&self->field_0x118, TREG_F(1) + 1.0f, 1.0f, TREG_F(2) + 0.05f);
}

static void on_exec_next_snap2_replace(ModContext*, void* args, void*, void*) {
    dOvlpFd2_c* self = mods::arg<dOvlpFd2_c*>(args, 0);
    if (!in_transition()) {
        OvlpFd2ExecNextSnap::g_orig(self);
        return;
    }

    if (cLib_calcTimer<s8>(&self->mTimer) == 0) {
        if (!JFWDisplay::getManager()->getFader()->startFadeIn((int)s_specialFrames)) {
            self->field_0x110 += self->field_0x112;
            self->field_0x11c = 0;

            dComIfGp_setWindowNum(1);
            dComIfGp_2dShowOff();
            self->setExecute(&dOvlpFd2_c::execFadeIn);
        }
    }
}

static void on_fd3_fade_in_post(ModContext*, void*, void*, void*) {
    s_transitionActive = false;
}

static void on_fd2_fade_in_post(ModContext*, void*, void*, void*) {
    s_transitionActive = false;
}

static void on_z2_fade_out_start_post(ModContext*, void* args, void*, void*) {
    if (in_transition()) {
        Z2SceneMgr* self = mods::arg<Z2SceneMgr*>(args, 0);
        self->load1stWait = 1;
    }
}

static void on_z2_load1st_post(ModContext*, void* args, void*, void*) {
    if (in_transition()) {
        Z2SceneMgr* self = mods::arg<Z2SceneMgr*>(args, 0);
        if (self->load1stWait == -15) {
            self->load1stWait = -1;
        }
    }
}

static void on_ovlp_fd_fade_in_post(ModContext*, void* args, void*, void*) {
    if (!in_transition()) return;
    overlap1_class* self = mods::arg<overlap1_class*>(args, 0);
    const int profName = fpcM_GetProfName(self);
    if (profName == fpcNm_OVERLAP10_e || profName == fpcNm_OVERLAP11_e) return;
    if (self->field_0xd0 > (int)s_doorFrames) {
        self->field_0xd0 = (int)s_doorFrames;
    }
    if (self->field_0xd4 > (int)s_doorFrames) {
        self->field_0xd4 = (int)s_doorFrames;
    }
}

static void on_ovlp_fd_fade_out_post(ModContext*, void* args, void*, void*) {
    if (!in_transition()) return;
    overlap1_class* self = mods::arg<overlap1_class*>(args, 0);
    const int profName = fpcM_GetProfName(self);
    if (profName == fpcNm_OVERLAP10_e || profName == fpcNm_OVERLAP11_e) return;
    if (self->field_0xcc > (int)s_doorFrames) {
        self->field_0xcc = (int)s_doorFrames;
    }
}

static HookAction on_fader_fade_out_pre(ModContext*, void* args, void*, void*) {
    if (!in_transition()) return HOOK_CONTINUE;
    int& duration = mods::arg_ref<int>(args, 1);
    const int budget =
        s_doorFrames > s_pauseMenuFrames ? (int)s_doorFrames : (int)s_pauseMenuFrames;
    if (duration > budget) {
        duration = budget;
    }
    return HOOK_CONTINUE;
}

static HookAction on_fader_fade_in_pre(ModContext*, void* args, void*, void*) {
    if (!in_transition()) return HOOK_CONTINUE;
    int& duration = mods::arg_ref<int>(args, 1);
    const int budget =
        s_doorFrames > s_pauseMenuFrames ? (int)s_doorFrames : (int)s_pauseMenuFrames;
    if (duration > budget) {
        duration = budget;
    }
    return HOOK_CONTINUE;
}

static void on_mw_fade_out_replace(ModContext*, void*, void*, void*) {
    if (!g_configFasterTransitions || !is_in_game()) {
        MwFadeOut::g_orig();
        return;
    }
    mDoGph_gInf_c::startFadeOut((int)s_pauseMenuFrames);
    mDoGph_gInf_c::setFadeColor(static_cast<JUtility::TColor&>(g_blackColor));
}

static void on_mw_fade_in_replace(ModContext*, void*, void*, void*) {
    if (!g_configFasterTransitions || !is_in_game()) {
        MwFadeIn::g_orig();
        return;
    }
    mDoGph_gInf_c::startFadeIn((int)s_pauseMenuFrames);
    mDoGph_gInf_c::setFadeColor(static_cast<JUtility::TColor&>(g_blackColor));
}

template <class Entry>
static bool install_replace(HookReplaceFn cb) {
    return mods::hook::replace<Entry>(s_hookSvc, cb) == MOD_OK;
}

template <class Entry>
static bool install_pre(HookPreFn cb) {
    return mods::hook::add_pre<Entry>(s_hookSvc, cb) == MOD_OK;
}

template <class Entry>
static bool install_post(HookPostFn cb) {
    return mods::hook::add_post<Entry>(s_hookSvc, cb) == MOD_OK;
}

template <class Entry>
static void uninstall_hook() {
    mods::hook::uninstall<Entry>(s_hookSvc);
}

static bool install_all_hooks() {
    if (s_hookSvc == nullptr) return false;

    const bool ok =
        install_replace<OvlpFd3ExecFirstSnap>(on_exec_first_snap3_replace) &&
        install_replace<OvlpFd3ExecFadeOut>(on_exec_fade_out3_replace) &&
        install_replace<OvlpFd3ExecNextSnap>(on_exec_next_snap3_replace) &&
        install_post<OvlpFd3ExecFadeIn>(on_fd3_fade_in_post) &&
        install_replace<OvlpFd2ExecFirstSnap>(on_exec_first_snap2_replace) &&
        install_replace<OvlpFd2ExecFadeOut>(on_exec_fade_out2_replace) &&
        install_replace<OvlpFd2ExecNextSnap>(on_exec_next_snap2_replace) &&
        install_post<OvlpFd2ExecFadeIn>(on_fd2_fade_in_post) &&
        install_post<Z2SceneMgrSetFadeOutStart>(on_z2_fade_out_start_post) &&
        install_post<Z2SceneMgrLoad1stDynamicWave>(on_z2_load1st_post) &&
        install_post<OvlpFdFadeIn>(on_ovlp_fd_fade_in_post) &&
        install_post<OvlpFdFadeOut>(on_ovlp_fd_fade_out_post) &&
        install_replace<MwFadeOut>(on_mw_fade_out_replace) &&
        install_replace<MwFadeIn>(on_mw_fade_in_replace) &&
        install_pre<JUTFaderStartFadeOut>(on_fader_fade_out_pre) &&
        install_pre<JUTFaderStartFadeIn>(on_fader_fade_in_pre);

    return ok;
}

static void uninstall_all_hooks() {
    uninstall_hook<OvlpFd3ExecFirstSnap>();
    uninstall_hook<OvlpFd3ExecFadeOut>();
    uninstall_hook<OvlpFd3ExecNextSnap>();
    uninstall_hook<OvlpFd3ExecFadeIn>();
    uninstall_hook<OvlpFd2ExecFirstSnap>();
    uninstall_hook<OvlpFd2ExecFadeOut>();
    uninstall_hook<OvlpFd2ExecNextSnap>();
    uninstall_hook<OvlpFd2ExecFadeIn>();
    uninstall_hook<Z2SceneMgrSetFadeOutStart>();
    uninstall_hook<Z2SceneMgrLoad1stDynamicWave>();
    uninstall_hook<OvlpFdFadeIn>();
    uninstall_hook<OvlpFdFadeOut>();
    uninstall_hook<MwFadeOut>();
    uninstall_hook<MwFadeIn>();
    uninstall_hook<JUTFaderStartFadeOut>();
    uninstall_hook<JUTFaderStartFadeIn>();
}

void faster_transitions_apply_mode() {
    if (g_configFasterTransitions) {
        if (!s_hooksInstalled) {
            s_hooksInstalled = install_all_hooks();
        }
    } else {
        s_transitionActive = false;
        s_fd3WhiteWait = 0;
        if (s_hooksInstalled) {
            uninstall_all_hooks();
            s_hooksInstalled = false;
        }
    }
}

ModResult init_faster_transitions(const HookService* hook_svc, ModError*) {
    if (!hook_svc) return MOD_ERROR;
    s_hookSvc = hook_svc;
    faster_transitions_apply_mode();
    return MOD_OK;
}

void shutdown_faster_transitions() {
    if (s_hooksInstalled) {
        uninstall_all_hooks();
        s_hooksInstalled = false;
    }
    s_hookSvc = nullptr;
}
