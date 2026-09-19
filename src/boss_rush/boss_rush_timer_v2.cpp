#include "boss_rush_timer_v2.hpp"

#include "../util.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"
#include "global.h"
#include "JSystem/J2DGraph/J2DAnmLoader.h"
#include "JSystem/J2DGraph/J2DAnimation.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/J2DGraph/J2DWindow.h"
#include "JSystem/JKernel/JKRFileLoader.h"
#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>
#include "JSystem/JUtility/JUTFont.h"
#include "m_Do/m_Do_ext.h"

#include <chrono>
#include <cmath>

namespace {

constexpr f32 kTimerPosX    = 198.0f;
constexpr f32 kTimerPosY    = 160.0f;
constexpr f32 kTimerSizeX   = 1.4f;
constexpr f32 kTimerSizeY   = 1.0f;
constexpr f32 kTimerAlpha   = 0.5f;
constexpr f32 kParentAlpha  = 1.0f;

constexpr f32  kRunSlideDistY  = 145.0f;
constexpr f32  kRunSlideSpeedY = 25.0f;
constexpr f32  kSlideInDistX   = 50.0f;
constexpr int  kSlideInFrames  = 15;
constexpr int  kAnimeEndFrame  = 22;
constexpr int  kSettleFrames   = 6;

constexpr f32 kGetInAnimSpeed        = 1.0f;
constexpr s16 kGetInWaitFrames       = 10;
constexpr s16 kGetInAlphaFrames      = 10;

enum DigitSlot {
    DIGIT_MIN_TENS, DIGIT_MIN_ONES, DIGIT_SEC_TENS, DIGIT_SEC_ONES,
    DIGIT_CS_TENS, DIGIT_CS_ONES,
    DIGIT_COUNT,
};
constexpr u64 kDigitPaneTags[DIGIT_COUNT][2] = {
    {MULTI_CHAR('t_n_6'), MULTI_CHAR('t_n_6_s')},
    {MULTI_CHAR('t_n_5'), MULTI_CHAR('t_n_5_s')},
    {MULTI_CHAR('t_n_4'), MULTI_CHAR('t_n_4_s')},
    {MULTI_CHAR('t_n_3'), MULTI_CHAR('t_n_3_s')},
    {MULTI_CHAR('t_n_2'), MULTI_CHAR('t_n_2_s')},
    {MULTI_CHAR('t_n_1'), MULTI_CHAR('t_n_1_s')},
};

J2DScreen*  s_screen      = nullptr;
CPaneMgr*   s_timePane    = nullptr;
CPaneMgr*   s_counterPane = nullptr;
CPaneMgr*   s_iconPane    = nullptr;
J2DPane*    s_digits[DIGIT_COUNT][2] = {};
bool        s_screenReady  = false;
bool        s_archiveHeld  = false;

std::chrono::steady_clock::time_point s_lastCall{};
bool s_haveLastCall = false;
int  s_settle     = 0;
int  s_animeFrame = 0;
bool s_animeDone  = false;
bool s_timeShown  = false;
f32  s_transY     = 0.0f;
u32  s_lastDrawn  = 0xFFFFFFFF;
bool s_recordTint = false;
std::chrono::steady_clock::time_point s_recordBeatStart{};
bool s_recordBeatActive = false;
bool s_rowMeasured      = false;
f32  s_rowCentreX  = 0.0f;
f32  s_rowCentreY  = 0.0f;

J2DScreen*       s_getinScreen = nullptr;
J2DAnmTransform* s_getinBck    = nullptr;
CPaneMgr*        s_getinParent = nullptr;
CPaneMgr*        s_getinRoot   = nullptr;
CPaneMgr*        s_getinText   = nullptr;
bool  s_getinActive     = false;
f32   s_getinBckFrame    = 0.0f;
f32   s_getinFadeFrame   = 0.0f;

inline f32 ease_quad(int n, int x) {
    return (static_cast<f32>(x) * static_cast<f32>(x)) /
           (static_cast<f32>(n) * static_cast<f32>(n));
}

void prune_getin_extras(J2DPane* pane, J2DPane** keep, int keepCount,
                        J2DPane* textMain, J2DPane* textShadow);

void destroy_screen() {
    delete s_timePane;
    delete s_counterPane;
    delete s_iconPane;
    delete s_screen;
    s_timePane = s_counterPane = s_iconPane = nullptr;
    s_screen = nullptr;

    delete s_getinParent;
    delete s_getinRoot;
    delete s_getinText;
    delete s_getinBck;
    delete s_getinScreen;
    s_getinParent = s_getinRoot = s_getinText = nullptr;
    s_getinBck = nullptr;
    s_getinScreen = nullptr;
    s_getinActive = false;
    s_getinFadeFrame = 0.0f;

    for (auto& row : s_digits) row[0] = row[1] = nullptr;
    s_screenReady = false;
    s_haveLastCall = false;
    s_lastDrawn = 0xFFFFFFFF;
    s_recordTint = false;
    s_recordBeatActive = false;
    s_rowMeasured = false;

    if (s_archiveHeld) {
        unloadObjectArchive("Timer");
        s_archiveHeld = false;
    }
}

bool ensure_screen() {
    if (s_screenReady) return true;

    if (loadObjectArchive("Timer") != 0) return false;

    dRes_info_c* info = dComIfG_getObjectResInfo("Timer");
    JKRArchive* archive = (info != nullptr) ? info->getArchive() : nullptr;
    if (archive == nullptr) return false;

    ensure_system_heap_capacity();

    J2DScreen* screen = new J2DScreen();
    if (screen == nullptr || !screen->setPriority("zelda_game_image_cow_game.blo", 0x20000, archive)) {
        delete screen;
        unloadObjectArchive("Timer");
        return false;
    }
    s_screen = screen;
    s_archiveHeld = true;

    dPaneClass_showNullPane(s_screen);

    s_counterPane = new CPaneMgr(s_screen, MULTI_CHAR('cow_n'), 2, NULL);
    s_timePane    = new CPaneMgr(s_screen, MULTI_CHAR('time_n'), 2, NULL);
    s_iconPane    = new CPaneMgr(s_screen, MULTI_CHAR('cow_i_n'), 2, NULL);
    if (s_counterPane != nullptr) s_counterPane->setAlphaRate(0.0f);
    if (s_timePane != nullptr)    s_timePane->setAlphaRate(0.0f);
    if (s_iconPane != nullptr)    s_iconPane->setAlphaRate(0.0f);

    for (int i = 0; i < DIGIT_COUNT; i++) {
        for (int j = 0; j < 2; j++) {
            s_digits[i][j] = s_screen->search(kDigitPaneTags[i][j]);
        }
    }

    J2DPane* denom[4] = {
        s_screen->search(MULTI_CHAR('c_n_2')), s_screen->search(MULTI_CHAR('c_n_2_s')),
        s_screen->search(MULTI_CHAR('c_n_1')), s_screen->search(MULTI_CHAR('c_n_1_s')),
    };
    for (J2DPane* pane : denom) {
        if (pane != nullptr) pane->hide();
    }
    J2DPane* slashS = s_screen->search(MULTI_CHAR('c_sl_s'));
    if (slashS != nullptr) slashS->hide();
    J2DPane* slash = s_screen->search(MULTI_CHAR('c_sl'));
    if (slash != nullptr) slash->hide();

    if (s_counterPane != nullptr) s_counterPane->hide();
    if (s_iconPane != nullptr)    s_iconPane->hide();
    if (s_timePane != nullptr)    s_timePane->show();

    s_getinScreen = new J2DScreen();
    if (s_getinScreen != nullptr &&
        s_getinScreen->setPriority("zelda_game_image_cow_get_in.blo", 0x20000, archive)) {
        dPaneClass_showNullPane(s_getinScreen);

        s_getinBck = static_cast<J2DAnmTransform*>(J2DAnmLoaderDataBase::load(
            JKRGetNameResource("zelda_game_image_cow_get_in.bck", archive)));
        s_getinParent = new CPaneMgr(s_getinScreen, MULTI_CHAR('get_in_n'), 2, NULL);
        s_getinRoot   = new CPaneMgr(s_getinScreen, MULTI_CHAR('n_all'), 0, NULL);
        s_getinText   = new CPaneMgr(s_getinScreen, MULTI_CHAR('get_in'), 0, NULL);

        static_cast<J2DTextBox*>(s_getinScreen->search(MULTI_CHAR('get_in_s')))
            ->setFont(mDoExt_getMesgFont());
        static_cast<J2DTextBox*>(s_getinScreen->search(MULTI_CHAR('get_in')))
            ->setFont(mDoExt_getMesgFont());

        J2DPane* keep[16];
        int keepCount = 0;
        J2DPane* textMain   = s_getinScreen->search(MULTI_CHAR('get_in'));
        J2DPane* textShadow = s_getinScreen->search(MULTI_CHAR('get_in_s'));
        for (J2DPane* p = s_getinText->getPanePtr();
             p != nullptr && keepCount < 16; p = p->getParentPane()) {
            keep[keepCount++] = p;
        }
        for (J2DPane* p = textShadow;
             p != nullptr && keepCount < 16; p = p->getParentPane()) {
            bool already = false;
            for (int i = 0; i < keepCount; i++) already |= (keep[i] == p);
            if (!already) keep[keepCount++] = p;
        }
        prune_getin_extras(s_getinScreen, keep, keepCount, textMain, textShadow);
    } else {
        delete s_getinScreen;
        s_getinScreen = nullptr;
    }

    s_screenReady = true;
    return true;
}

void change_digit(DigitSlot slot, int digit) {
    if (digit < 0 || digit >= 10) digit = 0;
    const char* tex = dMeter2Info_getNumberTextureName(digit);
    dComIfGp_getMain2DArchive()->getResource('TIMG', tex);
    for (int j = 0; j < 2; j++) {
        if (s_digits[slot][j] != nullptr) {
            static_cast<J2DPicture*>(s_digits[slot][j])->changeTexture(tex, 0);
        }
    }
}

void tint_digits(bool gold) {
    if (gold == s_recordTint) return;
    s_recordTint = gold;
    const JUtility::TColor fill(255, gold ? 210 : 255, gold ? 60 : 255, 255);
    const JUtility::TColor white(255, 255, 255, 255);
    for (int i = 0; i < DIGIT_COUNT; i++) {
        for (int j = 0; j < 2; j++) {
            if (s_digits[i][j] != nullptr) {
                static_cast<J2DPicture*>(s_digits[i][j])->setCornerColor(fill, fill, white, white);
            }
        }
    }
}

void play_getin_bck(f32 frame) {
    s_getinParent->getPanePtr()->setAnimation(s_getinBck);
    s_getinBck->setFrame(frame);
    s_getinParent->getPanePtr()->animationTransform();
    s_getinParent->getPanePtr()->setAnimation((J2DAnmTransform*)NULL);
}

void prune_getin_extras(J2DPane* pane, J2DPane** keep, int keepCount,
                        J2DPane* textMain, J2DPane* textShadow) {
    if (pane == nullptr) return;
    bool isKeep = false;
    for (int i = 0; i < keepCount; i++) {
        if (keep[i] == pane) {
            isKeep = true;
            break;
        }
    }
    if (!isKeep) {
        pane->hide();
    } else if (pane != textMain && pane != textShadow) {
        const int kind = pane->getKind();
        if (kind == static_cast<int>(MULTI_CHAR('PICT1'))) {
            static_cast<J2DPicture*>(pane)->setCornerColor(
                JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 0),
                JUtility::TColor(0, 0, 0, 0), JUtility::TColor(0, 0, 0, 0));
        } else if (kind == static_cast<int>(MULTI_CHAR('WIN1'))) {
            static_cast<J2DWindow*>(pane)->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                                         JUtility::TColor(0, 0, 0, 0));
        }
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        prune_getin_extras(child, keep, keepCount, textMain, textShadow);
    }
}

}

