#include "hp_bars.hpp"

#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#include "mods/svc/hook.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"

#include "../boss_bar/boss_bar.hpp"

#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_camera.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"
#include "m_Do/m_Do_ext.h"

#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JUtility/JUTFont.h"

bool g_configHpBarsEnabled = false;
bool g_configHpBarsShowNumbers = false;
bool g_configDamageNumbersEnabled = false;

DEFINE_HOOK(&dMeter2Draw_c::draw, Meter2DrawHook);

static std::unordered_map<fpc_ProcID, s16> g_maxHealthMap;
static std::unordered_map<fpc_ProcID, f32> g_enemyAlphaMap;
static std::unordered_map<fpc_ProcID, f32> g_enemyAnchorH;
static std::unordered_map<fpc_ProcID, f32> g_enemyChipMap;

struct DamagePopup {
    fpc_ProcID enemyId;
    s16 damageAmount;
    cXyz worldPos;
    f32 velY;
    f32 velX;
    f32 velZ;
    int currentFrame;
    int maxFrames;
    bool isCritical;
};

static std::unordered_map<fpc_ProcID, s16> s_lastHealthMap;
static std::vector<DamagePopup> s_damagePopups;

static void draw_damage_number(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                               JUtility::TColor top, JUtility::TColor bottom, u8 alpha,
                               f32 outlineScale = 1.0f) {
    JUTFont* font = mDoExt_getSubFont();
    if (!font) font = mDoExt_getMesgFont();
    if (!font) return;

    font->setGX();

    const f32 c = 1.7f * outlineScale;
    const f32 d = 1.2f * outlineScale;
    const f32 kOff[8][2] = {
        { c, 0.0f}, {-c, 0.0f}, {0.0f,  c}, {0.0f, -c},
        { d, d}, {d, -d}, {-d, d}, {-d, -d},
    };
    font->setCharColor(JUtility::TColor(0, 0, 0, alpha));
    for (const auto& o : kOff) {
        font->drawString_scale(x + o[0], y + o[1], charW, charH, text, true);
    }

    top.a = alpha;
    bottom.a = alpha;
    font->setGradColor(top, bottom);
    font->drawString_scale(x, y, charW, charH, text, true);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

static f32 get_text_width_ingame(const char* text, f32 charW) {
    JUTFont* font = mDoExt_getSubFont();
    if (!font) {
        font = mDoExt_getMesgFont();
    }
    if (font) {
        f32 totalWidth = 0.0f;
        for (size_t i = 0; text[i] != '\0'; i++) {
            f32 w = static_cast<f32>(font->getWidth(text[i]));
            if (w <= 0.0f) w = static_cast<f32>(font->getWidth());
            totalWidth += w * (charW / static_cast<f32>(font->getWidth()));
        }
        return totalWidth;
    }
    return static_cast<f32>(std::strlen(text)) * charW;
}

static bool isSenseOnlyEnemy(s16 name) {
    return (name == fpcNm_E_PO_e  ||
            name == fpcNm_E_NZ_e  ||
            name == fpcNm_E_HP_e  ||
            name == fpcNm_E_MS_e  ||
            name == fpcNm_E_GS_e  ||
            name == fpcNm_E_YM_e  ||
            name == fpcNm_E_YMB_e ||
            name == fpcNm_E_YK_e  ||
            name == fpcNm_E_YR_e  ||
            name == fpcNm_E_YG_e);
}

static bool isEnemyActive(fopAc_ac_c* actor) {
    if (fopAcM_GetName(actor) == fpcNm_E_ZS_e) {
        return !static_cast<fopEn_enemy_c*>(actor)->checkWolfNoLock();
    }
    return (actor->attention_info.flags & fopAc_AttnFlags_LOCK) != 0;
}

static bool isEnemyActor(fopAc_ac_c* actor, fopAc_ac_c* player) {
    if (!actor || actor == player) return false;

    s16 name = fopAcM_GetName(actor);
    if (name == fpcNm_ALINK_e || name == fpcNm_HORSE_e || name == fpcNm_COW_e ||
        name == fpcNm_NI_e    || name == fpcNm_DO_e    || name == fpcNm_SQ_e) {
        return false;
    }

    u8 group = fopAcM_GetGroup(actor);
    if (group == fopAc_NPC_e || group == fopAc_ENV_e) {
        return false;
    }

    if (group == fopAc_ENEMY_e) {
        return true;
    }

    if ((name >= 0x0D2 && name <= 0x0D2) ||
        (name >= 0x0E4 && name <= 0x0F5) ||
        (name >= 0x1AF && name <= 0x220) ||
        (name >= 0x230 && name <= 0x278)) {
        return true;
    }

    return false;
}

static cXyz enemy_hp_anchor(fopAc_ac_c* a, fpc_ProcID id) {
    cXyz anchor = a->current.pos;

    f32 h = 0.0f;
    if (a->attention_info.position.abs2() > 0.001f) {
        h = a->attention_info.position.y - a->current.pos.y;
    }
    if (h < 20.0f || h > 150.0f) {
        if (a->eyePos.abs2() > 0.001f) h = a->eyePos.y - a->current.pos.y;
    }
    if (h < 20.0f || h > 150.0f) h = 55.0f;

    auto it = g_enemyAnchorH.find(id);
    if (it != g_enemyAnchorH.end()) {
        if (h > it->second) it->second = h;
        h = it->second;
    } else {
        g_enemyAnchorH[id] = h;
    }

    anchor.y = a->current.pos.y + h + 8.0f;
    return anchor;
}

static bool checkLineOfSight(fopAc_ac_c* enemy, const cXyz& targetPos) {
    if (!enemy) return false;

    dCamera_c* cam = dCam_getBody();
    if (!cam) return true;

    cXyz start = cam->iEye();
    cXyz end = targetPos;

    cXyz diff = end - start;
    f32 dist = diff.abs();
    if (dist < 1.0f) return true;

    end = start + diff * ((dist - 45.0f) / dist);

    if (cam->lineBGCheck(&start, &end, static_cast<u32>(0x40b7))) {
        return false;
    }
    return true;
}

struct DrawHpBarContext {
    fopAc_ac_c* player;
    f32 cursorOffsetY;
};

static int drawEnemyHpBarCallback(void* pActor, void* pData) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(pActor);
    if (!actor) return 0;

    DrawHpBarContext* ctx = static_cast<DrawHpBarContext*>(pData);
    if (!ctx || !ctx->player) return 0;

    if (!isEnemyActor(actor, ctx->player)) {
        return 0;
    }

    if (actor->health <= 0) {
        return 0;
    }

    if ((actor->actor_status & fopAcStts_NODRAW_e) != 0 || (actor->actor_condition & fopAcCnd_NODRAW_e) != 0) {
        return 0;
    }

    s16 name = fopAcM_GetName(actor);

    if (g_configBossBarEnabled && boss_bar_is_boss_name(name)) {
        return 0;
    }

    bool isSenseActive = daPy_py_c::checkNowWolfPowerUp() || dComIfGs_wolfeye_effect_check();
    if (isSenseOnlyEnemy(name) && !isSenseActive) {
        return 0;
    }

    fpc_ProcID id = fopAcM_GetID(actor);

    auto itMaxHp = g_maxHealthMap.find(id);
    if (itMaxHp == g_maxHealthMap.end() || actor->health > itMaxHp->second) {
        g_maxHealthMap[id] = actor->health;
    }
    s16 maxHp = g_maxHealthMap[id];
    if (maxHp <= 0) maxHp = actor->health;

    if (!g_configHpBarsEnabled) {
        return 0;
    }

    cXyz pos = enemy_hp_anchor(actor, id);

    Vec screenPos;
    mDoLib_project(&pos, &screenPos);

    f32 screenW = mDoGph_gInf_c::getWidthF();
    f32 screenH = mDoGph_gInf_c::getHeightF();
    if (screenW <= 0.0f) screenW = 640.0f;
    if (screenH <= 0.0f) screenH = 480.0f;

    bool onScreen = (screenPos.z < 400000.0f &&
                     screenPos.x >= -80.0f && screenPos.x <= screenW + 80.0f &&
                     screenPos.y >= -80.0f && screenPos.y <= screenH + 80.0f);

    bool hasLos = false;
    if (onScreen) {
        hasLos = checkLineOfSight(actor, pos);
    }

    bool enemyActive = isEnemyActive(actor);

    f32 targetAlpha = 0.0f;
    if (enemyActive && onScreen && hasLos) {
        f32 dist = fopAcM_searchActorDistance(ctx->player, actor);
        const f32 maxDist = 4500.0f;
        const f32 fadeDist = 800.0f;

        if (dist < maxDist) {
            if (dist > (maxDist - fadeDist)) {
                targetAlpha = (maxDist - dist) / fadeDist;
            } else {
                targetAlpha = 1.0f;
            }
        }
    }

    f32 currentAlpha = 0.0f;
    auto itAlpha = g_enemyAlphaMap.find(id);
    if (itAlpha != g_enemyAlphaMap.end()) {
        currentAlpha = itAlpha->second;
    }

    currentAlpha += (targetAlpha - currentAlpha) * 0.2f;
    if (currentAlpha < 0.005f) {
        g_enemyAlphaMap.erase(id);
        g_enemyAnchorH.erase(id);
        g_enemyChipMap.erase(id);
        return 0;
    }
    g_enemyAlphaMap[id] = currentAlpha;

    if (!onScreen) {
        return 0;
    }

    f32 hpRatio = static_cast<f32>(actor->health) / static_cast<f32>(maxHp);
    if (hpRatio < 0.0f) hpRatio = 0.0f;
    if (hpRatio > 1.0f) hpRatio = 1.0f;

    // White trailing segment that chases the live value down, like the boss bar's chip.
    f32 chipRatio;
    auto itChip = g_enemyChipMap.find(id);
    if (itChip == g_enemyChipMap.end() || hpRatio >= itChip->second) {
        chipRatio = hpRatio;
    } else {
        chipRatio = itChip->second + (hpRatio - itChip->second) * 0.12f;
    }
    g_enemyChipMap[id] = chipRatio;

    u8 r, g, b;
    if (hpRatio > 0.5f) {
        f32 factor = (hpRatio - 0.5f) * 2.0f;
        r = static_cast<u8>((1.0f - factor) * 230.0f + factor * 30.0f);
        g = static_cast<u8>(factor * 190.0f + (1.0f - factor) * 170.0f);
        b = static_cast<u8>(factor * 70.0f + (1.0f - factor) * 30.0f);
    } else {
        f32 factor = hpRatio * 2.0f;
        r = static_cast<u8>((1.0f - factor) * 220.0f + factor * 230.0f);
        g = static_cast<u8>(factor * 170.0f);
        b = static_cast<u8>((1.0f - factor) * 40.0f + factor * 30.0f);
    }

    const f32 barWidth = 25.0f;
    const f32 barHeight = 1.0f;
    const f32 drawX = screenPos.x - (barWidth * 0.5f);
    const f32 drawY = screenPos.y - (barHeight * 0.5f);

    auto getAlpha = [currentAlpha](u8 baseAlpha) -> u8 {
        return static_cast<u8>(static_cast<f32>(baseAlpha) * currentAlpha);
    };

    J2DFillBox(drawX - 1.5f, drawY - 1.5f, barWidth + 3.0f, barHeight + 3.0f, JUtility::TColor(0, 0, 0, getAlpha(160)));
    J2DDrawFrame(drawX - 1.0f, drawY - 1.0f, barWidth + 2.0f, barHeight + 2.0f, JUtility::TColor(170, 135, 55, getAlpha(230)), 1);
    J2DFillBox(drawX, drawY, barWidth, barHeight, JUtility::TColor(12, 12, 16, getAlpha(230)));

    if (hpRatio > 0.0f) {
        f32 fillWidth = barWidth * hpRatio;
        J2DFillBox(drawX, drawY, fillWidth, barHeight, JUtility::TColor(r, g, b, getAlpha(240)));
        J2DFillBox(drawX, drawY, fillWidth, 1.0f, JUtility::TColor(255, 255, 255, getAlpha(80)));
    }

    if (chipRatio > hpRatio + 0.001f) {
        const f32 chipWidth = barWidth * (chipRatio - hpRatio);
        J2DFillBox(drawX + barWidth * hpRatio, drawY, chipWidth, barHeight,
                   JUtility::TColor(236, 226, 200, getAlpha(150)));
    }

    if (g_configHpBarsShowNumbers) {
        char hpText[32];
        std::snprintf(hpText, sizeof(hpText), "%d/%d", actor->health, maxHp);
        const f32 fontW = 7.0f;
        const f32 fontH = 8.5f;
        f32 textWidth = get_text_width_ingame(hpText, fontW);
        f32 textX = drawX + (barWidth - textWidth) * 0.5f;
        f32 textY = drawY - 3.5f;

        JUtility::TColor creamTop(255, 252, 240, 255);
        JUtility::TColor creamBot(226, 208, 168, 255);
        draw_damage_number(hpText, textX, textY, fontW, fontH, creamTop, creamBot, getAlpha(255), 0.6f);
    }

    return 0;
}

