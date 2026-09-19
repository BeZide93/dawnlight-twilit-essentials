#include "boss_rush_texts.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush.hpp"
#include "boss_rush_masterswd.hpp"
#include "boss_rush_timer.hpp"
#include "boss_rush_timer_v2.hpp"
#include "../boss_bar/boss_bar.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_camera_mng.h"
#include "d/d_meter2_info.h"
#include "m_Do/m_Do_lib.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JUtility/JUTFont.h"

#include <cstdio>
#include <cstring>

namespace {

f32 s_labelFade[kMaxBossGalleryEntries] = {};

void draw_world_label(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                      JUtility::TColor top, JUtility::TColor bottom, u8 alpha) {
    JUTFont* font = mDoExt_getSubFont();
    if (!font) font = mDoExt_getMesgFont();
    if (!font) return;

    font->setGX();

    const f32 c = 1.6f;
    const f32 d = 1.1f;
    const f32 kOff[8][2] = {
        { c, 0.0f}, {-c, 0.0f}, {0.0f,  c}, {0.0f, -c},
        { d, d}, {d, -d}, {-d, d}, {-d, -d},
    };
    font->setCharColor(JUtility::TColor(0, 0, 0, alpha));
    for (const auto& o : kOff) {
        font->drawString_scale(x + o[0], y + o[1], charW, charH, text, true);
    }

    JUtility::TColor t = top;
    JUtility::TColor b = bottom;
    t.a = alpha;
    b.a = alpha;
    font->setGradColor(t, b);
    font->drawString_scale(x, y, charW, charH, text, true);
}

f32 measure_text_width(const char* text, f32 charW) {
    JUTFont* font = mDoExt_getSubFont();
    if (!font) font = mDoExt_getMesgFont();
    if (!font) return static_cast<f32>(std::strlen(text)) * charW;

    f32 total = 0.0f;
    for (size_t i = 0; text[i] != '\0'; i++) {
        f32 w = static_cast<f32>(font->getWidth(text[i]));
        if (w <= 0.0f) w = static_cast<f32>(font->getWidth());
        total += w * (charW / static_cast<f32>(font->getWidth()));
    }
    return total;
}

}

f32 boss_rush_texts_measure_width(const char* text, f32 charW) {
    return measure_text_width(text, charW);
}

void boss_rush_texts_draw_label(const char* text, f32 x, f32 y, f32 charW, f32 charH,
                                JUtility::TColor top, JUtility::TColor bottom, u8 alpha) {
    draw_world_label(text, x, y, charW, charH, top, bottom, alpha);
}

void boss_rush_texts_reset_fade() {
    for (auto& f : s_labelFade) f = 0.0f;
}