void boss_rush_timer_v2_draw(unsigned int cs, bool showingResult, bool isRecord,
                             bool hasBest, unsigned int bestCs) {
    if (!ensure_screen()) return;

    const auto now = std::chrono::steady_clock::now();
    if (!s_haveLastCall ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastCall).count() > 100) {
        s_settle     = 0;
        s_animeFrame = 0;
        s_animeDone  = false;
        s_timeShown  = false;
        s_transY     = 0.0f;
        s_lastDrawn  = 0xFFFFFFFF;
        if (s_timePane != nullptr) s_timePane->setAlphaRate(0.0f);
    }
    s_lastCall = now;
    s_haveLastCall = true;

    if (s_timeShown && s_transY < kRunSlideDistY) {
        s_transY += kRunSlideSpeedY;
        if (s_transY > kRunSlideDistY) s_transY = kRunSlideDistY;
    }

    if (s_timePane != nullptr) {
        if (s_timeShown) {
            s_timePane->paneTrans(kTimerPosX, kTimerPosY + s_transY);
            s_timePane->getPanePtr()->scale(kTimerSizeX, kTimerSizeY);
            s_timePane->setAlphaRate(kParentAlpha * kTimerAlpha);
        } else {
            s_timePane->paneTrans(kTimerPosX - kSlideInDistX, kTimerPosY + s_transY);
            s_timePane->getPanePtr()->scale(kTimerSizeX, kTimerSizeY);
            s_timePane->setAlphaRate(0.0f);
        }
    }

    if (!s_animeDone) {
        if (s_settle > kSettleFrames - 1) {
            if (s_animeFrame <= kAnimeEndFrame) {
                s_animeFrame++;
            } else {
                s_animeDone = true;
            }
            if (s_timePane != nullptr && s_animeFrame <= kSlideInFrames) {
                const f32 t = ease_quad(kSlideInFrames, s_animeFrame);
                s_timePane->paneTrans(kTimerPosX + (1.0f - t) * -kSlideInDistX,
                                      kTimerPosY + s_transY);
                s_timePane->getPanePtr()->scale(kTimerSizeX, kTimerSizeY);
                s_timePane->setAlphaRate(kParentAlpha * (t * kTimerAlpha));
                if (s_animeFrame == kSlideInFrames) {
                    s_timeShown = true;
                }
            }
        } else {
            s_settle++;
        }
    }

    const bool recordBeat =
        showingResult && isRecord && s_animeDone && s_transY >= kRunSlideDistY;
    if (recordBeat) {
        if (!s_recordBeatActive) {
            s_recordBeatActive = true;
            s_recordBeatStart  = now;

            if (s_getinScreen != nullptr && s_getinParent != nullptr) {
                static_cast<J2DTextBox*>(s_getinScreen->search(MULTI_CHAR('get_in_s')))
                    ->setString("NEW RECORD!");
                static_cast<J2DTextBox*>(s_getinScreen->search(MULTI_CHAR('get_in')))
                    ->setString("NEW RECORD!");
                s_getinBckFrame    = 40.0f;
                s_getinFadeFrame   = 0.0f;
                s_getinActive      = true;
            }
        }
        const f32 beatCycle =
            std::chrono::duration<f32>(now - s_recordBeatStart).count() / 1.1f;
        const f32 beatPhase = std::fmod(beatCycle, 1.0f);
        const auto bump = [](f32 x, f32 centre, f32 width) {
            const f32 d = (x - centre) / width;
            return std::exp(-d * d);
        };
        const f32 pulse = 1.0f + 0.16f * (bump(beatPhase, 0.10f, 0.07f) +
                                          0.75f * bump(beatPhase, 0.30f, 0.08f));

        J2DPane* first = s_digits[DIGIT_MIN_TENS][0];
        J2DPane* last  = s_digits[DIGIT_CS_ONES][0];
        if (s_timePane != nullptr && first != nullptr && last != nullptr) {
            if (!s_rowMeasured) {
                Mtx m;
                const Vec tl = s_timePane->getGlobalVtx(first, &m, 0, false, 0);
                const Vec br = s_timePane->getGlobalVtx(last, &m, 3, false, 0);
                s_rowCentreX = (tl.x + br.x) * 0.5f;
                s_rowCentreY = (tl.y + br.y) * 0.5f;
                s_rowMeasured = true;
            }

            s_timePane->paneTrans(kTimerPosX, kTimerPosY + s_transY);
            s_timePane->getPanePtr()->scale(kTimerSizeX * pulse, kTimerSizeY * pulse);
            Mtx m;
            const Vec tl = s_timePane->getGlobalVtx(first, &m, 0, false, 0);
            const Vec br = s_timePane->getGlobalVtx(last, &m, 3, false, 0);
            const f32 dx = s_rowCentreX - (tl.x + br.x) * 0.5f;
            const f32 dy = s_rowCentreY - (tl.y + br.y) * 0.5f;
            s_timePane->paneTrans(kTimerPosX + dx / pulse,
                                  kTimerPosY + s_transY + dy / pulse);
        }
    } else if (s_recordBeatActive) {
        s_recordBeatActive = false;
        s_rowMeasured = false;
    }

    int total = static_cast<int>(cs);
    if (total > 99 * 6000 + 59 * 100 + 99) total = 99 * 6000 + 59 * 100 + 99;
    if (total < 0) total = 0;
    if (static_cast<u32>(total) != s_lastDrawn) {
        s_lastDrawn = static_cast<u32>(total);
        const int min  = total / 6000;
        const int sec  = (total / 100) % 60;
        const int frac = total % 100;
        change_digit(DIGIT_MIN_TENS, min / 10);
        change_digit(DIGIT_MIN_ONES, min % 10);
        change_digit(DIGIT_SEC_TENS, sec / 10);
        change_digit(DIGIT_SEC_ONES, sec % 10);
        change_digit(DIGIT_CS_TENS, frac / 10);
        change_digit(DIGIT_CS_ONES, frac % 10);
    }
    tint_digits(showingResult && isRecord);

    J2DGrafContext* graf_ctx = dComIfGp_getCurrentGrafPort();
    if (graf_ctx == nullptr) return;
    graf_ctx->setup2D();
    s_screen->draw(0.0f, 0.0f, graf_ctx);

    if (hasBest && s_timeShown && s_transY >= kRunSlideDistY &&
        s_timePane != nullptr && s_timePane->getPanePtr() != nullptr) {
        int bTotal = static_cast<int>(bestCs);
        if (bTotal > 99 * 6000 + 59 * 100 + 99) bTotal = 99 * 6000 + 59 * 100 + 99;
        const int bMin = bTotal / 6000;
        const int bSec = (bTotal / 100) % 60;
        const int bFrac = bTotal % 100;

        J2DPane* first = s_digits[DIGIT_MIN_TENS][0];
        J2DPane* last  = s_digits[DIGIT_CS_ONES][0];
        if (first != nullptr && last != nullptr) {
            Mtx m;
            const Vec t1 = s_timePane->getGlobalVtx(first, &m, 0, false, 0);
            const Vec b1 = s_timePane->getGlobalVtx(last, &m, 3, false, 0);
            const f32 rowCx = (t1.x + b1.x) * 0.5f;
            const f32 down = (b1.y - t1.y) + 6.0f;

            change_digit(DIGIT_MIN_TENS, bMin / 10);
            change_digit(DIGIT_MIN_ONES, bMin % 10);
            change_digit(DIGIT_SEC_TENS, bSec / 10);
            change_digit(DIGIT_SEC_ONES, bSec % 10);
            change_digit(DIGIT_CS_TENS, bFrac / 10);
            change_digit(DIGIT_CS_ONES, bFrac % 10);

            const JUtility::TColor goldFill(255, 210, 60, 255);
            const JUtility::TColor normFill(255, 255, 255, 255);
            const JUtility::TColor white(255, 255, 255, 255);
            for (int i = 0; i < DIGIT_COUNT; i++) {
                for (int j = 0; j < 2; j++) {
                    if (s_digits[i][j] != nullptr) {
                        static_cast<J2DPicture*>(s_digits[i][j])
                            ->setCornerColor(goldFill, goldFill, white, white);
                    }
                }
            }

            const f32 sc = 0.8f;
            s_timePane->getPanePtr()->scale(kTimerSizeX * sc, kTimerSizeY * sc);
            s_timePane->paneTrans(kTimerPosX, kTimerPosY + s_transY + down);
            const Vec t2 = s_timePane->getGlobalVtx(first, &m, 0, false, 0);
            const Vec b2 = s_timePane->getGlobalVtx(last, &m, 3, false, 0);
            const f32 dx = rowCx - (t2.x + b2.x) * 0.5f;
            const f32 dy = (b1.y + 6.0f + (b2.y - t2.y) * 0.5f) - (t2.y + b2.y) * 0.5f;
            s_timePane->paneTrans(kTimerPosX + dx / sc, kTimerPosY + s_transY + down + dy / sc);

            s_screen->draw(0.0f, 0.0f, graf_ctx);

            s_timePane->getPanePtr()->scale(kTimerSizeX, kTimerSizeY);
            s_timePane->paneTrans(kTimerPosX, kTimerPosY + s_transY);
            for (int i = 0; i < DIGIT_COUNT; i++) {
                for (int j = 0; j < 2; j++) {
                    if (s_digits[i][j] != nullptr) {
                        static_cast<J2DPicture*>(s_digits[i][j])
                            ->setCornerColor(normFill, normFill, white, white);
                    }
                }
            }
            change_digit(DIGIT_MIN_TENS, (cs / 6000) / 10);
            change_digit(DIGIT_MIN_ONES, (cs / 6000) % 10);
            change_digit(DIGIT_SEC_TENS, (cs / 100) % 60 / 10);
            change_digit(DIGIT_SEC_ONES, (cs / 100) % 60 % 10);
            change_digit(DIGIT_CS_TENS, (cs % 100) / 10);
            change_digit(DIGIT_CS_ONES, (cs % 100) % 10);
        }
    }

    if (s_getinActive && s_getinScreen != nullptr && s_getinParent != nullptr &&
        s_getinRoot != nullptr) {
        if (s_getinBckFrame < 60.0f) {
            s_getinBckFrame += kGetInAnimSpeed;
            if (s_getinBckFrame > 60.0f) s_getinBckFrame = 60.0f;
            play_getin_bck(s_getinBckFrame);
        } else {
            play_getin_bck(60.0f);
        }

        f32 alpha = 1.0f;
        if (!recordBeat) {
            s_getinFadeFrame += 1.0f;
            if (s_getinFadeFrame >= (f32)kGetInAlphaFrames) {
                s_getinActive = false;
            }
            alpha = 1.0f - ease_quad(kGetInAlphaFrames, (int)s_getinFadeFrame);
        }

        s_getinParent->setAlphaRate(alpha);
        s_getinRoot->paneTrans(0.0f, 0.0f);
        s_getinRoot->scale(1.0f, 1.0f);
        s_getinScreen->draw(0.0f, 0.0f, graf_ctx);
    }
}

void boss_rush_timer_v2_shutdown() {
    destroy_screen();
}