static void* trackEnemyDamageCallback(void* pActor, void*) {
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(pActor);
    if (!actor) return nullptr;

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!isEnemyActor(actor, player)) {
        return nullptr;
    }

    const bool suppressPopup =
        (g_configBossBarEnabled && boss_bar_is_boss_name(fopAcM_GetName(actor))) ||
        !isEnemyActive(actor);

    fpc_ProcID id = fopAcM_GetID(actor);
    s16 curHp = actor->health;

    auto it = s_lastHealthMap.find(id);
    if (it != s_lastHealthMap.end()) {
        s16 prevHp = it->second;
        if (curHp < prevHp) {
            s16 damage = prevHp - curHp;
            if (damage > 0 && !suppressPopup) {
                DamagePopup popup;
                popup.enemyId = id;
                popup.damageAmount = damage;

                popup.worldPos = enemy_hp_anchor(actor, id);

                f32 randX = (static_cast<f32>(std::rand() % 20) - 10.0f);
                f32 randZ = (static_cast<f32>(std::rand() % 20) - 10.0f);
                popup.worldPos.x += randX;
                popup.worldPos.z += randZ;

                popup.velY = 4.0f;
                popup.velX = randX * 0.05f;
                popup.velZ = randZ * 0.05f;
                popup.currentFrame = 0;
                popup.maxFrames = 62;
                popup.isCritical = (damage >= 60);

                s_damagePopups.push_back(popup);
            }
        }
    }

    s_lastHealthMap[id] = curHp;
    return nullptr;
}

