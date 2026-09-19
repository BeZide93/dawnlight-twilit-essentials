#include "general/general.hpp"
#include "general/always.hpp"
#include "general/damage_vignette.hpp"
#include "general/hud_auto_fade.hpp"
#include "general/oxygen_vignette.hpp"
#include "general/sprint_fov_kick.hpp"
#include "general/horse_cam.hpp"

#include "general/human_warp.hpp"
#include "general/faster_midna_cancel.hpp"
#include "general/faster_transitions.hpp"
#include "general/midna_select_freeze_guard.hpp"
#include "hp_bars/hp_bars.hpp"
#include "boss_bar/boss_bar.hpp"
#include "boss_rush/boss_rush.hpp"
#include "boss_rush/boss_rush_models.hpp"
#include "boss_rush/boss_rush_equipment.hpp"
#include "boss_rush/boss_rush_timer.hpp"
#include "boss_rush/boss_rush_portal.hpp"
#include "boss_rush/boss_rush_dpad.hpp"
#include "boss_rush/boss_rush_save.hpp"
#include "visible_equipment/visible_equipment.hpp"
#include "z_button/z_button.hpp"
#include "quick_access/quick_access.hpp"
#include "quick_access/quick_access_itemwheel.hpp"
#include "quick_access/quick_access_bottles.hpp"
#include "sheathed_spin/sheathed_spin.hpp"
#include "flurry_rush/flurry_rush.hpp"
#include "flurry_rush/flurry_vignette.hpp"
#include "puppet_zelda_pattern/puppet_zelda_pattern.hpp"
#include "stamina/stamina.hpp"
#include "stamina/sprint_human.hpp"
#include "stamina/sprint_wolf.hpp"
#include "stamina/sprint_swim.hpp"
#include "collection_menu/collection_menu.hpp"
#include "collection_menu/collection_menu_shield.hpp"
#include "controls/controls.hpp"
#include "util.hpp"

#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/config.h"
#include "mods/svc/ui.h"
#include "mods/svc/resource.h"
#include "mods/svc/texture.h"
#include "mods/svc/host.h"
#include "mods/svc/save.h"
#include "mods/svc/flow.h"
#include "mods/svc/message.h"
#include "mods/svc/actor.h"
#include "mods/svc/item.h"
#include "mods/svc/stage.h"
#include "mods/svc/gfx.h"
#include "mods/svc/camera.h"

#include "d/actor/d_a_title.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "d/d_camera.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"
#include "dusk/config_var.hpp"
#include "JSystem/JUtility/JUTFont.h"

#include <cstdio>
#include <string_view>

static constexpr float kHudAutoFadeIdleSeconds = 8.0f;
static constexpr float kHudAutoFadeFadeSeconds = 0.75f;
static constexpr float kHudAutoFadeRestAlpha   = 0.0f;

DEFINE_HOOK(&daTitle_c::fastLogoDispInit, TitleFastLogoDispInitHook);
DEFINE_HOOK(&daTitle_c::fastLogoDisp, TitleFastLogoDispExecuteHook);
DEFINE_HOOK(&mDoCPd_c::read, FreeCamPadReadHook);
DEFINE_HOOK(&dCamera_c::executeDebugFlyCam, FreeCamFlyCamHook);

static bool s_titleModActive = true;

struct daTitle_Access : public fopAc_ac_c {
    request_of_phase_process_class mPhaseReq;
    JKRHeap* mpHeap;
    J3DModel* mpModel;
    mDoExt_bckAnm mBck;
    mDoExt_bpkAnm mBpk;
    mDoExt_brkAnm mBrk;
    mDoExt_btkAnm mBtk;
    JKRExpHeap* m2DHeap;
    mDoDvdThd_mountArchive_c* mpMount;
    dDlst_daTitle_c mTitle;
    JUTFont* mpFont;
    u8 field_0x5f0[8];
    u8 mIsDispLogo;
    u8 field_0x5f9;
    u8 field_0x5fa;
    u8 mProcID;
    u8 mWaitTimer;
    CPaneMgrAlpha* field_0x600;
    u8 field_0x604;
};

void on_title_fast_logo_disp_init_post(ModContext*, void* args, void*, void*) {
    if (!args) return;
    daTitle_Access* title = (daTitle_Access*)mods::arg<daTitle_c*>(args, 0);
    if (title && s_titleModActive) {
        title->mWaitTimer = 0;
        title->field_0x5f9 = 1;
        title->field_0x5fa = 1;
        title->mIsDispLogo = 1;
    }
}

static HookAction on_title_fast_logo_disp_pre(ModContext*, void* args, void*, void*) {
    if (!args) return HOOK_CONTINUE;
    daTitle_Access* title = (daTitle_Access*)mods::arg<daTitle_c*>(args, 0);
    if (title && s_titleModActive) {
        title->mWaitTimer = 0;
        title->field_0x5f9 = 1;
        title->field_0x5fa = 1;
        title->mIsDispLogo = 1;
    }
    return HOOK_CONTINUE;
}

DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_OPTIONAL_SERVICE(ConfigService, svc_config);
IMPORT_OPTIONAL_SERVICE(UiService, svc_ui);
IMPORT_OPTIONAL_SERVICE(ResourceService, svc_resource);
IMPORT_OPTIONAL_SERVICE(TextureService, svc_texture);
IMPORT_OPTIONAL_SERVICE(HostService, svc_host);
IMPORT_OPTIONAL_SERVICE(SaveService, svc_save);
IMPORT_OPTIONAL_SERVICE(FlowService, svc_flow);
IMPORT_OPTIONAL_SERVICE(MessageService, svc_message);
IMPORT_OPTIONAL_SERVICE(ActorService, svc_actor);
IMPORT_OPTIONAL_SERVICE(ItemService, svc_item);
IMPORT_OPTIONAL_SERVICE(StageService, svc_stage);
IMPORT_OPTIONAL_SERVICE(GfxService, svc_gfx);

extern "C" MOD_EXPORT const void* const g_keep_mod_records[] = {
    &mod_meta_header_record,
    &mod_meta_import_svc_log,
    &mod_meta_import_svc_hook,
    &mod_meta_import_svc_config,
    &mod_meta_import_svc_ui,
    &mod_meta_import_svc_resource,
    &mod_meta_import_svc_texture,
    &mod_meta_import_svc_host,
    &mod_meta_import_svc_save,
    &mod_meta_import_svc_flow,
    &mod_meta_import_svc_message,
    &mod_meta_import_svc_actor,
    &mod_meta_import_svc_item,
    &mod_meta_import_svc_stage,
    &mod_meta_import_svc_gfx,
};

static constexpr float kFreeCamSlowFactor = 0.35f;

using FreeCamGetVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);

static FreeCamGetVarFn s_freeCamGetVar = nullptr;
static dusk::config::ConfigVarBase* s_freeCamVar = nullptr;
static dusk::config::ConfigVarBase* s_freeCamLockEventsVar = nullptr;
static bool s_freeCamToggleActive = false;
static cXyz s_flyCamSavedEye;
static cXyz s_flyCamSavedCenter;

static interface_of_controller_pad s_freeCamPadStash;
static bool s_freeCamPadStolen = false;

static void free_cam_blank_pad(interface_of_controller_pad& pad) {
    pad.mMainStickPosX = 0.0f;
    pad.mMainStickPosY = 0.0f;
    pad.mMainStickValue = 0.0f;
    pad.mMainStickAngle = 0;
    pad.mCStickPosX = 0.0f;
    pad.mCStickPosY = 0.0f;
    pad.mCStickValue = 0.0f;
    pad.mCStickAngle = 0;
    pad.mAnalogA = 0.0f;
    pad.mAnalogB = 0.0f;
    pad.mTriggerLeft = 0.0f;
    pad.mTriggerRight = 0.0f;
    pad.mButtonFlags &= PAD_BUTTON_START;
    pad.mPressedButtonFlags = 0;
}

static void free_cam_inject_pad(interface_of_controller_pad& pad) {
    pad = s_freeCamPadStash;
    pad.mCStickPosX *= kFreeCamSlowFactor;
    pad.mCStickPosY *= kFreeCamSlowFactor;
    pad.mButtonFlags &= ~PAD_TRIGGER_Z;
    pad.mPressedButtonFlags = 0;
}

static void on_free_cam_pad_read_post(ModContext*, void*, void*, void*) {
    if (s_freeCamVar == nullptr) return;

    if (dComIfGp_getPlayer(0) == nullptr) return;

    if (dComIfGp_isPauseFlag()) {
        s_freeCamPadStolen = false;
        return;
    }

    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);

    const u16 held = static_cast<u16>(pad.mButtonFlags);
    const u16 trig = static_cast<u16>(pad.mPressedButtonFlags);
    const bool combo = (held & PAD_TRIGGER_L) != 0 && (trig & PAD_BUTTON_A) != 0
                       && !quick_access_bottles_hotkey_active();

    if (combo) {
        auto* flyCam = static_cast<dusk::config::ConfigVar<bool>*>(s_freeCamVar);
        auto* lockEvents = s_freeCamLockEventsVar != nullptr
            ? static_cast<dusk::config::ConfigVar<bool>*>(s_freeCamLockEventsVar)
            : nullptr;
        if (s_freeCamToggleActive) {
            flyCam->clearOverride();
            if (lockEvents != nullptr) lockEvents->clearOverride();
            s_freeCamToggleActive = false;
            s_freeCamPadStolen = false;
        } else {
            flyCam->setOverrideValue(true);
            if (lockEvents != nullptr) lockEvents->setOverrideValue(true);
            s_freeCamToggleActive = true;
            s_freeCamPadStash = pad;
            s_freeCamPadStolen = true;
        }

        pad.mPressedButtonFlags &= ~PAD_BUTTON_A;
        pad.mButtonFlags &= ~PAD_BUTTON_A;
        return;
    }

    if (!s_freeCamToggleActive) return;

    s_freeCamPadStash = pad;
    s_freeCamPadStolen = true;
    free_cam_blank_pad(pad);
}

static HookAction on_fly_cam_pre(ModContext*, void* args, void*, void*) {
    if (!s_freeCamToggleActive) return HOOK_CONTINUE;
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr) return HOOK_CONTINUE;
    s_flyCamSavedEye = cam->mEye;
    s_flyCamSavedCenter = cam->mCenter;
    if (s_freeCamPadStolen) {
        free_cam_inject_pad(mDoCPd_c::getCpadInfo(PAD_1));
    }
    return HOOK_CONTINUE;
}

static void on_fly_cam_post(ModContext*, void* args, void*, void*) {
    if (!s_freeCamToggleActive) return;
    dCamera_c* cam = mods::arg<dCamera_c*>(args, 0);
    if (cam == nullptr) return;

    if (s_freeCamPadStolen) {
        free_cam_blank_pad(mDoCPd_c::getCpadInfo(PAD_1));
        s_freeCamPadStolen = false;

        dEvt_control_c* event = dComIfGp_getEvent();
        if (event != nullptr) {
            event->mEventStatus = 0;
        }
        g_dComIfG_gameInfo.play.getEvtManager().setCameraPlay(0);
        dScnPly_c::setPauseTimer(0);
    }

    if (cam->mDebugFlyCam.initialized) {
        cam->mEye = s_flyCamSavedEye + (cam->mEye - s_flyCamSavedEye) * kFreeCamSlowFactor;
        cam->mCenter = s_flyCamSavedCenter + (cam->mCenter - s_flyCamSavedCenter) * kFreeCamSlowFactor;
    }
}

static void init_free_camera_toggle(const HookService* hook_svc) {
    if (hook_svc == nullptr) return;

    void* addr = nullptr;
    if (hook_svc->resolve(mod_ctx, "dusk::config::GetConfigVar", &addr, nullptr) != MOD_OK) {
        return;
    }
    s_freeCamGetVar = reinterpret_cast<FreeCamGetVarFn>(addr);

    s_freeCamVar = s_freeCamGetVar("game.debugFlyCam");
    if (s_freeCamVar == nullptr) {
        return;
    }

    s_freeCamLockEventsVar = s_freeCamGetVar("game.debugFlyCamLockEvents");

    mods::hook::add_pre<FreeCamFlyCamHook>(hook_svc, on_fly_cam_pre);
    mods::hook::add_post<FreeCamFlyCamHook>(hook_svc, on_fly_cam_post);
    mods::hook::add_post<FreeCamPadReadHook>(hook_svc, on_free_cam_pad_read_post);
}

static void shutdown_free_camera_toggle() {
    if (!s_freeCamToggleActive || s_freeCamVar == nullptr) return;
    static_cast<dusk::config::ConfigVar<bool>*>(s_freeCamVar)->clearOverride();
    if (s_freeCamLockEventsVar != nullptr) {
        static_cast<dusk::config::ConfigVar<bool>*>(s_freeCamLockEventsVar)->clearOverride();
    }
    s_freeCamToggleActive = false;
}

