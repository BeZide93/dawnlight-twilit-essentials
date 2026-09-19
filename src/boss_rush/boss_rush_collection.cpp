#include "boss_rush_collection.hpp"
#include "boss_rush.hpp"
#include "d/d_menu_collect.h"
#include "d/d_menu_window.h"
#include "d/d_msg_out_font.h"
#include "d/d_msg_string_base.h"
#include "d/d_pane_class.h"
#include "helpers/string.hpp"
#include "m_Do/m_Do_audio.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "mods/svc/hook.hpp"

namespace {

static bool is_boss_rush_save_locked() {
    return is_boss_rush_active() || is_in_boss_rush_chamber() || boss_rush_is_fighting_here();
}

static void apply_boss_rush_save_lock(dMenu_Collect2D_c* collect2D) {
    if (!collect2D || !collect2D->mpScreen) return;

    J2DPane* saveBtn = collect2D->mpScreen->search(MULTI_CHAR('save_n'));
    const bool locked = is_boss_rush_save_locked();
    const u8 alpha = locked ? 110 : 255;

    if (saveBtn) saveBtn->setAlpha(alpha);
    if (collect2D->mpSelPm[0][5]) collect2D->mpSelPm[0][5]->setAlpha(alpha);

    static const u64 saveTextPanes[] = {
        MULTI_CHAR('f_sav_0'), MULTI_CHAR('f_sav_1'), MULTI_CHAR('f_sav_2'),
        MULTI_CHAR('sav_0'),   MULTI_CHAR('sav_1'),   MULTI_CHAR('sav_2'),
    };
    for (u64 tag : saveTextPanes) {
        if (J2DPane* p = collect2D->mpScreen->search(tag)) {
            p->show();
            p->setAlpha(alpha);
        }
    }
}

DEFINE_HOOK(&dMenu_Collect2D_c::wait_proc, BossRushCollectWaitProcHook);
static HookAction on_wait_proc_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D || !collect2D->mpScreen) return HOOK_CONTINUE;

    apply_boss_rush_save_lock(collect2D);

    if (dMw_A_TRIGGER()) {
        u8 curX = collect2D->mCursorX;
        u8 curY = collect2D->mCursorY;
        if (curY == 5 && curX == 0 && is_boss_rush_save_locked()) {
            mDoAud_seStartMenu(Z2SE_SYS_ERROR);
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

static void on_wait_proc_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;
    if (curX == 0 && curY == 5 && is_boss_rush_save_locked()) {
        collect2D->setAButtonString(0);
    }
}

DEFINE_HOOK(&dMenu_Collect2D_c::pointerActivateCurrent, BossRushCollectPointerActivateHook);
static HookAction on_pointer_activate_current_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (!collect2D) return HOOK_CONTINUE;

    u8 curX = collect2D->mCursorX;
    u8 curY = collect2D->mCursorY;
    if (curY == 5 && curX == 0 && is_boss_rush_save_locked()) {
        mDoAud_seStartMenu(Z2SE_SYS_ERROR);
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&dMenu_Collect2D_c::save_open_init, BossRushCollectSaveOpenInitHook);
static HookAction on_save_open_init_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    dMenu_Collect2D_c* collect2D = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (is_boss_rush_save_locked()) {
        if (collect2D) {
            collect2D->mSubWindowOpenCheck = 0;
        }
        mDoAud_seStartMenu(Z2SE_SYS_ERROR);
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&dMsgStringBase_c::getStringLocal, BossRushMsgStringGetStringLocalHook);
static HookAction on_get_string_local_pre(ModContext*, void* args, void* ret, void*) {
    if (!args) return HOOK_CONTINUE;
    u32 msgID = mods::arg<u32>(args, 1);
    if (msgID == 0x4C5 && is_boss_rush_save_locked()) {
        dMsgStringBase_c* msgStr = mods::arg<dMsgStringBase_c*>(args, 0);
        J2DTextBox* boxes[2] = { mods::arg<J2DTextBox*>(args, 2), mods::arg<J2DTextBox*>(args, 3) };
        COutFont_c* outFont = mods::arg<COutFont_c*>(args, 5);

        for (J2DTextBox* tb : boxes) {
            if (!tb) continue;
            if (msgStr) msgStr->resetStringLocal(tb);
            if (outFont) outFont->reset(tb);
            if (tb->getStringPtr()) {
                SAFE_STRCPY(tb->getStringPtr(), "Not available in Boss Rush.");
            }
        }
        if (ret) *(f32*)ret = 0.0f;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

}

void init_boss_rush_collection(const HookService* hook_svc, const LogService*, ModContext*) {
    if (hook_svc) {
        mods::hook::add_pre<BossRushCollectWaitProcHook>(hook_svc, on_wait_proc_pre);
        mods::hook::add_post<BossRushCollectWaitProcHook>(hook_svc, on_wait_proc_post);
        mods::hook::add_pre<BossRushCollectPointerActivateHook>(hook_svc, on_pointer_activate_current_pre);
        mods::hook::add_pre<BossRushCollectSaveOpenInitHook>(hook_svc, on_save_open_init_pre);
        mods::hook::add_pre<BossRushMsgStringGetStringLocalHook>(hook_svc, on_get_string_local_pre);
    }
}
