#include "quick_access_bottles.hpp"
#include "quick_access_internal.hpp"

#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTGamePad.h"
#include "JSystem/JUtility/JUTFont.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_s_play.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#define private public
#define protected public
#include "d/d_meter2_draw.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#undef protected
#undef private
#include "d/d_select_cursor.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "../z_button/z_common.hpp"
#include "../controls/controls.hpp"
#include "../compat/twilight_hd.hpp"
#include "../boss_rush/boss_rush.hpp"
#include "mods/svc/save.h"

#include <dolphin/gx.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>

extern const LogService* svc_log;
extern ModContext* mod_ctx;

static void qb_log(const char* fmt, ...) {
    if (svc_log == nullptr || mod_ctx == nullptr) return;
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    svc_log->info(mod_ctx, msg);
}

static ModContext* s_bottleModCtx = nullptr;
static const SaveService* s_bottleSaveSvc = nullptr;
static SaveObserverHandle s_bottleSaveObserver = 0;
static const char* kBottleBlobName = "quickAccessBottlesV1";

struct BottleBlob {
    u8 assignedSlot;
    u8 pad[7];
};
static_assert(sizeof(BottleBlob) == 8, "blob size");

static u8 s_assignedSlot = 0xFF;

static u8 bottle_item(int idx) {
    if (idx < 0 || idx >= 4) {
        return dItemNo_NONE_e;
    }
    return dComIfGs_getItem(SLOT_11 + idx, false);
}

static bool bottle_owned(int idx) {
    return bottle_item(idx) != dItemNo_NONE_e;
}

static void bottles_reset_assignment() {
    s_assignedSlot = 0xFF;
}

static void bottles_store() {
    if (s_bottleSaveSvc == nullptr || s_bottleModCtx == nullptr) {
        return;
    }
    BottleBlob blob{};
    blob.assignedSlot = s_assignedSlot;
    s_bottleSaveSvc->set_blob(s_bottleModCtx, kBottleBlobName, &blob, sizeof(blob));
}

static void on_bottles_new_save(ModContext*, uint32_t, void*) {
    bottles_reset_assignment();
}

static void on_bottles_save_loaded(ModContext* ctx, uint32_t, void*) {
    bottles_reset_assignment();
    if (s_bottleSaveSvc == nullptr) {
        return;
    }

    BottleBlob blob{};
    size_t size = sizeof(blob);
    if (s_bottleSaveSvc->get_blob(ctx, kBottleBlobName, &blob, &size) == MOD_OK &&
        size == sizeof(blob) && blob.assignedSlot < 4) {
        s_assignedSlot = blob.assignedSlot;
    }
}

static void bottles_play_error_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SYS_ERROR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static void bottles_play_ok_se() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

static const int QB_ITEM_PROC_KANDELAAR_POUR = 8;
static const int QB_ITEM_PROC_COMMON_CHANGE_ITEM = 12;
static const int QB_ITEM_PROC_BOTTLE_SWING = 13;

static u8 s_pendingSlot = 0xFF;
static u8 s_pendingItem = 0;
static u16 s_pendingProc = 0;
static int s_pendingFrames = 0;
static const int QB_PENDING_MAX_FRAMES = 900;

static u8 s_preSelectIndex = 0xFF;
static u8 s_preSelectPlay = 0;