static ConfigVarHandle s_varGeneralSkipCutscenes = 0;
static ConfigVarHandle s_varGeneralFastForwardCutscenes = 0;
static ConfigVarHandle s_varGeneralDominionSword = 0;
static ConfigVarHandle s_varHorseCamNoRecenter = 0;
static ConfigVarHandle s_varGeneralHumanWarp = 0;
static ConfigVarHandle s_varGeneralFasterMidnaCancel = 0;
static ConfigVarHandle s_varGeneralSceneTransitions = 0;
static ConfigVarHandle s_varGeneralLockonLetterbox = 0;
static ConfigVarHandle s_varHudAutoFade = 0;
static ConfigVarHandle s_varGeneralDrowningVignette = 0;
static ConfigVarHandle s_varGeneralSprintFovKick = 0;
static ConfigVarHandle s_varDamageVignette = 0;
static ConfigVarHandle s_varDamageVignetteIntensity = 0;
static ConfigVarHandle s_varHpBars = 0;
static ConfigVarHandle s_varHpBarsShowNumbers = 0;
static ConfigVarHandle s_varBossBar = 0;
static ConfigVarHandle s_varVisibleEquip = 0;
static ConfigVarHandle s_varVisibleEquipMode = 0;
static ConfigVarHandle s_varVisibleEquipMirrorBow = 0;
static ConfigVarHandle s_varVisibleEquipShowBow = 0;
static ConfigVarHandle s_varVisibleEquipShowLantern = 0;
static ConfigVarHandle s_varVisibleEquipQuiverOnBelt = 0;
static ConfigVarHandle s_varDamageNumbers = 0;
static ConfigVarHandle s_varCustomZButton = 0;
static ConfigVarHandle s_varQuickAccess = 0;
static ConfigVarHandle s_varQuickAccessAppearance = 0;
static ConfigVarHandle s_varQuickAccessHideWheelItems = 0;
static ConfigVarHandle s_varBottlesQuickAccess = 0;
static ConfigVarHandle s_varSheathedSpin = 0;
static ConfigVarHandle s_varFlurryRush = 0;
static ConfigVarHandle s_varFlurryRushPerfectFrames = 0;
static ConfigVarHandle s_varFlurryRushSlowFactor = 0;
static ConfigVarHandle s_varFlurryRushWindow = 0;
static ConfigVarHandle s_varFlurryRushHits = 0;
static ConfigVarHandle s_varStamina = 0;
static ConfigVarHandle s_varStaminaMax = 0;
static ConfigVarHandle s_varStaminaRegen = 0;
static ConfigVarHandle s_varStaminaSrcAttacks = 0;
static ConfigVarHandle s_varStaminaSrcJumpSpin = 0;
static ConfigVarHandle s_varStaminaSrcRolls = 0;
static ConfigVarHandle s_varStaminaSrcClimb = 0;
static ConfigVarHandle s_varStaminaSrcHang = 0;
static ConfigVarHandle s_varStaminaSrcSwim = 0;
static ConfigVarHandle s_varStaminaSrcPushPull = 0;
static ConfigVarHandle s_varStaminaSrcWolfDash = 0;
static ConfigVarHandle s_varStaminaSrcHiddenSkills = 0;
static ConfigVarHandle s_varStaminaSprint = 0;
static ConfigVarHandle s_varStaminaSrcSprint = 0;
static ConfigVarHandle s_varStaminaSprintSpeed = 0;
static ConfigVarHandle s_varStaminaWolfSprint = 0;
static ConfigVarHandle s_varStaminaWolfSprintSpeed = 0;
static ConfigVarHandle s_varStaminaSwimSprint = 0;
static ConfigVarHandle s_varStaminaSwimSprintSpeed = 0;
static ConfigVarHandle s_varStaminaCostAttack = 0;
static ConfigVarHandle s_varStaminaCostJumpAttack = 0;
static ConfigVarHandle s_varStaminaCostSpin = 0;
static ConfigVarHandle s_varStaminaCostRoll = 0;
static ConfigVarHandle s_varStaminaCostSidestep = 0;
static ConfigVarHandle s_varStaminaCostClimb = 0;
static ConfigVarHandle s_varStaminaCostHang = 0;
static ConfigVarHandle s_varStaminaCostCrawl = 0;
static ConfigVarHandle s_varStaminaCostSwim = 0;
static ConfigVarHandle s_varStaminaCostPushPull = 0;
static ConfigVarHandle s_varStaminaCostWolfDash = 0;
static ConfigVarHandle s_varStaminaCostSprint = 0;
static ConfigVarHandle s_varStaminaCostWolfSprint = 0;
static ConfigVarHandle s_varStaminaCostSwimSprint = 0;
static ConfigVarHandle s_varStaminaCostHiddenSkills = 0;
static ConfigVarHandle s_varPuppetZeldaPattern = 0;
static ConfigVarHandle s_varPuppetZeldaAlwaysShortest = 0;
static ConfigVarHandle s_varCollectionStarterEquip = 0;
static ConfigVarHandle s_varCollectionKeepOrdonShield = 0;
static ConfigVarHandle s_varCollectionShowOrdonHero = 0;
static ConfigVarHandle s_varCollectionOrdonHeroAlways = 0;
static ConfigVarHandle s_varBossRushSuggestedItems = 0;
static ConfigVarHandle s_varBossRushRefillAfterFight = 0;
static ConfigVarHandle s_varBossRushSeparateGanon = 0;
static ConfigVarHandle s_varBossRushVanillaGear = 0;
static ConfigVarHandle s_varBossRushTimer = 0;
static ConfigVarHandle s_varBossRushShowBestTimer = 0;
static ConfigVarHandle s_varBossRushBestTimes = 0;
static ConfigVarHandle s_varBossRushChainBest = 0;
static ConfigVarHandle s_varBossRushPortal = 0;

static bool s_generalInitialized = false;
static bool s_damageVignetteInitialized = false;
static bool s_oxygenVignetteInitialized = false;
static bool s_flurryVignetteInitialized = false;
static bool s_hpBarsInitialized = false;
static bool s_bossBarInitialized = false;
static bool s_bossRushInitialized = false;
static bool s_bossRushPortalInitialized = false;
static bool s_visibleEquipmentInitialized = false;
static bool s_zButtonInitialized = false;
static bool s_quickAccessInitialized = false;
static bool s_sheathedSpinInitialized = false;
static bool s_flurryRushInitialized = false;
static bool s_staminaInitialized = false;
static bool s_puppetZeldaPatternInitialized = false;
static bool s_collectionMenuInitialized = false;
static bool s_collectionMenuChestInitialized = false;

static void on_collection_starter_equip_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configCollectionStarterEquip = value->bool_value;
        request_collection_menu_reload();
    }
}

static void on_collection_keep_ordon_shield_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configCollectionKeepOrdonShield = value->bool_value;
        request_collection_menu_reload();
    }
}

static void on_collection_show_ordon_hero_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configCollectionShowOrdonHero = value->bool_value;
        sync_collection_ordon_hero_page();
        request_collection_menu_reload();
    }
}

static void on_collection_ordon_hero_always_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configCollectionOrdonHeroAlways = value->bool_value;
        request_collection_menu_reload();
    }
}

static void on_boss_rush_suggested_items_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushSuggestedItems = value->bool_value;
    }
}

static void on_boss_rush_refill_after_fight_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushRefillAfterFight = value->bool_value;
    }
}

static void on_boss_rush_separate_ganon_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushSeparateGanon = value->bool_value;
        if (is_in_boss_rush_chamber()) {
            reset_boss_rush_models();
        }
    }
}

static void on_boss_rush_vanilla_gear_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushVanillaGear = value->bool_value;
    }
}

static bool is_boss_rush_start_disabled(ModContext*, void*) {
    return is_boss_rush_active();
}

static bool is_boss_rush_timer_toggle_disabled(ModContext*, void*) {
    return is_boss_rush_active() && boss_rush_current_target_index() >= 0;
}

static bool is_boss_rush_fight_toggle_disabled(ModContext*, void*) {
    return !g_configBossRushTimer || is_boss_rush_timer_toggle_disabled(nullptr, nullptr);
}

static void on_horse_cam_no_recenter_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configHorseCamNoRecenter = value->bool_value;
    }
}

static void on_damage_vignette_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configDamageVignetteEnabled = value->bool_value;
        if (g_configDamageVignetteEnabled) {
            damage_vignette_request_preview();
        }
    }
}

static void on_damage_vignette_intensity_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configDamageVignetteIntensity = static_cast<int>(value->int_value);
        if (g_configDamageVignetteEnabled) {
            damage_vignette_request_preview();
        }
        if (g_configOxygenVignetteEnabled) {
            oxygen_vignette_request_preview();
        }
    }
}

static void on_oxygen_vignette_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configOxygenVignetteEnabled = value->bool_value;
        if (g_configOxygenVignetteEnabled) {
            oxygen_vignette_request_preview();
        }
    }
}

static bool is_boss_rush_timer_sub_disabled(ModContext* ctx, void* user) {
    return is_boss_rush_fight_toggle_disabled(ctx, user);
}

static void on_boss_rush_timer_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushTimer = value->bool_value;
    }
}

static void on_boss_rush_show_best_timer_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushShowBestTimer = value->bool_value;
    }
}

static void on_boss_rush_portal_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossRushPortal = value->bool_value;
    }
}

static void on_quick_access_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configQuickAccessEnabled = value->bool_value;
        quick_access_itemwheel_refresh();
    }
}

static void on_quick_access_appearance_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configQuickAccessAppearance = static_cast<int>(value->int_value);
    }
}

static void on_quick_access_hide_wheel_items_changed(ModContext* mod_ctx, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configQuickAccessHideWheelItems = value->bool_value;
    }
    quick_access_itemwheel_refresh();
}

static void on_bottles_quick_access_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBottlesQuickAccessEnabled = value->bool_value;
    }
}

static void on_sheathed_spin_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configSheathedSpinEnabled = value->bool_value;
    }
}

static void on_flurry_rush_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configFlurryRushEnabled = value->bool_value;
        flurry_rush_apply_enabled();
    }
}

static void on_flurry_rush_slow_factor_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 5) pct = 5;
        if (pct > 80) pct = 80;
        g_configFlurryRushSlowFactor = static_cast<int>(pct);
    }
}

static void on_flurry_rush_window_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t seconds = value->int_value;
        if (seconds < 1) seconds = 1;
        if (seconds > 10) seconds = 10;
        g_configFlurryRushWindowTicks = static_cast<int>(seconds * 30);
    }
}

static void on_flurry_rush_hits_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t hits = value->int_value;
        if (hits < 1) hits = 1;
        if (hits > 8) hits = 8;
        g_configFlurryRushHits = static_cast<int>(hits);
    }
}

static void on_flurry_rush_perfect_frames_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t frames = value->int_value;
        if (frames < 5) frames = 5;
        if (frames > 120) frames = 120;
        g_configFlurryRushPerfectFrames = static_cast<int>(frames);
    }
}

static void on_stamina_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configStaminaEnabled = value->bool_value;
    }
}

static void on_stamina_max_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configStaminaMax = static_cast<int>(value->int_value);
    }
}

static void on_stamina_regen_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configStaminaRegen = static_cast<int>(value->int_value);
    }
}

static void on_stamina_src_attacks_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcAttacks = value->bool_value;
}
static void on_stamina_src_jump_spin_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcJumpSpin = value->bool_value;
}
static void on_stamina_src_rolls_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcRolls = value->bool_value;
}
static void on_stamina_src_climb_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcClimb = value->bool_value;
}
static void on_stamina_src_hang_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcHang = value->bool_value;
}
static void on_stamina_src_swim_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcSwim = value->bool_value;
}
static void on_stamina_src_push_pull_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcPushPull = value->bool_value;
}
static void on_stamina_src_wolf_dash_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcWolfDash = value->bool_value;
}

static void on_stamina_src_hidden_skills_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcHiddenSkills = value->bool_value;
}

static void on_stamina_sprint_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSprint = value->bool_value;
}

static void on_stamina_src_sprint_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSrcSprint = value->bool_value;
}

static void on_stamina_sprint_speed_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 100) pct = 100;
        if (pct > 200) pct = 200;
        g_configStaminaSprintSpeed = static_cast<float>(pct) / 100.0f;
    }
}

static void on_stamina_wolf_sprint_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaWolfSprint = value->bool_value;
}

static void on_stamina_wolf_sprint_speed_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 100) pct = 100;
        if (pct > 200) pct = 200;
        g_configStaminaWolfSprintSpeed = static_cast<float>(pct) / 100.0f;
    }
}

static void on_stamina_swim_sprint_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) g_configStaminaSwimSprint = value->bool_value;
}

static void on_stamina_swim_sprint_speed_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        int64_t pct = value->int_value;
        if (pct < 100) pct = 100;
        if (pct > 200) pct = 200;
        g_configStaminaSwimSprintSpeed = static_cast<float>(pct) / 100.0f;
    }
}

static void on_stamina_cost_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
                                    const ConfigVarValue*, void* user_data) {
    if (value && user_data) {
        *static_cast<int*>(user_data) = static_cast<int>(value->int_value);
    }
}

static void on_puppet_zelda_pattern_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configPuppetZeldaPatternEnabled = value->bool_value;
    }
}

static void on_puppet_zelda_always_shortest_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configPuppetZeldaAlwaysShortest = value->bool_value;
    }
}

static void on_hp_bars_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configHpBarsEnabled = value->bool_value;
    }
}

static void on_general_skip_cutscenes_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configGeneralSkipCutscenes = value->bool_value;
    }
}

static void on_general_fast_forward_cutscenes_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configGeneralFastForwardCutscenes = value->bool_value;
    }
}

static void on_general_dominion_sword_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configGeneralDominionSword = value->bool_value;
    }
}

static void on_general_human_warp_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configGeneralHumanWarpAnimation = value->bool_value;
    }
}

static void on_general_faster_midna_cancel_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configFasterMidnaCancel = value->bool_value;
    }
}

static void on_general_lockon_letterbox_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configLockonNoLetterbox = value->bool_value;
    }
}

static void on_hud_auto_fade_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configHudAutoFadeEnabled = value->bool_value;
    }
}

static void on_general_sprint_fov_kick_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configSprintFovKickEnabled = value->bool_value;
    }
}

static void on_general_scene_transitions_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configFasterTransitions = value->int_value == 0;
        faster_transitions_apply_mode();
    }
}


static void on_boss_bar_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configBossBarEnabled = value->bool_value;
    }
}

static void on_hp_bars_show_numbers_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configHpBarsShowNumbers = value->bool_value;
    }
}

static void on_visible_equip_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipmentEnabled = value->bool_value;
    }
}

static void on_visible_equip_mode_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipDisplayMode = value->bool_value ? 1 : 0;
    }
}

static void on_visible_equip_mirror_bow_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipMirrorBow = value->bool_value;
    }
}

