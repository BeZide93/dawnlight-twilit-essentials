#include "boss_rush_portal.hpp"

#include "boss_rush.hpp"

#include "mods/hook.hpp"
#include "mods/service.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_dmap.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_fmap2D.h"
#include "d/d_menu_window.h"
#include "d/d_meter2_info.h"
#include "d/d_meter2_draw.h"
#include "d/d_save.h"
#include "d/d_s_play.h"
#include "d/d_select_cursor.h"
#include "d/actor/d_a_player.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/TColor.h"
#include "m_Do/m_Do_graphic.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <cmath>

bool g_configBossRushPortal = false;

static constexpr f32 kCompassBaseX = 398.0f;
static constexpr f32 kCompassBaseY = 344.0f;
static constexpr f32 kCursorSelectRadiusSq = 625.0f;

static bool s_warpPending      = false;
static bool s_requestMapClose  = false;
static bool s_requestDmapClose = false;
static bool s_portalHovered    = false;
static int  s_warpDelay        = 0;
static f32  s_glowPhase        = 0.0f;

static J2DPicture* s_goldPortalPic   = nullptr;
static J2DPicture* s_goldFogPic      = nullptr;
static ResTIMG*    s_lastPortalTimg  = nullptr;

static dSelect_cursor_c* s_hoverBracket = nullptr;

static bool portal_available() {
    if (!g_configBossRushPortal) return false;
    if (is_boss_rush_active() || is_boss_rush_transition_in_flight()) return false;
    return true;
}

static bool vanilla_warp_map_available() {
    return dComIfGs_isEventBit(dSv_event_flag_c::M_021) != 0
        && g_meter2_info.getMapStatus() != 9
        && g_meter2_info.getMapStatus() != 7
        && g_meter2_info.getMapStatus() != 8;
}

static bool is_fmap_view_active(dMenu_Fmap_c* fmap) {
    if (!fmap) return false;
    return fmap->mProcess == dMenu_Fmap_c::PROC_ALL_MAP;
}

static void get_portal_map_pos(f32* outX, f32* outY) {
    *outX = mDoGph_gInf_c::getSafeMinXF() + kCompassBaseX * mDoGph_gInf_c::hudAspectScaleUp;
    *outY = kCompassBaseY;
}

static void get_portal_screen_pos(dMenu_Fmap2DBack_c* back, f32* outX, f32* outY) {
    get_portal_map_pos(outX, outY);
    if (back) {
        *outX += back->mTransX;
        *outY += back->mTransZ;
    }
}

static void update_portal_textures() {
    JKRArchive* arc = g_dComIfG_gameInfo.play.getFmapResArchive();
    if (!arc) {
        arc = dComIfGp_getMain2DArchive();
    }
    if (!arc) return;

    ResTIMG* portalImg = (ResTIMG*)arc->getResource('TIMG', "im_map_icon_portal_4ia_40_05.bti");
    if (portalImg && portalImg != s_lastPortalTimg) {
        s_lastPortalTimg = portalImg;

        if (!s_goldPortalPic) {
            s_goldPortalPic = new J2DPicture(portalImg);
        } else {
            s_goldPortalPic->changeTexture(portalImg, 0);
        }
        s_goldPortalPic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 215, 0, 255));

        if (!s_goldFogPic) {
            s_goldFogPic = new J2DPicture(portalImg);
        } else {
            s_goldFogPic->changeTexture(portalImg, 0);
        }
        s_goldFogPic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 200, 40, 255));
    }
}