void draw_boss_rush_texts(float floorY) {
    if (!boss_rush_scene_load_stable()) {
        return;
    }

    J2DFillBox(0.0f, 0.0f, 0.0f, 0.0f, JUtility::TColor(0, 0, 0, 0));

    const size_t count = boss_rush_get_active_gallery_count();

    const daAlink_c* link = daAlink_getAlinkActorClass();

    for (size_t circleSlot = 0; circleSlot < count; ++circleSlot) {
        const size_t tableIdx = boss_rush_get_active_gallery_table_index(circleSlot);
        const BossGalleryEntry& boss = g_bossGalleryTable[tableIdx];

        cXyz pos;
        csXyz angle;
        boss_rush_get_slot_transform(circleSlot, count, floorY, pos, angle);

        f32 target = 0.0f;
        if (link != nullptr) {
            const f32 dx = link->current.pos.x - pos.x;
            const f32 dz = link->current.pos.z - pos.z;
            if (dx * dx + dz * dz < kBossInteractRadius * kBossInteractRadius) target = 1.0f;
        }
        s_labelFade[tableIdx] += (target - s_labelFade[tableIdx]) * 0.12f;
        if (s_labelFade[tableIdx] < 0.004f) { s_labelFade[tableIdx] = 0.0f; continue; }

        pos.y += boss.labelYOffset;

        Vec screenPos;
        mDoLib_project(&pos, &screenPos);

        if (screenPos.z >= 400000.0f || screenPos.x < -80.0f || screenPos.x > 720.0f ||
            screenPos.y < -80.0f || screenPos.y > 500.0f) {
            continue;
        }

        const f32 nameCharW = 22.0f, nameCharH = 26.0f;
        const f32 locCharW = 14.0f, locCharH = 17.0f;

        f32 nameW = measure_text_width(boss.displayName, nameCharW);
        f32 locW = boss.location ? measure_text_width(boss.location, locCharW) : 0.0f;

        const u8 nameA = static_cast<u8>(255.0f * s_labelFade[tableIdx]);
        const u8 locA = static_cast<u8>(220.0f * s_labelFade[tableIdx]);

        draw_world_label(boss.displayName, screenPos.x - nameW * 0.5f, screenPos.y - nameCharH - locCharH,
                         nameCharW, nameCharH,
                         JUtility::TColor(255, 236, 170, 255), JUtility::TColor(255, 190, 60, 255), nameA);

        if (boss.location != nullptr && boss.location[0] != '\0') {
            draw_world_label(boss.location, screenPos.x - locW * 0.5f, screenPos.y - locCharH,
                             locCharW, locCharH,
                             JUtility::TColor(230, 230, 230, 255), JUtility::TColor(180, 180, 180, 255), locA);
        }

        if (g_configBossRushTimer) {
            char timeBuf[24];
            u32 bestCs = 0;
            if (boss_rush_timer_best_cs(static_cast<int>(tableIdx), &bestCs)) {
                char t[16];
                boss_rush_timer_format(bestCs, t, sizeof(t));
                std::snprintf(timeBuf, sizeof(timeBuf), "Best  %s", t);
            } else {
                std::snprintf(timeBuf, sizeof(timeBuf), "Best  --:--.--");
            }
            const f32 tW = measure_text_width(timeBuf, locCharW);
            const u8 tA = static_cast<u8>(200.0f * s_labelFade[tableIdx]);
            draw_world_label(timeBuf, screenPos.x - tW * 0.5f, screenPos.y,
                             locCharW, locCharH,
                             JUtility::TColor(255, 226, 140, 255), JUtility::TColor(230, 170, 70, 255), tA);
        }
    }

    // Master sword spawn disabled for now.
    // draw_boss_rush_master_sword_label(link, floorY);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

void draw_boss_rush_fight_timer() {
    if (!g_configBossRushTimer) {
        return;
    }

    u32 cs = 0;
    if (!boss_rush_timer_active_cs(&cs)) {
        return;
    }

    u32 resultCs = 0;
    bool isRecord = false;
    const bool showingResult = boss_rush_timer_result(&resultCs, &isRecord);

    if (!boss_rush_is_fighting_here() && !boss_rush_is_returning_to_chamber() && !showingResult &&
        !boss_rush_timer_chain_active()) {
        return;
    }

    if (!showingResult) {
        static int s_menuFrames = 0;
        const bool menuish = dComIfGp_isPauseFlag() ||
                             dMeter2Info_getWindowStatus() != 0 ||
                             dMeter2Info_getPauseStatus() != 0;
        s_menuFrames = menuish ? (s_menuFrames + 1) : 0;
        if (s_menuFrames > 4) {
            return;
        }
        if (boss_bar_hidden_for_transition()) {
            return;
        }
    }

    const int tIdx = boss_rush_current_target_index();
    u32 bestCs = 0;
    bool hasBest = false;
    if (boss_rush_timer_chain_active()) {
        hasBest = g_configBossRushShowBestTimer && boss_rush_timer_chain_best_cs(&bestCs);
    } else {
        hasBest = g_configBossRushShowBestTimer && (tIdx >= 0) &&
                  boss_rush_timer_best_cs(tIdx, &bestCs);
    }
    hasBest = hasBest && !showingResult;
    boss_rush_timer_v2_draw(showingResult ? resultCs : cs, showingResult, isRecord,
                            hasBest, hasBest ? bestCs : 0);
}

void draw_boss_rush_debug_coords(daAlink_c* link) {
    if (link == nullptr) return;

    JUTFont* font = mDoExt_getSubFont();
    if (!font) font = mDoExt_getMesgFont();
    if (!font) return;

    const f32 boxX = 16.0f;
    const f32 boxY = 130.0f;
    const f32 boxW = 210.0f;
    const f32 boxH = 132.0f;
    J2DFillBox(boxX, boxY, boxW, boxH, JUtility::TColor(12, 16, 24, 190));

    J2DFillBox(boxX, boxY, 3.0f, boxH, JUtility::TColor(240, 195, 75, 250));

    const f32 textX = boxX + 8.0f;
    const f32 charW = 11.0f;
    const f32 charH = 14.0f;
    const f32 lineH = 18.0f;

    char buf[64];
    f32 curY = boxY + 6.0f;

    std::snprintf(buf, sizeof(buf), "X:   %9.2f", link->current.pos.x);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(255, 255, 255, 255), JUtility::TColor(210, 210, 210, 255), 255);
    curY += lineH;

    std::snprintf(buf, sizeof(buf), "Y:   %9.2f", link->current.pos.y);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(255, 255, 255, 255), JUtility::TColor(210, 210, 210, 255), 255);
    curY += lineH;

    std::snprintf(buf, sizeof(buf), "Z:   %9.2f", link->current.pos.z);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(255, 255, 255, 255), JUtility::TColor(210, 210, 210, 255), 255);
    curY += lineH;

    s16 rawAngle = link->shape_angle.y;
    f32 deg = (static_cast<f32>(rawAngle) * 360.0f) / 65536.0f;
    if (deg < 0.0f) deg += 360.0f;
    std::snprintf(buf, sizeof(buf), "Ang: %6d (%5.1f deg)", rawAngle, deg);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(255, 230, 130, 255), JUtility::TColor(240, 180, 60, 255), 255);
    curY += lineH;

    const char* stage = dComIfGp_getStartStageName();
    int room = dComIfGp_roomControl_getStayNo();
    std::snprintf(buf, sizeof(buf), "Stg: %s (R%d)", stage ? stage : "-", room);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(180, 220, 255, 255), JUtility::TColor(130, 170, 220, 255), 255);
    curY += lineH;

    s16 point = dComIfGp_getStartStagePoint();
    std::snprintf(buf, sizeof(buf), "Pt:  %d", point);
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(180, 255, 190, 255), JUtility::TColor(130, 220, 150, 255), 255);
    curY += lineH;

    camera_process_class* cam = dComIfGp_getCamera(0);
    if (cam != nullptr) {
        s16 camAngle = cam->angle.y;
        f32 camDeg = (static_cast<f32>(camAngle) * 360.0f) / 65536.0f;
        if (camDeg < 0.0f) camDeg += 360.0f;
        std::snprintf(buf, sizeof(buf), "Cam: %6d (%5.1f deg)", camAngle, camDeg);
        draw_world_label(buf, textX, curY, charW, charH,
                         JUtility::TColor(255, 200, 200, 255), JUtility::TColor(220, 140, 140, 255), 255);
        curY += lineH;
    }

    const char* fightLabel = nullptr;
    bool fightEngaged = false;
    if (boss_bar_current_fight_state(&fightLabel, fightEngaged)) {
        std::snprintf(buf, sizeof(buf), "Fight: %s", fightLabel);
    } else {
        std::snprintf(buf, sizeof(buf), "Fight: (none tracked)");
    }
    draw_world_label(buf, textX, curY, charW, charH,
                     JUtility::TColor(230, 200, 255, 255), JUtility::TColor(180, 140, 220, 255), 255);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}