static void on_visible_equip_show_bow_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipShowBow = value->bool_value;
    }
}

static void on_visible_equip_show_lantern_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipShowLantern = value->bool_value;
    }
}

static void on_visible_equip_quiver_on_belt_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configVisibleEquipQuiverOnBelt = value->bool_value;
    }
}

static void on_damage_numbers_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        g_configDamageNumbersEnabled = value->bool_value;
    }
}

static void on_custom_z_button_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value) {
        if (isNativeZButtonEngine()) {
            g_configCustomZButtonEnabled = false;
            g_configZButtonEnabled = false;
            return;
        }
        g_configCustomZButtonEnabled = value->bool_value;
        g_configZButtonEnabled = value->bool_value;
    }
}

static bool is_hp_bars_sub_disabled(ModContext*, void*) {
    return !g_configHpBarsEnabled;
}

static bool is_visible_equip_sub_disabled(ModContext*, void*) {
    return !g_configVisibleEquipmentEnabled;
}

static bool is_puppet_zelda_sub_disabled(ModContext*, void*) {
    return !g_configPuppetZeldaPatternEnabled;
}

static bool is_collection_starter_sub_disabled(ModContext*, void*) {
    return !g_configCollectionStarterEquip;
}

static bool is_collection_ordon_hero_sub_disabled(ModContext*, void*) {
    return !g_configCollectionShowOrdonHero;
}

static bool is_stamina_sub_disabled(ModContext*, void*) {
    return !g_configStaminaEnabled;
}

static bool is_sprint_speed_disabled(ModContext*, void*) {
    return !g_configStaminaSprint;
}

static bool is_wolf_sprint_speed_disabled(ModContext*, void*) {
    return !g_configStaminaWolfSprint;
}

static bool is_swim_sprint_speed_disabled(ModContext*, void*) {
    return !g_configStaminaSwimSprint;
}

static bool is_quick_access_sub_disabled(ModContext*, void*) {
    return !g_configQuickAccessEnabled;
}

static bool is_bottles_sub_disabled(ModContext*, void*) {
    return !g_configBottlesQuickAccessEnabled;
}

static bool is_z_slot_sub_disabled(ModContext*, void*) {
    return !g_configCustomZButtonEnabled;
}

static ModResult build_visible_equip_dialog(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    if (!svc_ui) return MOD_OK;

    if (s_varVisibleEquipShowBow != 0) {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_TOGGLE;
        ctrl.label = "Hero's Bow & Quiver";
        ctrl.binding = UI_BINDING_CONFIG_VAR;
        ctrl.config_var = s_varVisibleEquipShowBow;
        svc_ui->pane_add_control(ctx, pane, &ctrl, nullptr);
    }

    if (s_varVisibleEquipShowLantern != 0) {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_TOGGLE;
        ctrl.label = "Lantern (Belt)";
        ctrl.binding = UI_BINDING_CONFIG_VAR;
        ctrl.config_var = s_varVisibleEquipShowLantern;
        svc_ui->pane_add_control(ctx, pane, &ctrl, nullptr);
    }

    return MOD_OK;
}

static void stamina_dialog_toggle(ModContext* ctx, UiElementHandle pane, const char* label, ConfigVarHandle var) {
    if (var == 0) return;
    UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
    ctrl.kind = UI_CONTROL_TOGGLE;
    ctrl.label = label;
    ctrl.binding = UI_BINDING_CONFIG_VAR;
    ctrl.config_var = var;
    svc_ui->pane_add_control(ctx, pane, &ctrl, nullptr);
}

static ModResult build_stamina_dialog(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    if (!svc_ui) return MOD_OK;
    stamina_dialog_toggle(ctx, pane, "Sword attacks", s_varStaminaSrcAttacks);
    stamina_dialog_toggle(ctx, pane, "Hidden Skills", s_varStaminaSrcHiddenSkills);
    stamina_dialog_toggle(ctx, pane, "Jump & spin attacks", s_varStaminaSrcJumpSpin);
    stamina_dialog_toggle(ctx, pane, "Rolls, side hops & backflips", s_varStaminaSrcRolls);
    stamina_dialog_toggle(ctx, pane, "Sprint", s_varStaminaSrcSprint);
    stamina_dialog_toggle(ctx, pane, "Climbing walls", s_varStaminaSrcClimb);
    stamina_dialog_toggle(ctx, pane, "Hanging on ledges", s_varStaminaSrcHang);
    stamina_dialog_toggle(ctx, pane, "Swimming", s_varStaminaSrcSwim);
    stamina_dialog_toggle(ctx, pane, "Pushing & pulling", s_varStaminaSrcPushPull);
    stamina_dialog_toggle(ctx, pane, "Wolf sprint", s_varStaminaSrcWolfDash);
    return MOD_OK;
}

static void on_open_stamina_dialog(ModContext* ctx, void*) {
    if (!svc_ui) return;

    static UiDialogAction s_staminaDoneAction;
    s_staminaDoneAction.struct_size = sizeof(UiDialogAction);
    s_staminaDoneAction.label = "Done";
    s_staminaDoneAction.on_pressed = nullptr;
    s_staminaDoneAction.user_data = nullptr;
    s_staminaDoneAction.keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Stamina Activities";
    desc.body_rml = "Choose which activities draw from the stamina meter:";
    desc.variant = UI_DIALOG_NORMAL;
    desc.actions = &s_staminaDoneAction;
    desc.action_count = 1;
    desc.build = build_stamina_dialog;

    UiDialogHandle hDialog = 0;
    svc_ui->dialog_push(ctx, &desc, &hDialog);
}

static void stamina_dialog_number(ModContext* ctx, UiElementHandle pane, const char* label,
                                  const char* help_rml, ConfigVarHandle var) {
    if (!svc_ui || var == 0) return;
    UiControlDesc c = UI_CONTROL_DESC_INIT;
    c.kind = UI_CONTROL_NUMBER;
    c.label = label;
    c.help_rml = help_rml;
    c.binding = UI_BINDING_CONFIG_VAR;
    c.config_var = var;
    c.is_disabled = is_stamina_sub_disabled;
    c.min = 5;
    c.max = 400;
    c.step = 5;
    c.suffix = "%";
    svc_ui->pane_add_control(ctx, pane, &c, nullptr);
}

static ModResult build_stamina_costs_dialog(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    if (!svc_ui) return MOD_OK;

    svc_ui->pane_add_section(mod_ctx, pane, "Actions (cost per use)");
    stamina_dialog_number(ctx, pane, "Sword attack",
        "<p>Stamina cost per sword swing (default: 10 of the 100-point pool).</p>",
        s_varStaminaCostAttack);
    stamina_dialog_number(ctx, pane, "Jump attack",
        "<p>Stamina cost per jump attack - the leaping stab and the big leaping "
        "overhead (default: 14).</p>",
        s_varStaminaCostJumpAttack);
    stamina_dialog_number(ctx, pane, "Hidden skill",
        "<p>Stamina cost per hidden skill move - Shield Attack, Back Slice, Helm "
        "Splitter, Ending Blow and Mortal Draw (default: 14).</p>",
        s_varStaminaCostHiddenSkills);
    stamina_dialog_number(ctx, pane, "Spin attack (extra)",
        "<p>Extra charge on top of the swing the spin input already costs, so a spin "
        "totals swing + this (default: 10 extra).</p>",
        s_varStaminaCostSpin);
    stamina_dialog_number(ctx, pane, "Roll / backflip",
        "<p>Stamina cost per roll, side roll and backflip (default: 14).</p>",
        s_varStaminaCostRoll);
    stamina_dialog_number(ctx, pane, "Side hop",
        "<p>Stamina cost per side hop while locked on (default: 12).</p>",
        s_varStaminaCostSidestep);

    svc_ui->pane_add_section(mod_ctx, pane, "Continuous (drain per frame while doing)");
    stamina_dialog_number(ctx, pane, "Climbing",
        "<p>Wall climbing drain per frame (default: 0.55).</p>",
        s_varStaminaCostClimb);
    stamina_dialog_number(ctx, pane, "Ledge hang & shimmy",
        "<p>Hanging on a ledge and shuffling sideways drain per frame (default: 0.45).</p>",
        s_varStaminaCostHang);
    stamina_dialog_number(ctx, pane, "Crawling",
        "<p>Crawling drain per frame (default: 0.22).</p>",
        s_varStaminaCostCrawl);
    stamina_dialog_number(ctx, pane, "Swimming",
        "<p>Swimming and diving drain per frame - idle floating is free (default: 0.40).</p>",
        s_varStaminaCostSwim);
    stamina_dialog_number(ctx, pane, "Pushing & pulling",
        "<p>Pushing or pulling objects drain per frame (default: 0.55).</p>",
        s_varStaminaCostPushPull);
    stamina_dialog_number(ctx, pane, "Wolf dash (vanilla)",
        "<p>The vanilla wolf dash drain per frame - also applies to the held wolf "
        "sprint's dash bursts (default: 0.90).</p>",
        s_varStaminaCostWolfDash);
    stamina_dialog_number(ctx, pane, "Human sprint",
        "<p>Human sprint (hold the roll button) drain per frame (default: 0.90).</p>",
        s_varStaminaCostSprint);
    stamina_dialog_number(ctx, pane, "Wolf sprint",
        "<p>Held wolf sprint drain per frame while running between dash bursts "
        "(default: 0.90).</p>",
        s_varStaminaCostWolfSprint);
    stamina_dialog_number(ctx, pane, "Swim sprint",
        "<p>Swim sprint (hold the roll button) drain per frame (default: 0.85).</p>",
        s_varStaminaCostSwimSprint);
    return MOD_OK;
}

static void on_open_stamina_costs_dialog(ModContext* ctx, void*) {
    if (!svc_ui) return;

    static UiDialogAction s_costsDoneAction;
    s_costsDoneAction.struct_size = sizeof(UiDialogAction);
    s_costsDoneAction.label = "Done";
    s_costsDoneAction.on_pressed = nullptr;
    s_costsDoneAction.user_data = nullptr;
    s_costsDoneAction.keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Stamina Costs";
    desc.body_rml = "Every cost as a percentage of its default (100% = default).";
    desc.variant = UI_DIALOG_NORMAL;
    desc.actions = &s_costsDoneAction;
    desc.action_count = 1;
    desc.build = build_stamina_costs_dialog;

    UiDialogHandle hDialog = 0;
    svc_ui->dialog_push(ctx, &desc, &hDialog);
}

static void on_open_visible_equip_dialog(ModContext* ctx, void*) {
    if (!svc_ui) return;

    static UiDialogAction s_doneAction;
    s_doneAction.struct_size = sizeof(UiDialogAction);
    s_doneAction.label = "Done";
    s_doneAction.on_pressed = nullptr;
    s_doneAction.user_data = nullptr;
    s_doneAction.keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Visible Equipment";
    desc.body_rml = "Select which items to display on Link's model:";
    desc.variant = UI_DIALOG_NORMAL;
    desc.actions = &s_doneAction;
    desc.action_count = 1;
    desc.build = build_visible_equip_dialog;

    UiDialogHandle hDialog = 0;
    svc_ui->dialog_push(ctx, &desc, &hDialog);
}

static void ui_add_toggle(UiElementHandle pane, const char* label, ConfigVarHandle var,
                          const char* help_rml, UiPredicateFn disabled = nullptr) {
    if (!svc_ui || var == 0) return;
    UiControlDesc c = UI_CONTROL_DESC_INIT;
    c.kind = UI_CONTROL_TOGGLE;
    c.label = label;
    c.help_rml = help_rml;
    c.binding = UI_BINDING_CONFIG_VAR;
    c.config_var = var;
    c.is_disabled = disabled;
    svc_ui->pane_add_control(mod_ctx, pane, &c, nullptr);
}

static void ui_add_select(UiElementHandle pane, const char* label, ConfigVarHandle var,
                          const char* help_rml, const char* const* options, size_t option_count,
                          UiPredicateFn disabled = nullptr) {
    if (!svc_ui || var == 0) return;
    UiControlDesc c = UI_CONTROL_DESC_INIT;
    c.kind = UI_CONTROL_SELECT;
    c.label = label;
    c.help_rml = help_rml;
    c.binding = UI_BINDING_CONFIG_VAR;
    c.config_var = var;
    c.options = options;
    c.option_count = option_count;
    c.is_disabled = disabled;
    svc_ui->pane_add_control(mod_ctx, pane, &c, nullptr);
}