static HookAction handle_portal_cursor_pre(dMenu_Fmap_c* fmap) {
    if (!portal_available() || s_warpPending || !fmap || !is_fmap_view_active(fmap)) {
        s_portalHovered = false;
        return HOOK_CONTINUE;
    }

    dMenu_Fmap2DBack_c* back = fmap->mpDraw2DBack;
    if (!back) {
        s_portalHovered = false;
        return HOOK_CONTINUE;
    }

    f32 portalX = 0.0f, portalY = 0.0f;
    get_portal_map_pos(&portalX, &portalY);

    const f32 cursorX = back->getArrowPos2DX();
    const f32 cursorY = back->getArrowPos2DY();
    const f32 dx = cursorX - portalX;
    const f32 dy = cursorY - portalY;
    const bool hovered = (dx * dx + dy * dy <= kCursorSelectRadiusSq);

    if (hovered && !s_portalHovered) {
        Z2GetAudioMgr()->seStart(Z2SE_WARP_MAP_CURSOR, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }
    s_portalHovered = hovered;

    if (s_portalHovered) {
        if (dMw_A_TRIGGER() || dMw_Z_TRIGGER()) {
            s_warpPending     = true;
            s_requestMapClose = true;
            Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            start_boss_rush_dungeon_warp();
            return HOOK_SKIP_ORIGINAL;
        }
    }

    return HOOK_CONTINUE;
}

static void handle_portal_cursor_post(dMenu_Fmap_c* fmap) {
    if (!fmap || !fmap->mpDraw2DTop || !is_fmap_view_active(fmap)) return;
    dMenu_Fmap2DTop_c* top = fmap->mpDraw2DTop;

    if (s_portalHovered) {
        top->setAButtonString(0x527, dMenu_Fmap2DTop_c::ALPHA_DEFAULT);
    }
}

DEFINE_HOOK(&dMenu_Fmap_c::region_map_proc, BossRushPortalRegionMap);

static HookAction fmap_region_map_pre(ModContext*, void*, void*, void*) {
    if (!portal_available() || s_warpPending) {
        s_portalHovered = false;
        return HOOK_CONTINUE;
    }

    HookAction cursorAction = handle_portal_cursor_pre(dMenu_Fmap_c::MyClass);
    if (cursorAction == HOOK_SKIP_ORIGINAL) {
        return HOOK_SKIP_ORIGINAL;
    }

    if (!dMw_Z_TRIGGER()) return HOOK_CONTINUE;
    if (vanilla_warp_map_available()) return HOOK_CONTINUE;

    s_warpPending     = true;
    s_requestMapClose = true;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    start_boss_rush_dungeon_warp();
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&dMenu_Fmap_c::all_map_proc, BossRushPortalAllMap);

static HookAction fmap_all_map_pre(ModContext*, void*, void*, void*) {
    return handle_portal_cursor_pre(dMenu_Fmap_c::MyClass);
}

static void fmap_all_map_post(ModContext*, void*, void*, void*) {
    handle_portal_cursor_post(dMenu_Fmap_c::MyClass);
}

DEFINE_HOOK(&dMenu_Fmap_c::portal_warp_map_proc, BossRushPortalWarpMap);

static HookAction fmap_portal_warp_map_pre(ModContext*, void*, void*, void*) {
    return handle_portal_cursor_pre(dMenu_Fmap_c::MyClass);
}

static void fmap_portal_warp_map_post(ModContext*, void*, void*, void*) {
    handle_portal_cursor_post(dMenu_Fmap_c::MyClass);
}

DEFINE_HOOK(&dMenu_Fmap_c::getNextStatus, BossRushFmapGetNextStatus);

static HookAction fmap_get_next_status_pre(ModContext*, void* args, void* retval, void*) {
    if (!s_requestMapClose) return HOOK_CONTINUE;
    s_requestMapClose = false;

    dMenu_Fmap_c* fmap = mods::arg<dMenu_Fmap_c*>(args, 0);
    u8* param_0 = mods::arg<u8*>(args, 1);
    if (param_0) *param_0 = 0;

    if (fmap) {
        if (fmap->mPanDirection == 3) {
            fmap->mPanDirection = 1;
            g_meter2_info.setMapStatus(0);
            g_meter2_info.setMapKeyDirection(0x400);
        } else {
            fmap->mPanDirection = 3;
            g_meter2_info.setMapStatus(0);
            g_meter2_info.setMapKeyDirection(0x200);
        }
    }

    Z2GetAudioMgr()->seStart(Z2SE_SY_MAP_CLOSE_L, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    dMeter2Info_set2DVibrationM();

    *static_cast<u8*>(retval) = 9;
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&dMeter2Draw_c::draw, BossRushPortalDraw);

static void fmap_draw_portal_post(ModContext*, void*, void*, void*) {
    if (!portal_available()) return;

    dMenu_Fmap_c* fmap = dMenu_Fmap_c::MyClass;
    if (!fmap || !fmap->mpDraw2DBack || !is_fmap_view_active(fmap)) {
        return;
    }

    dMenu_Fmap2DBack_c* back = fmap->mpDraw2DBack;

    f32 posX = 0.0f, posY = 0.0f;
    get_portal_screen_pos(back, &posX, &posY);

    update_portal_textures();

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }

    if (s_goldFogPic) {
        const f32 pulse    = 0.5f + 0.5f * std::sin(s_glowPhase);
        const f32 fogSize  = 44.0f + pulse * 16.0f;
        const u8  fogAlpha = static_cast<u8>(110.0f + pulse * 90.0f);
        s_goldFogPic->setAlpha(fogAlpha);
        s_goldFogPic->draw(posX - fogSize * 0.5f, posY - fogSize * 0.5f, fogSize, fogSize,
                           false, false, false);
    }

    if (s_goldPortalPic) {
        s_goldPortalPic->setAlpha(255);
        s_goldPortalPic->draw(posX - 20.0f, posY - 20.0f, 40.0f, 40.0f, false, false, false);
    }

    if (s_portalHovered) {
        if (!s_hoverBracket) {
            s_hoverBracket = new dSelect_cursor_c(4, 1.0f, nullptr);
        }
        if (s_hoverBracket) {
            s_hoverBracket->onUpdateFlag();
            s_hoverBracket->setAlphaRate(1.0f);
            s_hoverBracket->setPos(posX, posY);
            s_hoverBracket->setScale(1.0f);
            s_hoverBracket->draw();
            s_hoverBracket->resetUpdateFlag();
        }
    }
}

DEFINE_HOOK(&dMenu_Dmap_c::getNextStatus, BossRushDmapGetNextStatus);

static HookAction dmap_get_next_status_pre(ModContext*, void*, void* retval, void*) {
    if (!s_requestDmapClose) return HOOK_CONTINUE;
    s_requestDmapClose = false;

    g_meter2_info.setMapStatus(0);
    g_meter2_info.setMapKeyDirection(0x400);
    *static_cast<u8*>(retval) = 1;
    return HOOK_SKIP_ORIGINAL;
}

DEFINE_HOOK(&dMenu_Dmap_c::mapMode_proc, BossRushPortalDmapMap);

static HookAction dmap_map_mode_pre(ModContext*, void*, void*, void*) {
    if (!portal_available() || s_warpPending) return HOOK_CONTINUE;
    if (!dMw_Z_TRIGGER()) return HOOK_CONTINUE;

    s_warpPending      = true;
    s_requestDmapClose = true;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_OK, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    start_boss_rush_dungeon_warp();
    return HOOK_SKIP_ORIGINAL;
}

void update_boss_rush_portal(const LogService* log_svc, ModContext* mod_ctx) {
    if (portal_available()) {
        s_glowPhase += 0.06f;
        if (s_glowPhase >= 6.2831853f) s_glowPhase -= 6.2831853f;
    }

    if (!s_warpPending) {
        s_warpDelay = 0;
        return;
    }

    if (dMenu_Fmap_c::MyClass != nullptr) {
        return;
    }
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause() || g_meter2_info.getMapStatus() != 0) {
        return;
    }

    s_warpPending = false;
    s_warpDelay   = 0;
}