void update_hp_bars(const LogService*, ModContext*) {
    if (!g_configDamageNumbersEnabled) {
        s_lastHealthMap.clear();
        s_damagePopups.clear();
        return;
    }

    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) {
        return;
    }

    fopAcIt_Judge(trackEnemyDamageCallback, nullptr);

    for (auto it = s_damagePopups.begin(); it != s_damagePopups.end();) {
        it->currentFrame++;
        it->worldPos.y += it->velY;
        it->worldPos.x += it->velX;
        it->worldPos.z += it->velZ;
        it->velY *= 0.90f;

        if (it->currentFrame >= it->maxFrames) {
            it = s_damagePopups.erase(it);
        } else {
            ++it;
        }
    }
}

static void draw_damage_popups() {
    if (!g_configDamageNumbersEnabled || s_damagePopups.empty()) {
        return;
    }

    J2DFillBox(0.0f, 0.0f, 0.0f, 0.0f, JUtility::TColor(0, 0, 0, 0));

    for (const auto& popup : s_damagePopups) {
        cXyz pos = popup.worldPos;
        Vec screenPos;
        mDoLib_project(&pos, &screenPos);

        if (screenPos.z < 400000.0f && screenPos.x > -50.0f && screenPos.x < 700.0f && screenPos.y > -50.0f && screenPos.y < 500.0f) {
            f32 alphaF = 1.0f;
            const int kFade = 18;
            if (popup.currentFrame > (popup.maxFrames - kFade)) {
                alphaF = static_cast<f32>(popup.maxFrames - popup.currentFrame) / static_cast<f32>(kFade);
            }
            if (alphaF < 0.0f) alphaF = 0.0f;

            u8 alpha = static_cast<u8>(alphaF * 255.0f);
            if (alpha == 0) continue;

            f32 popScale = 1.0f;
            if (popup.currentFrame < 7) {
                popScale = 1.55f - (static_cast<f32>(popup.currentFrame) * 0.08f);
            }

            char numText[32];
            if (popup.isCritical) {
                std::snprintf(numText, sizeof(numText), "-%d!", popup.damageAmount);
            } else {
                std::snprintf(numText, sizeof(numText), "-%d", popup.damageAmount);
            }

            f32 charW = (popup.isCritical ? 24.0f : 17.0f) * popScale;
            f32 charH = (popup.isCritical ? 30.0f : 21.0f) * popScale;

            f32 textW = get_text_width_ingame(numText, charW);
            f32 drawX = screenPos.x - (textW * 0.5f);
            f32 drawY = screenPos.y - charH - 8.0f;

            JUtility::TColor top    = popup.isCritical ? JUtility::TColor(255, 236, 150, 255)
                                                       : JUtility::TColor(255, 150, 120, 255);
            JUtility::TColor bottom = popup.isCritical ? JUtility::TColor(255, 176,  40, 255)
                                                       : JUtility::TColor(224,  32,  28, 255);

            draw_damage_number(numText, drawX, drawY, charW, charH, top, bottom, alpha);
        }
    }
}