static ModResult tab_quality_of_life(ModContext*, UiWindowHandle, UiElementHandle left,
                                     UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Quality of life options.</p>", nullptr);

    static const char* const kSceneTransitionModes[] = {"Fast", "Vanilla"};

    svc_ui->pane_add_section(mod_ctx, left, "Cutscenes");
    ui_add_toggle(left, "Skip all cutscenes", s_varGeneralSkipCutscenes,
        "<p>Skips skippable cutscenes automatically.</p>");
    ui_add_toggle(left, "Fast-forward unskippable cutscenes", s_varGeneralFastForwardCutscenes,
        "<p>Plays unskippable cutscenes at 4x speed.</p>");

    svc_ui->pane_add_section(mod_ctx, left, "Scene Transitions");
    ui_add_select(left, "Transition speed", s_varGeneralSceneTransitions,
        "<p><b>Fast</b> speeds up room, door and map transitions. "
        "<b>Vanilla</b> keeps the normal speed.</p>",
        kSceneTransitionModes, 2);

    svc_ui->pane_add_section(mod_ctx, left, "Warping");
    ui_add_toggle(left, "Warp as human", s_varGeneralHumanWarp,
        "<p>Human Link warps with the light beam instead of turning into a wolf first.</p>");

    svc_ui->pane_add_section(mod_ctx, left, "Midna");
    ui_add_toggle(left, "Faster call cancel", s_varGeneralFasterMidnaCancel,
        "<p>Lets you cancel Midna's call faster.</p>");

    svc_ui->pane_add_section(mod_ctx, left, "Z Button Slot");
    if (isNativeZButtonEngine()) {
        svc_ui->pane_add_rml(mod_ctx, left,
            "<span style=\"color: #a8bcd4;\">This Dusklight build (Lazy Tweaks) already "
            "provides 3-slot Z-button support natively.</span>", nullptr);
    } else {
        ui_add_toggle(left, "Enabled", s_varCustomZButton,
            "<p>Enables a 3rd item slot on the Z button. Midna moves to a separate button - "
            "both can be changed in the Controls tab.</p>");
    }

    svc_ui->pane_add_section(mod_ctx, left, "Stamina");
    ui_add_toggle(left, "Enabled", s_varStamina,
        "<p>Adds a stamina meter for attacks, sprint, climbing, and swimming.</p>");

    {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_BUTTON;
        c.label = "Choose Stamina Activities...";
        c.help_rml = "<p>Choose which actions consume stamina.</p>";
        c.on_pressed = on_open_stamina_dialog;
        c.is_disabled = is_stamina_sub_disabled;
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_BUTTON;
        c.label = "Set Stamina Costs...";
        c.help_rml = "<p>Set the stamina cost of every action as a percentage of its default.</p>";
        c.on_pressed = on_open_stamina_costs_dialog;
        c.is_disabled = is_stamina_sub_disabled;
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    if (s_varStaminaMax != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Max stamina";
        c.help_rml = "<p>Maximum stamina capacity (default: 100).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varStaminaMax;
        c.is_disabled = is_stamina_sub_disabled;
        c.min = 40;
        c.max = 300;
        c.step = 10;
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    if (s_varStaminaRegen != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Refill speed";
        c.help_rml = "<p>Stamina recovery speed percentage (default: 100%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varStaminaRegen;
        c.is_disabled = is_stamina_sub_disabled;
        c.min = 25;
        c.max = 400;
        c.step = 25;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    ui_add_toggle(left, "Sprint (hold roll button)", s_varStaminaSprint,
        "<p>Hold the roll button while running to sprint.</p>");
    if (s_varStaminaSprintSpeed != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Sprint speed";
        c.help_rml = "<p>Human sprint speed percentage (default: 110%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varStaminaSprintSpeed;
        c.is_disabled = is_sprint_speed_disabled;
        c.min = 100;
        c.max = 200;
        c.step = 5;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    ui_add_toggle(left, "Wolf sprint (hold roll button)", s_varStaminaWolfSprint,
        "<p>Hold the roll button as Wolf Link to continuously sprint.</p>");
    if (s_varStaminaWolfSprintSpeed != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Wolf sprint speed";
        c.help_rml = "<p>Wolf sprint speed percentage (default: 110%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varStaminaWolfSprintSpeed;
        c.is_disabled = is_wolf_sprint_speed_disabled;
        c.min = 100;
        c.max = 200;
        c.step = 5;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    ui_add_toggle(left, "Swim sprint (hold roll button)", s_varStaminaSwimSprint,
        "<p>Hold the roll button while swimming to continuously sprint (non-Zora tunics only).</p>");
    if (s_varStaminaSwimSprintSpeed != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Swim sprint speed";
        c.help_rml = "<p>Swim sprint speed percentage (default: 115%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varStaminaSwimSprintSpeed;
        c.is_disabled = is_swim_sprint_speed_disabled;
        c.min = 100;
        c.max = 200;
        c.step = 5;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    return MOD_OK;
}

static ModResult tab_general(ModContext*, UiWindowHandle, UiElementHandle left,
                             UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>General options.</p>", nullptr);
    /*ui_add_toggle(left, "Two-handed sword carry (test)", s_varGeneralDominionSword,
        "<p>Link holds his drawn sword with both hands.</p>");*/
    ui_add_toggle(left, "Horse camera: no auto-recenter", s_varHorseCamNoRecenter,
        "<p>On Epona, the camera stays where you point it with the C-Stick.</p>");
    ui_add_toggle(left, "No letterbox while lock-on", s_varGeneralLockonLetterbox,
        "<p>No black bars on the screen while Z-targeting.</p>");
    ui_add_toggle(left, "Enable HUD auto fade", s_varHudAutoFade,
        "<p>Fades the whole HUD out while Link stands still, and back in the moment he moves.</p>");
    ui_add_toggle(left, "Sprint FOV kick", s_varGeneralSprintFovKick,
        "<p>The camera zooms out slightly while sprinting.</p>");

    svc_ui->pane_add_section(mod_ctx, left, "Indicator");
    ui_add_toggle(left, "Damage vignette", s_varDamageVignette,
        "<p>Red screen-edge flash when hit, plus a pulsing vignette at low health.</p>");
    ui_add_toggle(left, "Drowning vignette", s_varGeneralDrowningVignette,
        "<p>Blue screen-edge vignette while the air meter runs low.</p>");
    if (s_varDamageVignetteIntensity != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Intensity";
        c.help_rml = "<p>Overlay strength in percent for both vignettes (default: 50%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varDamageVignetteIntensity;
        c.min = 5;
        c.max = 100;
        c.step = 5;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    return MOD_OK;
}

static ModResult tab_visuals(ModContext*, UiWindowHandle, UiElementHandle left,
                             UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Visual customization options.</p>", nullptr);

    svc_ui->pane_add_section(mod_ctx, left, "Visible Equipment");
    ui_add_toggle(left, "Enabled", s_varVisibleEquip,
        "<p>Shows the bow, quiver and lantern on Link.</p>");
    {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_BUTTON;
        c.label = "Choose Visible Items...";
        c.help_rml = "<p>Select which items are displayed on Link.</p>";
        c.on_pressed = on_open_visible_equip_dialog;
        c.is_disabled = is_visible_equip_sub_disabled;
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    ui_add_toggle(left, "Show gear always (even when not equipped)", s_varVisibleEquipMode,
        "<p>Shows gear even when it is not equipped.</p>", is_visible_equip_sub_disabled);
    ui_add_toggle(left, "Mirror bow angle", s_varVisibleEquipMirrorBow,
        "<p>Flips the slung bow to Link's opposite shoulder.</p>", is_visible_equip_sub_disabled);
    ui_add_toggle(left, "Quiver on left belt", s_varVisibleEquipQuiverOnBelt,
        "<p>Moves the quiver from the back to the left hip.</p>",
        is_visible_equip_sub_disabled);
    return MOD_OK;
}

static ModResult tab_quick_access(ModContext*, UiWindowHandle, UiElementHandle left,
                                  UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Quick item access and input modifications.</p>", nullptr);

    svc_ui->pane_add_section(mod_ctx, left, "Quick Access");
    static const char* const kQuickAccessAppearances[] = { "Radial", "Item Bar (BotW-style)" };
    ui_add_toggle(left, "Enabled", s_varQuickAccess,
        "<p>Tap the Quick Access button to use the item assigned to it. Hold it to open the "
        "item menu. The button can be changed in the Controls tab.</p>");
    ui_add_select(left, "Appearance", s_varQuickAccessAppearance,
        "<p><b>Radial</b>: vanilla item wheel look. <b>Item Bar</b>: horizontal bar at the top "
        "of the screen, like in Breath of the Wild. Press X while the menu is open to "
        "customize its items.</p>",
        kQuickAccessAppearances, 2, is_quick_access_sub_disabled);
    ui_add_toggle(left, "Hide items from item wheel", s_varQuickAccessHideWheelItems,
        "<p>Hides your quick items from the normal item wheel. Disabling Quick Access "
        "restores them.</p>",
        is_quick_access_sub_disabled);

    svc_ui->pane_add_section(mod_ctx, left, "Bottle Quick Access");
    ui_add_toggle(left, "Bottle quick access", s_varBottlesQuickAccess,
        "<p>Your four bottles in their own menu. Tap the bottle button to use the selected "
        "bottle, hold it to open the menu. Note: while bound to L, L no longer triggers "
        "targeting/shield. The button can be changed in the Controls tab.</p>");

    return MOD_OK;
}

static void on_start_boss_rush_pressed(ModContext*, void*) {
    start_boss_rush();
}

static void on_kill_current_boss_pressed(ModContext*, void*) {
    boss_rush_debug_kill_current_boss();
}

static void on_export_boss_rush_save_pressed(ModContext*, void*) {
    boss_rush_save_export_current();
}

static void on_confirm_clear_best_pressed(ModContext* ctx, UiDialogHandle dialog, void*) {
    boss_rush_timer_clear_best();
}

static void on_clear_boss_rush_best_times_pressed(ModContext* mod_ctx, void*) {
    if (!svc_ui) return;

    static UiDialogAction actions[2];
    actions[0].struct_size = sizeof(UiDialogAction);
    actions[0].label = "Cancel";
    actions[0].on_pressed = nullptr;
    actions[0].user_data = nullptr;
    actions[0].keep_open = false;

    actions[1].struct_size = sizeof(UiDialogAction);
    actions[1].label = "Yes, clear";
    actions[1].on_pressed = on_confirm_clear_best_pressed;
    actions[1].user_data = nullptr;
    actions[1].keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Clear Best Times";
    desc.body_rml = "<p>Are you sure you want to clear all Boss Rush best times?</p>";
    desc.variant = UI_DIALOG_DANGER;
    desc.icon = "warning";
    desc.actions = actions;
    desc.action_count = 2;

    UiDialogHandle dialog = 0;
    svc_ui->dialog_push(mod_ctx, &desc, &dialog);
}

static ModResult tab_combat(ModContext*, UiWindowHandle, UiElementHandle left,
                            UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Combat tweaks and display overlays.</p>", nullptr);

    svc_ui->pane_add_section(mod_ctx, left, "Enemy HP Bars");
    ui_add_toggle(left, "Show HP bars", s_varHpBars,
        "<p>Shows health bars above nearby enemies.</p>");
    ui_add_toggle(left, "Show exact HP numbers", s_varHpBarsShowNumbers,
        "<p>Shows current/max HP numbers on enemy health bars.</p>", is_hp_bars_sub_disabled);
    ui_add_toggle(left, "Show damage numbers", s_varDamageNumbers,
        "<p>Displays floating damage numbers when enemies take damage.</p>");
    ui_add_toggle(left, "Show boss bars", s_varBossBar,
        "<p>Displays a boss health bar at the top of the screen during boss fights.</p>");

    svc_ui->pane_add_section(mod_ctx, left, "Sheathed Spin");
    ui_add_toggle(left, "Enabled", s_varSheathedSpin,
        "<p>Allows performing a spin attack directly while the sword is sheathed.</p>");

#if 0
    svc_ui->pane_add_section(mod_ctx, left, "Flurry Rush");
    ui_add_toggle(left, "Enabled", s_varFlurryRush,
        "<p>Dodge through an enemy attack with perfect timing to slow down time and land extra hits.</p>");
    if (s_varFlurryRushPerfectFrames != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Perfect dodge window";
        c.help_rml = "<p>How long after a dodge an attack still counts as a perfect dodge "
                     "(default: 30).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varFlurryRushPerfectFrames;
        c.min = 5;
        c.max = 120;
        c.step = 5;
        c.suffix = " frames";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    if (s_varFlurryRushSlowFactor != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Time scale";
        c.help_rml = "<p>Game speed during the slow motion, as a percentage (default: 30%).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varFlurryRushSlowFactor;
        c.min = 5;
        c.max = 80;
        c.step = 5;
        c.suffix = "%";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    if (s_varFlurryRushHits != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Flurry hits";
        c.help_rml = "<p>How many hits complete the flurry (default: 4).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varFlurryRushHits;
        c.min = 1;
        c.max = 8;
        c.step = 1;
        c.suffix = " hits";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
    if (s_varFlurryRushWindow != 0) {
        UiControlDesc c = UI_CONTROL_DESC_INIT;
        c.kind = UI_CONTROL_NUMBER;
        c.label = "Max window";
        c.help_rml = "<p>Cap for the slow-motion window, in seconds (default: 5).</p>";
        c.binding = UI_BINDING_CONFIG_VAR;
        c.config_var = s_varFlurryRushWindow;
        c.min = 1;
        c.max = 10;
        c.step = 1;
        c.suffix = "s";
        svc_ui->pane_add_control(mod_ctx, left, &c, nullptr);
    }
#endif

    svc_ui->pane_add_section(mod_ctx, left, "Puppet Zelda");
    ui_add_toggle(left, "Enabled", s_varPuppetZeldaPattern,
        "<p>Replaces Puppet Zelda's RNG with a fixed, learnable attack pattern.</p>");
    ui_add_toggle(left, "Always shortest attacks (Sword Dive only)", s_varPuppetZeldaAlwaysShortest,
        "<p>Puppet Zelda only uses Sword Dive attacks for quick reflect practice.</p>", is_puppet_zelda_sub_disabled);

    return MOD_OK;
}

static ModResult tab_menus(ModContext*, UiWindowHandle, UiElementHandle left,
                           UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Menu and collection screen options.</p>", nullptr);

    svc_ui->pane_add_section(mod_ctx, left, "Collection Screen");
    ui_add_toggle(left, "Add wooden sword, ordon shield and ordon clothes", s_varCollectionStarterEquip,
        "<p>Adds Wooden Sword, Ordon Shield, and Ordon Clothes as equippable slots on the Collection screen.</p>");
    ui_add_toggle(left, "Keep Ordon Shield in collection", s_varCollectionKeepOrdonShield,
        "<p>Keeps the Ordon Shield equippable in the Collection screen even if burnt. "
        "Requires the starter gear option.</p>", is_collection_starter_sub_disabled);
    ui_add_toggle(left, "Show Ordon Hero", s_varCollectionShowOrdonHero,
        "<p>Shows the extra Ordon Hero gear (Reinforced Shield and Ordon Hero tunic) "
        "on the Collection screen and creates a second page. Cycle pages with R and L - "
        "hold them briefly.</p>");
    ui_add_toggle(left, "Show always Ordon Hero", s_varCollectionOrdonHeroAlways,
        "<p>When off, the gear only appears once unlocked: the Reinforced Shield once you "
        "own the Ordon Shield, the Ordon Hero tunic once you have the Hero's Clothes. "
        "When on, both are always shown. Requires the Show Ordon Hero option.</p>",
        is_collection_ordon_hero_sub_disabled);
    return MOD_OK;
}

static ModResult tab_boss_rush(ModContext*, UiWindowHandle, UiElementHandle left,
                               UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Fight any boss on demand. Warning: spoilers for the whole game.</p>",
        nullptr);

    {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_BUTTON;
        ctrl.label = "Start Boss Rush";
        ctrl.help_rml = "<p>Warps you to the Boss Rush chamber.</p>";
        ctrl.on_pressed = on_start_boss_rush_pressed;
        ctrl.is_disabled = is_boss_rush_start_disabled;
        svc_ui->pane_add_control(mod_ctx, left, &ctrl, nullptr);
    }

    ui_add_toggle(left, "Automatically assign suggested items", s_varBossRushSuggestedItems,
        "<p>Automatically assigns recommended items when entering a boss fight.</p>");

    ui_add_toggle(left, "Refill after every boss fight", s_varBossRushRefillAfterFight,
        "<p>Refills health and item ammo to full upon returning to the chamber.</p>");

    ui_add_toggle(left, "Vanilla gear only", s_varBossRushVanillaGear,
        "<p>Restricts Link's hearts and gear to what is normally available at that boss.</p>");

    ui_add_toggle(left, "Boss Rush timer", s_varBossRushTimer,
        "<p>Displays a fight timer on screen and tracks personal best times for each boss.</p>",
        is_boss_rush_timer_toggle_disabled);
    ui_add_toggle(left, "Show best timer below timer", s_varBossRushShowBestTimer,
        "<p>Shows your personal best below the timer.</p>",
        is_boss_rush_fight_toggle_disabled);

    {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_BUTTON;
        ctrl.label = "Clear best timers";
        ctrl.help_rml = "<p>Resets all stored Boss Rush best times.</p>";
        ctrl.on_pressed = on_clear_boss_rush_best_times_pressed;
        svc_ui->pane_add_control(mod_ctx, left, &ctrl, nullptr);
    }

    ui_add_toggle(left, "Separate Ganon fights", s_varBossRushSeparateGanon,
        "<p>Fights all 4 Ganon phases in sequence as a single gauntlet instead of separate statues.</p>");

    ui_add_toggle(left, "Map portal", s_varBossRushPortal,
        "<p>Press the portal button on the map screen (see Controls tab) to warp directly "
        "into the Boss Rush chamber.</p>");

#if 0
    {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_BUTTON;
        ctrl.label = "DEBUG: Kill current boss";
        ctrl.help_rml = "<p>Sets the current boss's HP to 0. Testing only.</p>";
        ctrl.on_pressed = on_kill_current_boss_pressed;
        svc_ui->pane_add_control(mod_ctx, left, &ctrl, nullptr);
    }
#endif

#if 0
    svc_ui->pane_add_section(mod_ctx, left, "Preset Save");
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>If a preset save is shipped, entering Boss Rush loads it. On exit your own save "
        "is reloaded. Create one in-game with the export button below.</p>", nullptr);
    {
        UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
        ctrl.kind = UI_CONTROL_BUTTON;
        ctrl.label = "Export current save as preset";
        ctrl.help_rml = "<p>Writes your current save to a file for use as the Boss Rush preset.</p>";
        ctrl.on_pressed = on_export_boss_rush_save_pressed;
        svc_ui->pane_add_control(mod_ctx, left, &ctrl, nullptr);
    }
#endif

    return MOD_OK;
}

static ModResult tab_controls(ModContext*, UiWindowHandle, UiElementHandle left,
                              UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_rml(mod_ctx, right,
        "<p>Remap the controller buttons of the mod's features. A binding is grayed out "
        "while its feature is disabled.</p>", nullptr);

    svc_ui->pane_add_section(mod_ctx, left, "Z Button Slot");
    ui_add_select(left, "Midna button", g_controlsVars[CTRL_BIND_MIDNA],
        "<p>While the Z Button Slot feature is enabled, Midna is called with this button "
        "instead of Z. The item slot itself always stays on Z.</p>",
        kControlsButtonLabels, CTRL_BTN_COUNT, is_z_slot_sub_disabled);

    svc_ui->pane_add_section(mod_ctx, left, "Quick Access");
    ui_add_select(left, "Quick Access button", g_controlsVars[CTRL_BIND_QUICK_ACCESS],
        "<p>Tap to use your quick item, hold to open the Quick Access menu.</p>",
        kControlsButtonLabels, CTRL_BTN_COUNT, is_quick_access_sub_disabled);

    svc_ui->pane_add_section(mod_ctx, left, "Bottle Quick Access");
    ui_add_select(left, "Bottles button", g_controlsVars[CTRL_BIND_BOTTLES],
        "<p>Tap to use the selected bottle, hold to open the bottle menu. Note: while bound "
        "to L, L no longer triggers targeting/shield.</p>",
        kControlsButtonLabels, CTRL_BTN_COUNT, is_bottles_sub_disabled);

    svc_ui->pane_add_section(mod_ctx, left, "Boss Rush");
    ui_add_select(left, "Retry button", g_controlsVars[CTRL_BIND_BOSSRUSH_RETRY],
        "<p>During a Boss Rush fight, press to restart the fight.</p>",
        kControlsButtonLabels, CTRL_BTN_COUNT, nullptr);

    return MOD_OK;
}

static const UiTabDesc s_modSettingsTabs[] = {
    { sizeof(UiTabDesc), "General",   tab_general,   nullptr, nullptr },
    { sizeof(UiTabDesc), "Quality of Life", tab_quality_of_life, nullptr, nullptr },
    { sizeof(UiTabDesc), "Combat",    tab_combat,    nullptr, nullptr },
    { sizeof(UiTabDesc), "Visuals",   tab_visuals,   nullptr, nullptr },
    { sizeof(UiTabDesc), "Quick Access", tab_quick_access, nullptr, nullptr },
    { sizeof(UiTabDesc), "Menus",     tab_menus,     nullptr, nullptr },
    { sizeof(UiTabDesc), "BossRush",  tab_boss_rush, nullptr, nullptr },
    { sizeof(UiTabDesc), "Controls",  tab_controls,  nullptr, nullptr },
};

static void on_open_mod_settings(ModContext*, void*) {
    if (!svc_ui) return;
    UiWindowDesc wd = UI_WINDOW_DESC_INIT;
    wd.tabs = s_modSettingsTabs;
    wd.tab_count = sizeof(s_modSettingsTabs) / sizeof(s_modSettingsTabs[0]);
    UiWindowHandle w = 0;
    svc_ui->window_push(mod_ctx, &wd, &w);
}

static UiMenuTabHandle s_menuTabTwilitEssentials = 0;

static const char* const kDiscordChannelUrl =
    "https://discord.com/channels/1491394561266679922/1534172217846403243";

extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void* hwnd, const char* op,
    const char* file, const char* params, const char* dir, int show);
#pragma comment(lib, "shell32.lib")

static void on_open_discord_channel(ModContext*, void*) {
    ShellExecuteA(nullptr, "open", kDiscordChannelUrl, nullptr, nullptr, 1 /* SW_SHOWNORMAL */);
}

static ModResult build_mod_ui_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    if (!svc_ui) return MOD_OK;

    {
        UiControlDesc ctrlSettings = UI_CONTROL_DESC_INIT;
        ctrlSettings.kind = UI_CONTROL_BUTTON;
        ctrlSettings.label = "Mod Settings...";
        ctrlSettings.on_pressed = on_open_mod_settings;
        svc_ui->pane_add_control(mod_ctx, panel, &ctrlSettings, nullptr);
    }

    {
        svc_ui->pane_add_text(mod_ctx, panel,
            "If you need help, found a bug, or would like to give feedback, please report it "
            "in the Twilit Essentials Discord channel:",
            nullptr);
    }
    {
        UiControlDesc ctrlDiscord = UI_CONTROL_DESC_INIT;
        ctrlDiscord.kind = UI_CONTROL_BUTTON;
        ctrlDiscord.label = "View channel in Discord";
        ctrlDiscord.on_pressed = on_open_discord_channel;
        svc_ui->pane_add_control(mod_ctx, panel, &ctrlDiscord, nullptr);
    }

    return MOD_OK;
}

static void log_init_result(const char* name, bool ok) {
    if (svc_log == nullptr) return;
    char msg[96];
    std::snprintf(msg, sizeof(msg), "init %s: %s", name, ok ? "OK" : "FAILED");
    svc_log->debug(mod_ctx, msg);
}

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if (svc_hook == nullptr) {
        return mods::set_error(error, MOD_ERROR, "HookService unavailable");
    }

    ensure_system_heap_capacity();
    if (svc_config) {
        ConfigVarDesc descGeneralSkipCut = CONFIG_VAR_DESC_INIT;
        descGeneralSkipCut.name = "generalSkipCutscenes";
        descGeneralSkipCut.type = CONFIG_VAR_BOOL;
        descGeneralSkipCut.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralSkipCut, &s_varGeneralSkipCutscenes) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralSkipCutscenes, &g_configGeneralSkipCutscenes);
            svc_config->subscribe(mod_ctx, s_varGeneralSkipCutscenes, on_general_skip_cutscenes_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralFfCut = CONFIG_VAR_DESC_INIT;
        descGeneralFfCut.name = "generalFastForwardCutscenes";
        descGeneralFfCut.type = CONFIG_VAR_BOOL;
        descGeneralFfCut.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralFfCut, &s_varGeneralFastForwardCutscenes) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralFastForwardCutscenes, &g_configGeneralFastForwardCutscenes);
            svc_config->subscribe(mod_ctx, s_varGeneralFastForwardCutscenes, on_general_fast_forward_cutscenes_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralDominionSword = CONFIG_VAR_DESC_INIT;
        descGeneralDominionSword.name = "generalDominionRodSword";
        descGeneralDominionSword.type = CONFIG_VAR_BOOL;
        descGeneralDominionSword.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralDominionSword, &s_varGeneralDominionSword) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralDominionSword, &g_configGeneralDominionSword);
            svc_config->subscribe(mod_ctx, s_varGeneralDominionSword, on_general_dominion_sword_changed, nullptr, nullptr);
        }

        ConfigVarDesc descHorseCamNoRecenter = CONFIG_VAR_DESC_INIT;
        descHorseCamNoRecenter.name = "horseCamNoRecenter";
        descHorseCamNoRecenter.type = CONFIG_VAR_BOOL;
        descHorseCamNoRecenter.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descHorseCamNoRecenter, &s_varHorseCamNoRecenter) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varHorseCamNoRecenter, &g_configHorseCamNoRecenter);
            svc_config->subscribe(mod_ctx, s_varHorseCamNoRecenter, on_horse_cam_no_recenter_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralHumanWarp = CONFIG_VAR_DESC_INIT;
        descGeneralHumanWarp.name = "generalHumanWarpAnimation";
        descGeneralHumanWarp.type = CONFIG_VAR_BOOL;
        descGeneralHumanWarp.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralHumanWarp, &s_varGeneralHumanWarp) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralHumanWarp, &g_configGeneralHumanWarpAnimation);
            svc_config->subscribe(mod_ctx, s_varGeneralHumanWarp, on_general_human_warp_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralFasterMidnaCancel = CONFIG_VAR_DESC_INIT;
        descGeneralFasterMidnaCancel.name = "generalFasterMidnaCancel";
        descGeneralFasterMidnaCancel.type = CONFIG_VAR_BOOL;
        descGeneralFasterMidnaCancel.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descGeneralFasterMidnaCancel, &s_varGeneralFasterMidnaCancel) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralFasterMidnaCancel, &g_configFasterMidnaCancel);
            svc_config->subscribe(mod_ctx, s_varGeneralFasterMidnaCancel, on_general_faster_midna_cancel_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralLockonLetterbox = CONFIG_VAR_DESC_INIT;
        descGeneralLockonLetterbox.name = "generalLockonLetterbox";
        descGeneralLockonLetterbox.type = CONFIG_VAR_BOOL;
        descGeneralLockonLetterbox.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralLockonLetterbox, &s_varGeneralLockonLetterbox) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralLockonLetterbox, &g_configLockonNoLetterbox);
            svc_config->subscribe(mod_ctx, s_varGeneralLockonLetterbox, on_general_lockon_letterbox_changed, nullptr, nullptr);
        }

        g_configHudAutoFadeIdleSeconds = kHudAutoFadeIdleSeconds;
        g_configHudAutoFadeFadeSeconds = kHudAutoFadeFadeSeconds;
        g_configHudAutoFadeRestAlpha = kHudAutoFadeRestAlpha;
        ConfigVarDesc descHudAutoFade = CONFIG_VAR_DESC_INIT;
        descHudAutoFade.name = "hudAutoFadeEnabled";
        descHudAutoFade.type = CONFIG_VAR_BOOL;
        descHudAutoFade.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descHudAutoFade, &s_varHudAutoFade) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varHudAutoFade, &g_configHudAutoFadeEnabled);
            svc_config->subscribe(mod_ctx, s_varHudAutoFade, on_hud_auto_fade_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralSprintFovKick = CONFIG_VAR_DESC_INIT;
        descGeneralSprintFovKick.name = "generalSprintFovKick";
        descGeneralSprintFovKick.type = CONFIG_VAR_BOOL;
        descGeneralSprintFovKick.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralSprintFovKick, &s_varGeneralSprintFovKick) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralSprintFovKick, &g_configSprintFovKickEnabled);
            svc_config->subscribe(mod_ctx, s_varGeneralSprintFovKick, on_general_sprint_fov_kick_changed, nullptr, nullptr);
        }

        ConfigVarDesc descDamageVignette = CONFIG_VAR_DESC_INIT;
        descDamageVignette.name = "damageVignetteEnabled";
        descDamageVignette.type = CONFIG_VAR_BOOL;
        descDamageVignette.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descDamageVignette, &s_varDamageVignette) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varDamageVignette, &g_configDamageVignetteEnabled);
            svc_config->subscribe(mod_ctx, s_varDamageVignette, on_damage_vignette_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralDrowningVignette = CONFIG_VAR_DESC_INIT;
        descGeneralDrowningVignette.name = "oxygenVignetteEnabled";
        descGeneralDrowningVignette.type = CONFIG_VAR_BOOL;
        descGeneralDrowningVignette.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descGeneralDrowningVignette, &s_varGeneralDrowningVignette) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varGeneralDrowningVignette, &g_configOxygenVignetteEnabled);
            svc_config->subscribe(mod_ctx, s_varGeneralDrowningVignette, on_oxygen_vignette_changed, nullptr, nullptr);
        }

        ConfigVarDesc descDamageVignetteIntensity = CONFIG_VAR_DESC_INIT;
        descDamageVignetteIntensity.name = "damageVignetteIntensity";
        descDamageVignetteIntensity.type = CONFIG_VAR_INT;
        descDamageVignetteIntensity.default_int = 50;
        if (svc_config->register_var(mod_ctx, &descDamageVignetteIntensity, &s_varDamageVignetteIntensity) == MOD_OK) {
            int64_t intensity = 50;
            svc_config->get_int(mod_ctx, s_varDamageVignetteIntensity, &intensity);
            g_configDamageVignetteIntensity = static_cast<int>(intensity);
            svc_config->subscribe(mod_ctx, s_varDamageVignetteIntensity, on_damage_vignette_intensity_changed, nullptr, nullptr);
        }

        ConfigVarDesc descGeneralSceneTransitions = CONFIG_VAR_DESC_INIT;
        descGeneralSceneTransitions.name = "generalSceneTransitions";
        descGeneralSceneTransitions.type = CONFIG_VAR_INT;
        descGeneralSceneTransitions.default_int = 0;
        if (svc_config->register_var(mod_ctx, &descGeneralSceneTransitions, &s_varGeneralSceneTransitions) == MOD_OK) {
            int64_t mode = 0;
            svc_config->get_int(mod_ctx, s_varGeneralSceneTransitions, &mode);
            g_configFasterTransitions = mode == 0;
            svc_config->subscribe(mod_ctx, s_varGeneralSceneTransitions, on_general_scene_transitions_changed, nullptr, nullptr);
        }


        ConfigVarDesc descHp = CONFIG_VAR_DESC_INIT;
        descHp.name = "hpBarsEnabled";
        descHp.type = CONFIG_VAR_BOOL;
        descHp.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descHp, &s_varHpBars) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varHpBars, &g_configHpBarsEnabled);
            svc_config->subscribe(mod_ctx, s_varHpBars, on_hp_bars_changed, nullptr, nullptr);
        }

        ConfigVarDesc descHpNum = CONFIG_VAR_DESC_INIT;
        descHpNum.name = "hpBarsShowNumbers";
        descHpNum.type = CONFIG_VAR_BOOL;
        descHpNum.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descHpNum, &s_varHpBarsShowNumbers) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varHpBarsShowNumbers, &g_configHpBarsShowNumbers);
            svc_config->subscribe(mod_ctx, s_varHpBarsShowNumbers, on_hp_bars_show_numbers_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossBar = CONFIG_VAR_DESC_INIT;
        descBossBar.name = "bossBarEnabled";
        descBossBar.type = CONFIG_VAR_BOOL;
        descBossBar.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossBar, &s_varBossBar) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossBar, &g_configBossBarEnabled);
            svc_config->subscribe(mod_ctx, s_varBossBar, on_boss_bar_changed, nullptr, nullptr);
        }

        ConfigVarDesc descDmg = CONFIG_VAR_DESC_INIT;
        descDmg.name = "damageNumbersEnabled";
        descDmg.type = CONFIG_VAR_BOOL;
        descDmg.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descDmg, &s_varDamageNumbers) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varDamageNumbers, &g_configDamageNumbersEnabled);
            svc_config->subscribe(mod_ctx, s_varDamageNumbers, on_damage_numbers_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquip = CONFIG_VAR_DESC_INIT;
        descEquip.name = "visibleEquipmentEnabled";
        descEquip.type = CONFIG_VAR_BOOL;
        descEquip.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descEquip, &s_varVisibleEquip) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varVisibleEquip, &g_configVisibleEquipmentEnabled);
            svc_config->subscribe(mod_ctx, s_varVisibleEquip, on_visible_equip_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquipMode = CONFIG_VAR_DESC_INIT;
        descEquipMode.name = "visibleEquipmentAlwaysShowUnlocked";
        descEquipMode.type = CONFIG_VAR_BOOL;
        descEquipMode.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descEquipMode, &s_varVisibleEquipMode) == MOD_OK) {
            bool modeBool = true;
            svc_config->get_bool(mod_ctx, s_varVisibleEquipMode, &modeBool);
            g_configVisibleEquipDisplayMode = modeBool ? 1 : 0;
            svc_config->subscribe(mod_ctx, s_varVisibleEquipMode, on_visible_equip_mode_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquipMirror = CONFIG_VAR_DESC_INIT;
        descEquipMirror.name = "visibleEquipmentMirrorBow";
        descEquipMirror.type = CONFIG_VAR_BOOL;
        descEquipMirror.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descEquipMirror, &s_varVisibleEquipMirrorBow) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varVisibleEquipMirrorBow, &g_configVisibleEquipMirrorBow);
            svc_config->subscribe(mod_ctx, s_varVisibleEquipMirrorBow, on_visible_equip_mirror_bow_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquipBow = CONFIG_VAR_DESC_INIT;
        descEquipBow.name = "visibleEquipmentShowBow";
        descEquipBow.type = CONFIG_VAR_BOOL;
        descEquipBow.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descEquipBow, &s_varVisibleEquipShowBow) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varVisibleEquipShowBow, &g_configVisibleEquipShowBow);
            svc_config->subscribe(mod_ctx, s_varVisibleEquipShowBow, on_visible_equip_show_bow_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquipLantern = CONFIG_VAR_DESC_INIT;
        descEquipLantern.name = "visibleEquipmentShowLantern";
        descEquipLantern.type = CONFIG_VAR_BOOL;
        descEquipLantern.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descEquipLantern, &s_varVisibleEquipShowLantern) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varVisibleEquipShowLantern, &g_configVisibleEquipShowLantern);
            svc_config->subscribe(mod_ctx, s_varVisibleEquipShowLantern, on_visible_equip_show_lantern_changed, nullptr, nullptr);
        }

        ConfigVarDesc descEquipQuiverBelt = CONFIG_VAR_DESC_INIT;
        descEquipQuiverBelt.name = "visibleEquipmentQuiverOnBelt";
        descEquipQuiverBelt.type = CONFIG_VAR_BOOL;
        descEquipQuiverBelt.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descEquipQuiverBelt, &s_varVisibleEquipQuiverOnBelt) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varVisibleEquipQuiverOnBelt, &g_configVisibleEquipQuiverOnBelt);
            svc_config->subscribe(mod_ctx, s_varVisibleEquipQuiverOnBelt, on_visible_equip_quiver_on_belt_changed, nullptr, nullptr);
        }

        ConfigVarDesc descZ = CONFIG_VAR_DESC_INIT;
        descZ.name = "customZButtonEnabled";
        descZ.type = CONFIG_VAR_BOOL;
        descZ.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descZ, &s_varCustomZButton) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varCustomZButton, &g_configCustomZButtonEnabled);
            if (isNativeZButtonEngine()) {
                g_configCustomZButtonEnabled = false;
            }
            g_configZButtonEnabled = g_configCustomZButtonEnabled;
            svc_config->subscribe(mod_ctx, s_varCustomZButton, on_custom_z_button_changed, nullptr, nullptr);
        }

        ConfigVarDesc descQuickAccess = CONFIG_VAR_DESC_INIT;
        descQuickAccess.name = "quickAccessEnabled";
        descQuickAccess.type = CONFIG_VAR_BOOL;
        descQuickAccess.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descQuickAccess, &s_varQuickAccess) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varQuickAccess, &g_configQuickAccessEnabled);
            svc_config->subscribe(mod_ctx, s_varQuickAccess, on_quick_access_changed, nullptr, nullptr);
        }

        ConfigVarDesc descQuickAccessAppearance = CONFIG_VAR_DESC_INIT;
        descQuickAccessAppearance.name = "quickAccessAppearance";
        descQuickAccessAppearance.type = CONFIG_VAR_INT;
        descQuickAccessAppearance.default_int = 0;
        if (svc_config->register_var(mod_ctx, &descQuickAccessAppearance, &s_varQuickAccessAppearance) == MOD_OK) {
            int64_t appearance = 0;
            svc_config->get_int(mod_ctx, s_varQuickAccessAppearance, &appearance);
            g_configQuickAccessAppearance = static_cast<int>(appearance);
            svc_config->subscribe(mod_ctx, s_varQuickAccessAppearance, on_quick_access_appearance_changed, nullptr, nullptr);
        }

        ConfigVarDesc descQuickAccessHideWheel = CONFIG_VAR_DESC_INIT;
        descQuickAccessHideWheel.name = "quickAccessHideWheelItems";
        descQuickAccessHideWheel.type = CONFIG_VAR_BOOL;
        descQuickAccessHideWheel.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descQuickAccessHideWheel, &s_varQuickAccessHideWheelItems) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varQuickAccessHideWheelItems, &g_configQuickAccessHideWheelItems);
            svc_config->subscribe(mod_ctx, s_varQuickAccessHideWheelItems, on_quick_access_hide_wheel_items_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBottlesQuickAccess = CONFIG_VAR_DESC_INIT;
        descBottlesQuickAccess.name = "bottlesQuickAccessEnabled";
        descBottlesQuickAccess.type = CONFIG_VAR_BOOL;
        descBottlesQuickAccess.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBottlesQuickAccess, &s_varBottlesQuickAccess) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBottlesQuickAccess, &g_configBottlesQuickAccessEnabled);
            svc_config->subscribe(mod_ctx, s_varBottlesQuickAccess, on_bottles_quick_access_changed, nullptr, nullptr);
        }

        ConfigVarDesc descSpin = CONFIG_VAR_DESC_INIT;
        descSpin.name = "sheathedSpinEnabled";
        descSpin.type = CONFIG_VAR_BOOL;
        descSpin.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descSpin, &s_varSheathedSpin) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varSheathedSpin, &g_configSheathedSpinEnabled);
            svc_config->subscribe(mod_ctx, s_varSheathedSpin, on_sheathed_spin_changed, nullptr, nullptr);
        }

        ConfigVarDesc descFlurryRush = CONFIG_VAR_DESC_INIT;
        descFlurryRush.name = "flurryRushEnabled";
        descFlurryRush.type = CONFIG_VAR_BOOL;
        descFlurryRush.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descFlurryRush, &s_varFlurryRush) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varFlurryRush, &g_configFlurryRushEnabled);
            svc_config->subscribe(mod_ctx, s_varFlurryRush, on_flurry_rush_changed, nullptr, nullptr);
        }

        ConfigVarDesc descFlurryRushSlowFactor = CONFIG_VAR_DESC_INIT;
        descFlurryRushSlowFactor.name = "flurryRushSlowFactor";
        descFlurryRushSlowFactor.type = CONFIG_VAR_INT;
        descFlurryRushSlowFactor.default_int = 30;
        if (svc_config->register_var(mod_ctx, &descFlurryRushSlowFactor, &s_varFlurryRushSlowFactor) == MOD_OK) {
            int64_t v = 30;
            svc_config->get_int(mod_ctx, s_varFlurryRushSlowFactor, &v);
            if (v < 5) v = 5;
            if (v > 80) v = 80;
            g_configFlurryRushSlowFactor = static_cast<int>(v);
            svc_config->subscribe(mod_ctx, s_varFlurryRushSlowFactor, on_flurry_rush_slow_factor_changed, nullptr, nullptr);
        }

        ConfigVarDesc descFlurryRushWindow = CONFIG_VAR_DESC_INIT;
        descFlurryRushWindow.name = "flurryRushWindow";
        descFlurryRushWindow.type = CONFIG_VAR_INT;
        descFlurryRushWindow.default_int = 5;
        if (svc_config->register_var(mod_ctx, &descFlurryRushWindow, &s_varFlurryRushWindow) == MOD_OK) {
            int64_t v = 5;
            svc_config->get_int(mod_ctx, s_varFlurryRushWindow, &v);
            if (v < 1) v = 1;
            if (v > 10) v = 10;
            g_configFlurryRushWindowTicks = static_cast<int>(v * 30);
            svc_config->subscribe(mod_ctx, s_varFlurryRushWindow, on_flurry_rush_window_changed, nullptr, nullptr);
        }

        ConfigVarDesc descFlurryRushHits = CONFIG_VAR_DESC_INIT;
        descFlurryRushHits.name = "flurryRushHits";
        descFlurryRushHits.type = CONFIG_VAR_INT;
        descFlurryRushHits.default_int = 4;
        if (svc_config->register_var(mod_ctx, &descFlurryRushHits, &s_varFlurryRushHits) == MOD_OK) {
            int64_t v = 4;
            svc_config->get_int(mod_ctx, s_varFlurryRushHits, &v);
            if (v < 1) v = 1;
            if (v > 8) v = 8;
            g_configFlurryRushHits = static_cast<int>(v);
            svc_config->subscribe(mod_ctx, s_varFlurryRushHits, on_flurry_rush_hits_changed, nullptr, nullptr);
        }

        ConfigVarDesc descFlurryRushPerfectFrames = CONFIG_VAR_DESC_INIT;
        descFlurryRushPerfectFrames.name = "flurryRushPerfectFrames";
        descFlurryRushPerfectFrames.type = CONFIG_VAR_INT;
        descFlurryRushPerfectFrames.default_int = 30;
        if (svc_config->register_var(mod_ctx, &descFlurryRushPerfectFrames, &s_varFlurryRushPerfectFrames) == MOD_OK) {
            int64_t v = 30;
            svc_config->get_int(mod_ctx, s_varFlurryRushPerfectFrames, &v);
            if (v < 5) v = 5;
            if (v > 120) v = 120;
            g_configFlurryRushPerfectFrames = static_cast<int>(v);
            svc_config->subscribe(mod_ctx, s_varFlurryRushPerfectFrames, on_flurry_rush_perfect_frames_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStamina = CONFIG_VAR_DESC_INIT;
        descStamina.name = "staminaEnabled";
        descStamina.type = CONFIG_VAR_BOOL;
        descStamina.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descStamina, &s_varStamina) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varStamina, &g_configStaminaEnabled);
            svc_config->subscribe(mod_ctx, s_varStamina, on_stamina_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaMax = CONFIG_VAR_DESC_INIT;
        descStaminaMax.name = "staminaMax";
        descStaminaMax.type = CONFIG_VAR_INT;
        descStaminaMax.default_int = 100;
        if (svc_config->register_var(mod_ctx, &descStaminaMax, &s_varStaminaMax) == MOD_OK) {
            int64_t v = 100;
            svc_config->get_int(mod_ctx, s_varStaminaMax, &v);
            g_configStaminaMax = static_cast<int>(v);
            svc_config->subscribe(mod_ctx, s_varStaminaMax, on_stamina_max_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaRegen = CONFIG_VAR_DESC_INIT;
        descStaminaRegen.name = "staminaRegen";
        descStaminaRegen.type = CONFIG_VAR_INT;
        descStaminaRegen.default_int = 100;
        if (svc_config->register_var(mod_ctx, &descStaminaRegen, &s_varStaminaRegen) == MOD_OK) {
            int64_t v = 100;
            svc_config->get_int(mod_ctx, s_varStaminaRegen, &v);
            g_configStaminaRegen = static_cast<int>(v);
            svc_config->subscribe(mod_ctx, s_varStaminaRegen, on_stamina_regen_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaSprint = CONFIG_VAR_DESC_INIT;
        descStaminaSprint.name = "staminaSprint";
        descStaminaSprint.type = CONFIG_VAR_BOOL;
        descStaminaSprint.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descStaminaSprint, &s_varStaminaSprint) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varStaminaSprint, &g_configStaminaSprint);
            svc_config->subscribe(mod_ctx, s_varStaminaSprint, on_stamina_sprint_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaWolfSprint = CONFIG_VAR_DESC_INIT;
        descStaminaWolfSprint.name = "staminaWolfSprint";
        descStaminaWolfSprint.type = CONFIG_VAR_BOOL;
        descStaminaWolfSprint.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descStaminaWolfSprint, &s_varStaminaWolfSprint) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varStaminaWolfSprint, &g_configStaminaWolfSprint);
            svc_config->subscribe(mod_ctx, s_varStaminaWolfSprint, on_stamina_wolf_sprint_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaSprintSpeed = CONFIG_VAR_DESC_INIT;
        descStaminaSprintSpeed.name = "staminaSprintSpeed";
        descStaminaSprintSpeed.type = CONFIG_VAR_INT;
        descStaminaSprintSpeed.default_int = 110;
        if (svc_config->register_var(mod_ctx, &descStaminaSprintSpeed, &s_varStaminaSprintSpeed) == MOD_OK) {
            int64_t v = 110;
            svc_config->get_int(mod_ctx, s_varStaminaSprintSpeed, &v);
            if (v < 100) v = 100;
            if (v > 200) v = 200;
            g_configStaminaSprintSpeed = static_cast<float>(v) / 100.0f;
            svc_config->subscribe(mod_ctx, s_varStaminaSprintSpeed, on_stamina_sprint_speed_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaWolfSprintSpeed = CONFIG_VAR_DESC_INIT;
        descStaminaWolfSprintSpeed.name = "staminaWolfSprintSpeed";
        descStaminaWolfSprintSpeed.type = CONFIG_VAR_INT;
        descStaminaWolfSprintSpeed.default_int = 110;
        if (svc_config->register_var(mod_ctx, &descStaminaWolfSprintSpeed, &s_varStaminaWolfSprintSpeed) == MOD_OK) {
            int64_t v = 110;
            svc_config->get_int(mod_ctx, s_varStaminaWolfSprintSpeed, &v);
            if (v < 100) v = 100;
            if (v > 200) v = 200;
            g_configStaminaWolfSprintSpeed = static_cast<float>(v) / 100.0f;
            svc_config->subscribe(mod_ctx, s_varStaminaWolfSprintSpeed, on_stamina_wolf_sprint_speed_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaSwimSprint = CONFIG_VAR_DESC_INIT;
        descStaminaSwimSprint.name = "staminaSwimSprint";
        descStaminaSwimSprint.type = CONFIG_VAR_BOOL;
        descStaminaSwimSprint.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descStaminaSwimSprint, &s_varStaminaSwimSprint) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varStaminaSwimSprint, &g_configStaminaSwimSprint);
            svc_config->subscribe(mod_ctx, s_varStaminaSwimSprint, on_stamina_swim_sprint_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStaminaSwimSprintSpeed = CONFIG_VAR_DESC_INIT;
        descStaminaSwimSprintSpeed.name = "staminaSwimSprintSpeed";
        descStaminaSwimSprintSpeed.type = CONFIG_VAR_INT;
        descStaminaSwimSprintSpeed.default_int = 115;
        if (svc_config->register_var(mod_ctx, &descStaminaSwimSprintSpeed, &s_varStaminaSwimSprintSpeed) == MOD_OK) {
            int64_t v = 115;
            svc_config->get_int(mod_ctx, s_varStaminaSwimSprintSpeed, &v);
            if (v < 100) v = 100;
            if (v > 200) v = 200;
            g_configStaminaSwimSprintSpeed = static_cast<float>(v) / 100.0f;
            svc_config->subscribe(mod_ctx, s_varStaminaSwimSprintSpeed, on_stamina_swim_sprint_speed_changed, nullptr, nullptr);
        }

        struct { const char* name; ConfigVarHandle* handle; bool* global; ConfigChangedFn cb; } staminaSrcVars[] = {
            { "staminaSrcAttacks",  &s_varStaminaSrcAttacks,  &g_configStaminaSrcAttacks,  on_stamina_src_attacks_changed },
            { "staminaSrcJumpSpin", &s_varStaminaSrcJumpSpin, &g_configStaminaSrcJumpSpin, on_stamina_src_jump_spin_changed },
            { "staminaSrcRolls",    &s_varStaminaSrcRolls,    &g_configStaminaSrcRolls,    on_stamina_src_rolls_changed },
            { "staminaSrcSprint",   &s_varStaminaSrcSprint,   &g_configStaminaSrcSprint,   on_stamina_src_sprint_changed },
            { "staminaSrcClimb",    &s_varStaminaSrcClimb,    &g_configStaminaSrcClimb,    on_stamina_src_climb_changed },
            { "staminaSrcHang",     &s_varStaminaSrcHang,     &g_configStaminaSrcHang,     on_stamina_src_hang_changed },
            { "staminaSrcSwim",     &s_varStaminaSrcSwim,     &g_configStaminaSrcSwim,     on_stamina_src_swim_changed },
            { "staminaSrcPushPull", &s_varStaminaSrcPushPull, &g_configStaminaSrcPushPull, on_stamina_src_push_pull_changed },
            { "staminaSrcWolfDash", &s_varStaminaSrcWolfDash, &g_configStaminaSrcWolfDash, on_stamina_src_wolf_dash_changed },
            { "staminaSrcHiddenSkills", &s_varStaminaSrcHiddenSkills, &g_configStaminaSrcHiddenSkills, on_stamina_src_hidden_skills_changed },
        };
        for (auto& sv : staminaSrcVars) {
            ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
            d.name = sv.name;
            d.type = CONFIG_VAR_BOOL;
            d.default_bool = true;
            if (svc_config->register_var(mod_ctx, &d, sv.handle) == MOD_OK) {
                svc_config->get_bool(mod_ctx, *sv.handle, sv.global);
                svc_config->subscribe(mod_ctx, *sv.handle, sv.cb, nullptr, nullptr);
            }
        }

        struct StaminaCostVar { const char* name; ConfigVarHandle* handle; int* global; };
        const StaminaCostVar staminaCostVars[] = {
            { "staminaCostAttack",     &s_varStaminaCostAttack,     &g_configStaminaCostAttack },
            { "staminaCostJumpAttack", &s_varStaminaCostJumpAttack, &g_configStaminaCostJumpAttack },
            { "staminaCostSpin",       &s_varStaminaCostSpin,       &g_configStaminaCostSpin },
            { "staminaCostRoll",       &s_varStaminaCostRoll,       &g_configStaminaCostRoll },
            { "staminaCostSidestep",   &s_varStaminaCostSidestep,   &g_configStaminaCostSidestep },
            { "staminaCostClimb",      &s_varStaminaCostClimb,      &g_configStaminaCostClimb },
            { "staminaCostHang",       &s_varStaminaCostHang,       &g_configStaminaCostHang },
            { "staminaCostCrawl",      &s_varStaminaCostCrawl,      &g_configStaminaCostCrawl },
            { "staminaCostSwim",       &s_varStaminaCostSwim,       &g_configStaminaCostSwim },
            { "staminaCostPushPull",   &s_varStaminaCostPushPull,   &g_configStaminaCostPushPull },
            { "staminaCostWolfDash",   &s_varStaminaCostWolfDash,   &g_configStaminaCostWolfDash },
            { "staminaCostSprint",     &s_varStaminaCostSprint,     &g_configStaminaCostSprint },
            { "staminaCostWolfSprint", &s_varStaminaCostWolfSprint, &g_configStaminaCostWolfSprint },
            { "staminaCostSwimSprint", &s_varStaminaCostSwimSprint, &g_configStaminaCostSwimSprint },
            { "staminaCostHiddenSkills", &s_varStaminaCostHiddenSkills, &g_configStaminaCostHiddenSkills },
        };
        for (auto& cv : staminaCostVars) {
            ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
            d.name = cv.name;
            d.type = CONFIG_VAR_INT;
            d.default_int = 100;
            if (svc_config->register_var(mod_ctx, &d, cv.handle) == MOD_OK) {
                int64_t v = 100;
                svc_config->get_int(mod_ctx, *cv.handle, &v);
                *cv.global = static_cast<int>(v);
                svc_config->subscribe(mod_ctx, *cv.handle, on_stamina_cost_changed, cv.global, nullptr);
            }
        }

        ConfigVarDesc descZelda = CONFIG_VAR_DESC_INIT;
        descZelda.name = "puppetZeldaPatternEnabled";
        descZelda.type = CONFIG_VAR_BOOL;
        descZelda.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descZelda, &s_varPuppetZeldaPattern) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varPuppetZeldaPattern, &g_configPuppetZeldaPatternEnabled);
            svc_config->subscribe(mod_ctx, s_varPuppetZeldaPattern, on_puppet_zelda_pattern_changed, nullptr, nullptr);
        }

        ConfigVarDesc descZeldaShortest = CONFIG_VAR_DESC_INIT;
        descZeldaShortest.name = "puppetZeldaAlwaysShortest";
        descZeldaShortest.type = CONFIG_VAR_BOOL;
        descZeldaShortest.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descZeldaShortest, &s_varPuppetZeldaAlwaysShortest) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varPuppetZeldaAlwaysShortest, &g_configPuppetZeldaAlwaysShortest);
            svc_config->subscribe(mod_ctx, s_varPuppetZeldaAlwaysShortest, on_puppet_zelda_always_shortest_changed, nullptr, nullptr);
        }

        ConfigVarDesc descStarter = CONFIG_VAR_DESC_INIT;
        descStarter.name = "collectionStarterEquip";
        descStarter.type = CONFIG_VAR_BOOL;
        descStarter.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descStarter, &s_varCollectionStarterEquip) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varCollectionStarterEquip, &g_configCollectionStarterEquip);
            svc_config->subscribe(mod_ctx, s_varCollectionStarterEquip, on_collection_starter_equip_changed, nullptr, nullptr);
        }

        ConfigVarDesc descKeepShield = CONFIG_VAR_DESC_INIT;
        descKeepShield.name = "collectionKeepOrdonShield";
        descKeepShield.type = CONFIG_VAR_BOOL;
        descKeepShield.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descKeepShield, &s_varCollectionKeepOrdonShield) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varCollectionKeepOrdonShield, &g_configCollectionKeepOrdonShield);
            svc_config->subscribe(mod_ctx, s_varCollectionKeepOrdonShield, on_collection_keep_ordon_shield_changed, nullptr, nullptr);
        }

        ConfigVarDesc descShowOrdonHero = CONFIG_VAR_DESC_INIT;
        descShowOrdonHero.name = "collectionShowOrdonHero";
        descShowOrdonHero.type = CONFIG_VAR_BOOL;
        descShowOrdonHero.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descShowOrdonHero, &s_varCollectionShowOrdonHero) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varCollectionShowOrdonHero, &g_configCollectionShowOrdonHero);
            svc_config->subscribe(mod_ctx, s_varCollectionShowOrdonHero, on_collection_show_ordon_hero_changed, nullptr, nullptr);
        }

        ConfigVarDesc descOrdonHeroAlways = CONFIG_VAR_DESC_INIT;
        descOrdonHeroAlways.name = "collectionOrdonHeroAlways";
        descOrdonHeroAlways.type = CONFIG_VAR_BOOL;
        descOrdonHeroAlways.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descOrdonHeroAlways, &s_varCollectionOrdonHeroAlways) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varCollectionOrdonHeroAlways, &g_configCollectionOrdonHeroAlways);
            svc_config->subscribe(mod_ctx, s_varCollectionOrdonHeroAlways, on_collection_ordon_hero_always_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushSuggested = CONFIG_VAR_DESC_INIT;
        descBossRushSuggested.name = "bossRushSuggestedItems";
        descBossRushSuggested.type = CONFIG_VAR_BOOL;
        descBossRushSuggested.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossRushSuggested, &s_varBossRushSuggestedItems) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushSuggestedItems, &g_configBossRushSuggestedItems);
            svc_config->subscribe(mod_ctx, s_varBossRushSuggestedItems, on_boss_rush_suggested_items_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushRefill = CONFIG_VAR_DESC_INIT;
        descBossRushRefill.name = "bossRushRefillAfterFight";
        descBossRushRefill.type = CONFIG_VAR_BOOL;
        descBossRushRefill.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossRushRefill, &s_varBossRushRefillAfterFight) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushRefillAfterFight, &g_configBossRushRefillAfterFight);
            svc_config->subscribe(mod_ctx, s_varBossRushRefillAfterFight, on_boss_rush_refill_after_fight_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushSeparateGanon = CONFIG_VAR_DESC_INIT;
        descBossRushSeparateGanon.name = "bossRushSeparateGanon";
        descBossRushSeparateGanon.type = CONFIG_VAR_BOOL;
        descBossRushSeparateGanon.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descBossRushSeparateGanon, &s_varBossRushSeparateGanon) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushSeparateGanon, &g_configBossRushSeparateGanon);
            svc_config->subscribe(mod_ctx, s_varBossRushSeparateGanon, on_boss_rush_separate_ganon_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushVanillaGear = CONFIG_VAR_DESC_INIT;
        descBossRushVanillaGear.name = "bossRushVanillaGear";
        descBossRushVanillaGear.type = CONFIG_VAR_BOOL;
        descBossRushVanillaGear.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossRushVanillaGear, &s_varBossRushVanillaGear) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushVanillaGear, &g_configBossRushVanillaGear);
            svc_config->subscribe(mod_ctx, s_varBossRushVanillaGear, on_boss_rush_vanilla_gear_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushTimer = CONFIG_VAR_DESC_INIT;
        descBossRushTimer.name = "bossRushTimer";
        descBossRushTimer.type = CONFIG_VAR_BOOL;
        descBossRushTimer.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossRushTimer, &s_varBossRushTimer) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushTimer, &g_configBossRushTimer);
            svc_config->subscribe(mod_ctx, s_varBossRushTimer, on_boss_rush_timer_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushShowBestTimer = CONFIG_VAR_DESC_INIT;
        descBossRushShowBestTimer.name = "bossRushShowBestTimer";
        descBossRushShowBestTimer.type = CONFIG_VAR_BOOL;
        descBossRushShowBestTimer.default_bool = true;
        if (svc_config->register_var(mod_ctx, &descBossRushShowBestTimer, &s_varBossRushShowBestTimer) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushShowBestTimer, &g_configBossRushShowBestTimer);
            svc_config->subscribe(mod_ctx, s_varBossRushShowBestTimer, on_boss_rush_show_best_timer_changed, nullptr, nullptr);
        }

        ConfigVarDesc descBossRushBestTimes = CONFIG_VAR_DESC_INIT;
        descBossRushBestTimes.name = "bossRushBestTimes";
        descBossRushBestTimes.type = CONFIG_VAR_STRING;
        descBossRushBestTimes.default_string = "";
        svc_config->register_var(mod_ctx, &descBossRushBestTimes, &s_varBossRushBestTimes);
        boss_rush_timer_init(svc_config, mod_ctx, s_varBossRushBestTimes);

        ConfigVarDesc descBossRushChainBest = CONFIG_VAR_DESC_INIT;
        descBossRushChainBest.name = "bossRushChainBest";
        descBossRushChainBest.type = CONFIG_VAR_STRING;
        descBossRushChainBest.default_string = "";
        svc_config->register_var(mod_ctx, &descBossRushChainBest, &s_varBossRushChainBest);
        boss_rush_timer_init_chain_best(svc_config, mod_ctx, s_varBossRushChainBest);

        ConfigVarDesc descBossRushPortal = CONFIG_VAR_DESC_INIT;
        descBossRushPortal.name = "bossRushPortal";
        descBossRushPortal.type = CONFIG_VAR_BOOL;
        descBossRushPortal.default_bool = false;
        if (svc_config->register_var(mod_ctx, &descBossRushPortal, &s_varBossRushPortal) == MOD_OK) {
            svc_config->get_bool(mod_ctx, s_varBossRushPortal, &g_configBossRushPortal);
            svc_config->subscribe(mod_ctx, s_varBossRushPortal, on_boss_rush_portal_changed, nullptr, nullptr);
        }

        init_controls_config(svc_config, mod_ctx);
    }


    if (svc_ui) {
        UiModsPanelDesc panelDesc = UI_MODS_PANEL_DESC_INIT;
        panelDesc.build = build_mod_ui_panel;
        svc_ui->register_mods_panel(mod_ctx, &panelDesc);

        UiMenuTabDesc menuTabDesc = UI_MENU_TAB_DESC_INIT;
        menuTabDesc.label = "Twilit Essentials";
        menuTabDesc.on_selected = on_open_mod_settings;
        if (svc_ui->register_menu_tab(mod_ctx, &menuTabDesc, &s_menuTabTwilitEssentials) == MOD_OK) {
            log_init_result("menu_tab", true);
        } else {
            s_menuTabTwilitEssentials = 0;
            log_init_result("menu_tab", false);
        }
    }

    if (svc_hook) {
        mods::hook::add_post<TitleFastLogoDispInitHook>(svc_hook, on_title_fast_logo_disp_init_post);
        mods::hook::add_pre<TitleFastLogoDispExecuteHook>(svc_hook, on_title_fast_logo_disp_pre);
    }

    s_generalInitialized = init_general(svc_hook, error) == MOD_OK;
    log_init_result("general", s_generalInitialized);
    s_damageVignetteInitialized =
        init_damage_vignette(svc_gfx, svc_resource, svc_log, mod_ctx, error) == MOD_OK;
    log_init_result("damage_vignette", s_damageVignetteInitialized);
    s_oxygenVignetteInitialized =
        init_oxygen_vignette(svc_gfx, svc_resource, svc_log, mod_ctx, error) == MOD_OK;
    log_init_result("oxygen_vignette", s_oxygenVignetteInitialized);
    s_flurryVignetteInitialized =
        init_flurry_vignette(svc_gfx, svc_resource, svc_log, mod_ctx, error) == MOD_OK;
    log_init_result("flurry_vignette", s_flurryVignetteInitialized);
    init_midna_select_freeze_guard(svc_hook, error);
    //init_free_camera_toggle(svc_hook);
    s_hpBarsInitialized = init_hp_bars(svc_hook, error) == MOD_OK;
    log_init_result("hp_bars", s_hpBarsInitialized);
    s_bossBarInitialized = init_boss_bar(svc_hook, error) == MOD_OK;
    log_init_result("boss_bar", s_bossBarInitialized);
    s_bossRushInitialized = init_boss_rush(svc_hook, svc_log, svc_ui, svc_config, mod_ctx, error) == MOD_OK;
    log_init_result("boss_rush", s_bossRushInitialized);
    s_bossRushPortalInitialized = init_boss_rush_portal(svc_hook, svc_log, mod_ctx) == MOD_OK;
    log_init_result("boss_rush_portal", s_bossRushPortalInitialized);
    log_init_result("boss_rush_dpad", init_boss_rush_dpad(svc_hook, svc_log, mod_ctx) == MOD_OK);
    s_visibleEquipmentInitialized = init_visible_equipment(svc_hook, error) == MOD_OK;
    log_init_result("visible_equipment", s_visibleEquipmentInitialized);
    s_zButtonInitialized = init_z_button(svc_hook, svc_log, mod_ctx, error) == MOD_OK;
    log_init_result("z_button", s_zButtonInitialized);
    log_init_result("quick_access_bottles", init_quick_access_bottles(svc_hook, svc_save, mod_ctx, error) == MOD_OK);
    s_quickAccessInitialized = init_quick_access(svc_hook, svc_save, mod_ctx, error) == MOD_OK;
    log_init_result("quick_access", s_quickAccessInitialized);
    log_init_result("quick_access_itemwheel", init_quick_access_itemwheel(svc_hook, error) == MOD_OK);
    s_sheathedSpinInitialized = init_sheathed_spin(svc_hook) == MOD_OK;
    log_init_result("sheathed_spin", s_sheathedSpinInitialized);
    s_flurryRushInitialized = init_flurry_rush(svc_hook, svc_log, error) == MOD_OK;
    log_init_result("flurry_rush", s_flurryRushInitialized);
    s_staminaInitialized = init_stamina(svc_hook, error) == MOD_OK;
    log_init_result("stamina", s_staminaInitialized);
    s_puppetZeldaPatternInitialized = init_puppet_zelda_pattern(svc_hook, error) == MOD_OK;
    log_init_result("puppet_zelda_pattern", s_puppetZeldaPatternInitialized);
    s_collectionMenuInitialized = init_collection_menu(svc_hook, svc_log, svc_save, mod_ctx, error) == MOD_OK;
    log_init_result("collection_menu", s_collectionMenuInitialized);
    s_collectionMenuChestInitialized = init_collection_menu_chest(svc_hook, svc_log, mod_ctx, error) == MOD_OK;
    log_init_result("collection_menu_shield", s_collectionMenuChestInitialized);

    s_titleModActive = true;
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    if (s_generalInitialized) update_general(svc_log, mod_ctx);
    if (s_damageVignetteInitialized) update_damage_vignette(svc_log, mod_ctx);
    if (s_oxygenVignetteInitialized) update_oxygen_vignette(svc_log, mod_ctx);
    if (s_hpBarsInitialized) update_hp_bars(svc_log, mod_ctx);
    if (s_bossBarInitialized) update_boss_bar(svc_log, mod_ctx);
    if (s_bossRushInitialized) update_boss_rush(svc_log, mod_ctx);
    if (s_bossRushPortalInitialized) update_boss_rush_portal(svc_log, mod_ctx);
    if (s_visibleEquipmentInitialized) update_visible_equipment(svc_log, mod_ctx);
    if (s_zButtonInitialized) update_z_button(svc_log, mod_ctx);
    if (s_quickAccessInitialized) update_quick_access(svc_log, mod_ctx);
    if (s_sheathedSpinInitialized) update_sheathed_spin(svc_log, mod_ctx);
    if (s_flurryRushInitialized) update_flurry_rush(svc_log, mod_ctx);
    if (s_flurryVignetteInitialized) update_flurry_vignette(svc_log, mod_ctx);
    if (s_staminaInitialized) update_stamina(svc_log, mod_ctx);
    if (s_puppetZeldaPatternInitialized) update_puppet_zelda_pattern(svc_log, mod_ctx);
    if (s_collectionMenuInitialized) update_collection_menu(svc_log, mod_ctx);
    if (s_collectionMenuChestInitialized) update_collection_menu_chest(svc_log, mod_ctx);
    return MOD_OK;
}

