#include "boss_rush_portal.hpp"

#include "boss_rush.hpp"
#include "boss_rush_texts.hpp"

#include "mods/hook.hpp"
#include "mods/service.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_menu_dmap.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_fmap2D.h"
#include "d/d_menu_window.h"
#include "d/d_meter2_info.h"
#include "d/d_meter2_draw.h"
#include "d/d_pane_class.h"
#include "d/d_save.h"
#include "d/d_s_play.h"
#include "d/d_select_cursor.h"
#include "d/actor/d_a_player.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/TColor.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "dusk/config_var.hpp"

#include <cmath>
#include <string_view>

bool g_configBossRushPortal = true;

static constexpr f32 kPortalMapX = 427.0f;
static constexpr f32 kPortalMapY = 344.0f;
static constexpr f32 kCursorSelectRadiusSq = 729.0f;
static constexpr f32 kPortalDrawSize = 52.0f;
static constexpr f32 kHoverBracketScale = 0.8f;

static bool s_warpPending      = false;
static bool s_requestMapClose  = false;
static bool s_requestDmapClose = false;
static bool s_portalHovered    = false;
static int  s_warpDelay        = 0;

static J2DScreen* s_goldPortalScreen = nullptr;
static J2DPane*   s_goldPortalRoot   = nullptr;
static f32        s_goldPortalW      = 0.0f;

static dSelect_cursor_c* s_hoverBracket = nullptr;

static const dusk::config::ConfigVar<bool>* s_mirrorModeVar = nullptr;

static bool is_mirror_mode() {
    return s_mirrorModeVar != nullptr && s_mirrorModeVar->getValue();
}

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
    *outX = kPortalMapX;
    *outY = kPortalMapY;
}

static void get_portal_screen_pos(dMenu_Fmap2DBack_c* back, f32* outX, f32* outY) {
    get_portal_map_pos(outX, outY);
    if (back) {
        *outX += back->mTransX;
        *outY += back->mTransZ;
        if (is_mirror_mode()) {
            *outX = back->getMirrorPosX(*outX, 0.0f);
        }
    }
}

static void tint_portal_pane_gold(J2DPane* pane) {
    if (pane == nullptr) return;
    if (pane->getTypeID() == 18) {
        static_cast<J2DPicture*>(pane)->setBlackWhite(
            JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 215, 0, 255));
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        tint_portal_pane_gold(child);
    }
}

static void ensure_portal_screen() {
    if (s_goldPortalScreen != nullptr) {
        return;
    }

    JKRArchive* arc = g_dComIfG_gameInfo.play.getFmapResArchive();
    if (!arc) {
        arc = dComIfGp_getMain2DArchive();
    }
    if (!arc) return;

    J2DScreen* screen = new J2DScreen();
    bool ok = screen->setPriority("zelda_map_screen_portal_icon.blo", 0x20000, arc);
    if (!ok) {
        ok = screen->setPriority("SCRN/zelda_map_screen_portal_icon.blo", 0x20000, arc);
    }
    if (!ok) {
        delete screen;
        return;
    }
    dPaneClass_showNullPane(screen);

    J2DPane* root = screen->search(MULTI_CHAR('Null'));
    if (root == nullptr) {
        delete screen;
        return;
    }

    f32 nativeW = root->getWidth();
    if (nativeW <= 0.0f) nativeW = 40.0f;

    tint_portal_pane_gold(root);

    s_goldPortalScreen = screen;
    s_goldPortalRoot   = root;
    s_goldPortalW      = nativeW;
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

    ensure_portal_screen();

    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx) {
        ctx->setup2D();
    }

    if (s_goldPortalScreen != nullptr && s_goldPortalRoot != nullptr) {
        const f32 scale = kPortalDrawSize / s_goldPortalW;
        s_goldPortalRoot->scale(scale, scale);
        s_goldPortalRoot->translate(posX, posY);
        s_goldPortalScreen->draw(0.0f, 0.0f, ctx);
    }

    if (s_portalHovered) {
        if (!s_hoverBracket) {
            s_hoverBracket = new dSelect_cursor_c(4, 1.0f, nullptr);
        }
        if (s_hoverBracket) {
            s_hoverBracket->onUpdateFlag();
            s_hoverBracket->setAlphaRate(1.0f);
            s_hoverBracket->setPos(posX, posY);
            s_hoverBracket->setScale(kHoverBracketScale);
            s_hoverBracket->draw();
            s_hoverBracket->resetUpdateFlag();
        }

        if (ctx) {
            ctx->setup2D();
        }
        J2DFillBox(0.0f, 0.0f, 0.0f, 0.0f, JUtility::TColor(0, 0, 0, 0));
        const f32 charW = 18.0f;
        const f32 charH = 22.0f;
        const f32 textW = boss_rush_texts_measure_width("Boss Rush", charW);
        boss_rush_texts_draw_label("Boss Rush", posX - textW * 0.5f, posY - kPortalDrawSize * 0.5f - 8.0f,
                                   charW, charH, JUtility::TColor(255, 235, 140, 255),
                                   JUtility::TColor(255, 190, 0, 255), 255);
        if (ctx) {
            ctx->setup2D();
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

ModResult init_boss_rush_portal(const HookService* hook_svc, const LogService*, ModContext* ctx) {
    if (!hook_svc) return MOD_OK;

    void* addr = nullptr;
    if (ctx != nullptr &&
        hook_svc->resolve(ctx, "dusk::config::GetConfigVar", &addr, nullptr) == MOD_OK &&
        addr != nullptr) {
        using GetConfigVarFn = dusk::config::ConfigVarBase* (*)(std::string_view);
        s_mirrorModeVar = static_cast<const dusk::config::ConfigVar<bool>*>(
            reinterpret_cast<GetConfigVarFn>(addr)("game.enableMirrorMode"));
    }

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
    s_warpDelay        = 0;

    delete s_goldPortalScreen;
    s_goldPortalScreen = nullptr;
    s_goldPortalRoot   = nullptr;
    s_goldPortalW      = 0.0f;
}