ModResult init_boss_rush_portal(const HookService* hook_svc, const LogService*, ModContext*) {
    if (!hook_svc) return MOD_OK;

    mods::hook::add_pre<BossRushPortalRegionMap>(hook_svc, fmap_region_map_pre);
    mods::hook::add_pre<BossRushPortalAllMap>(hook_svc, fmap_all_map_pre);
    mods::hook::add_post<BossRushPortalAllMap>(hook_svc, fmap_all_map_post);
    mods::hook::add_pre<BossRushPortalWarpMap>(hook_svc, fmap_portal_warp_map_pre);
    mods::hook::add_post<BossRushPortalWarpMap>(hook_svc, fmap_portal_warp_map_post);
    mods::hook::add_pre<BossRushFmapGetNextStatus>(hook_svc, fmap_get_next_status_pre);
    mods::hook::add_post<BossRushPortalDraw>(hook_svc, fmap_draw_portal_post);
    mods::hook::add_pre<BossRushDmapGetNextStatus>(hook_svc, dmap_get_next_status_pre);
    mods::hook::add_pre<BossRushPortalDmapMap>(hook_svc, dmap_map_mode_pre);
    return MOD_OK;
}

void shutdown_boss_rush_portal() {
    s_warpPending      = false;
    s_requestMapClose  = false;
    s_requestDmapClose = false;
    s_portalHovered    = false;
    s_glowPhase        = 0.0f;
    s_warpDelay        = 0;
}