#ifdef _MSC_VER
#define MOD_SEH_TRY __try
#define MOD_SEH_EXCEPT __except (1)
#else
#define MOD_SEH_TRY if (true)
#define MOD_SEH_EXCEPT else
#endif

static void run_shutdown_step(const char *name, void (*fn)()) {
    MOD_SEH_TRY {
        fn();
    } MOD_SEH_EXCEPT {
        if (svc_log != nullptr) {
            char msg[96];
            std::snprintf(msg, sizeof(msg), "shutdown %s FAULTED - continuing", name);
            svc_log->error(mod_ctx, msg);
        }
    }
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    s_titleModActive = false;
    if (svc_ui && s_menuTabTwilitEssentials != 0) {
        svc_ui->unregister_menu_tab(mod_ctx, s_menuTabTwilitEssentials);
        s_menuTabTwilitEssentials = 0;
    }
    run_shutdown_step("free_camera_toggle", shutdown_free_camera_toggle);
    run_shutdown_step("general", shutdown_general);
    run_shutdown_step("damage_vignette", shutdown_damage_vignette);
    run_shutdown_step("oxygen_vignette", shutdown_oxygen_vignette);
    run_shutdown_step("flurry_vignette", shutdown_flurry_vignette);
    run_shutdown_step("midna_select_freeze_guard", shutdown_midna_select_freeze_guard);
    run_shutdown_step("hp_bars", shutdown_hp_bars);
    run_shutdown_step("boss_bar", shutdown_boss_bar);
    run_shutdown_step("boss_rush", shutdown_boss_rush);
    run_shutdown_step("boss_rush_portal", shutdown_boss_rush_portal);
    run_shutdown_step("boss_rush_dpad", shutdown_boss_rush_dpad);
    run_shutdown_step("visible_equipment", shutdown_visible_equipment);
    run_shutdown_step("z_button", shutdown_z_button);
    run_shutdown_step("quick_access", shutdown_quick_access);
    run_shutdown_step("quick_access_bottles", shutdown_quick_access_bottles);
    run_shutdown_step("flurry_rush", shutdown_flurry_rush);
    run_shutdown_step("stamina", shutdown_stamina);
    run_shutdown_step("collection_menu", shutdown_collection_menu);
    run_shutdown_step("collection_menu_chest", shutdown_collection_menu_chest);
    return MOD_OK;
}

}

const ResourceService* get_resource_service() {
    return svc_resource;
}

const TextureService* get_texture_service() {
    return svc_texture;
}