static void on_meter2_draw_post(ModContext*, void*, void*, void*) {
    if (!g_configHpBarsEnabled && !g_configDamageNumbersEnabled) {
        s_damagePopups.clear();
        return;
    }

    // Only real pause menus hide the bars; keep drawing through hitstop
    // (which is a scene pause), like the stamina meter does.
    if (dComIfGp_isPauseFlag()) {
        return;
    }

    if (dComIfGp_getEvent() && dComIfGp_getEvent()->runCheck()) {
        return;
    }

    if (g_configHpBarsEnabled) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        if (player) {
            DrawHpBarContext ctx;
            ctx.player = player;
            ctx.cursorOffsetY = 10.0f;

            fopAcIt_Executor(reinterpret_cast<fopAcIt_ExecutorFunc>(drawEnemyHpBarCallback), &ctx);
        }
    }

    if (g_configDamageNumbersEnabled) {
        draw_damage_popups();
    } else {
        s_damagePopups.clear();
    }
}

ModResult init_hp_bars(const HookService* hook_svc, ModError*) {
    if (!hook_svc) {
        return MOD_OK;
    }
    return mods::hook::add_post<Meter2DrawHook>(hook_svc, on_meter2_draw_post);
}

void shutdown_hp_bars() {
    g_maxHealthMap.clear();
    g_enemyAlphaMap.clear();
    g_enemyAnchorH.clear();
    g_enemyChipMap.clear();
    s_lastHealthMap.clear();
    s_damagePopups.clear();
}