static bool bottles_lantern_equipped() {
    for (int i = 0; i < 3; i++) {
        const u8 sel = dComIfGp_getSelectItem(i);
        if (sel == dItemNo_KANTERA_e || sel == dItemNo_KANTERA2_e) {
            return true;
        }
        if (i == 2) {
            const u8 idx = dComIfGs_getSelectItemIndex(2);
            if (idx < 24) {
                const u8 item = dComIfGs_getItem(idx, false);
                if (item == dItemNo_KANTERA_e || item == dItemNo_KANTERA2_e) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void bottles_finish_pending() {
    const u8 result = (s_pendingItem == dItemNo_MILK_BOTTLE_e)
                          ? static_cast<u8>(dItemNo_HALF_MILK_BOTTLE_e)
                          : static_cast<u8>(dItemNo_EMPTY_BOTTLE_e);
    if (bottle_item(s_pendingSlot) == s_pendingItem) {
        dComIfGs_setBottleItemIn(s_pendingItem, result);
    }

    if (dComIfGs_getSelectItemIndex(SELECT_ITEM_B) != s_preSelectIndex) {
        dComIfGs_setSelectItemIndex(SELECT_ITEM_B, s_preSelectIndex);
    }
    if (dComIfGp_getSelectItem(SELECT_ITEM_B) != s_preSelectPlay) {
        g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_B, s_preSelectPlay);
    }

    if (dComIfGs_getSelectItemIndex(SELECT_ITEM_B) == SLOT_11 + s_pendingSlot) {
        dComIfGs_setSelectItemIndex(SELECT_ITEM_B, 0xFF);
        g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_B, dItemNo_NONE_e);
    }

    qb_log("[qb] finish_pending: slot=%d item=0x%02X result=0x%02X", (int)s_pendingSlot,
           (int)s_pendingItem, (int)result);

    s_pendingSlot = 0xFF;
}

static void bottles_use_bottle(int slotIdx) {
    QaSelectSlotScope bottleScope(SELECT_ITEM_B);
    daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
    if (link == nullptr) {
        return;
    }

    const u8 item = bottle_item(slotIdx);
    if (item == dItemNo_NONE_e) {
        bottles_play_error_se();
        return;
    }

    const bool isOil = link->checkOilBottleItem(item);
    if (isOil && !bottles_lantern_equipped()) {
        bottles_play_error_se();
        return;
    }

    const u16 procBefore = link->mProcID;
    const u8 equipBefore = link->mEquipItem;
    const u8 playBefore = dComIfGp_getSelectItem(SELECT_ITEM_B);
    s_preSelectIndex = dComIfGs_getSelectItemIndex(SELECT_ITEM_B);
    s_preSelectPlay = playBefore;

    qb_log("[qb] use_bottle: slot=%d item=0x%02X isOil=%d zEngine=%d "
           "before: procID=%d equip=0x%02X selectItemId=%d zIdx=%d resolved2=0x%02X",
           slotIdx, (int)item, (int)isOil, (int)g_configCustomZButtonEnabled,
           (int)procBefore, (int)equipBefore, (int)link->mSelectItemId,
           (int)g_zInventorySlot, (int)resolved_select_item(2));

    g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_B, item);
    dComIfGs_setSelectItemIndex(SELECT_ITEM_B, SLOT_11 + slotIdx);

    int proc = link->checkNewItemChange(SELECT_ITEM_B);
    if (isOil && proc == QB_ITEM_PROC_COMMON_CHANGE_ITEM) {
        proc = QB_ITEM_PROC_KANDELAAR_POUR;
    }

    if (item == dItemNo_EMPTY_BOTTLE_e && link->mEquipItem != dItemNo_KANTERA_e) {
        if (proc == QB_ITEM_PROC_COMMON_CHANGE_ITEM && link->mEquipItem == dItemNo_NONE_e) {
            link->setBottleModel(dItemNo_EMPTY_BOTTLE_e);
            proc = QB_ITEM_PROC_BOTTLE_SWING;
        } else if (proc == 0 && link->mEquipItem == dItemNo_EMPTY_BOTTLE_e) {
            proc = QB_ITEM_PROC_BOTTLE_SWING;
        }
    }

    qb_log("[qb] use_bottle: checkNewItemChange proc=%d", proc);

    if (proc == 0) {
        qb_log("[qb] use_bottle: proc==0, bailing (error sound)");
        bottles_play_error_se();
        g_dComIfG_gameInfo.play.setSelectItem(SELECT_ITEM_B, playBefore);
        dComIfGs_setSelectItemIndex(SELECT_ITEM_B, s_preSelectIndex);
        return;
    }

    link->changeItemTriggerKeepProc(SELECT_ITEM_B, proc);

    qb_log("[qb] use_bottle: after trigger: procID=%d (was %d) equip=0x%02X (was 0x%02X) "
           "selectItemId=%d zIdx=%d resolved2=0x%02X",
           (int)link->mProcID, (int)procBefore, (int)link->mEquipItem, (int)equipBefore,
           (int)link->mSelectItemId, (int)g_zInventorySlot, (int)resolved_select_item(2));

    s_pendingSlot = static_cast<u8>(slotIdx);
    s_pendingItem = item;
    s_pendingProc = link->mProcID;
    s_pendingFrames = 0;
    bottles_play_ok_se();
}

DEFINE_HOOK(&mDoCPd_c::read, PadReadBottlesHook);

static void on_pad_read_bottles_post(ModContext*, void*, void*, void*) {
    if (s_pendingSlot != 0xFF) {
        daAlink_c* link = static_cast<daAlink_c*>(daPy_getLinkPlayerActorClass());
        if (link == nullptr || link->mProcID != s_pendingProc ||
            ++s_pendingFrames >= QB_PENDING_MAX_FRAMES) {
            bottles_finish_pending();
        }
    }
}

ModResult init_quick_access_bottles(const HookService* hook_svc, const SaveService* save_svc,
                                    ModContext* mod_ctx, ModError*) {
    if (hook_svc) {
        const HookOptions beforeTwilightHd = twilight_hd_hook_order(kTwilightHdRunBefore);
        mods::hook::add_post<PadReadBottlesHook>(hook_svc, on_pad_read_bottles_post, &beforeTwilightHd);
    }

    s_bottleSaveSvc = save_svc;
    s_bottleModCtx = mod_ctx;
    if (save_svc != nullptr && mod_ctx != nullptr) {
        save_svc->observe_saves(mod_ctx, on_bottles_new_save, on_bottles_save_loaded, nullptr,
                                nullptr, &s_bottleSaveObserver);
        on_bottles_save_loaded(mod_ctx, 0, nullptr);
    }
    return MOD_OK;
}

void shutdown_quick_access_bottles() {
    s_pendingSlot = 0xFF;
}

u8 qa_bottle_item(int idx) {
    return bottle_item(idx);
}

bool qa_bottle_owned(int idx) {
    return bottle_owned(idx);
}

int qa_bottle_assigned_slot() {
    return s_assignedSlot < 4 ? static_cast<int>(s_assignedSlot) : -1;
}

void qa_bottle_use(int idx) {
    if (!bottle_owned(idx)) {
        bottles_play_error_se();
        return;
    }
    if (s_assignedSlot != static_cast<u8>(idx)) {
        s_assignedSlot = static_cast<u8>(idx);
        bottles_store();
    }
    bottles_use_bottle(idx);
}
