#include "boss_rush_music.hpp"

#include "boss_rush.hpp"
#include "boss_rush_gamemode.hpp"

#include "mods/hook.hpp"

#include "f_ap/f_ap_game.h"
#include "f_op/f_op_overlap_mng.h"
#include "m_Do/m_Do_audio.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "Z2AudioLib/Z2SoundInfo.h"

DEFINE_HOOK(&fapGm_Execute, BossRushMusicExecuteHook);
DEFINE_HOOK(&Z2SeqMgr::bgmStart, BossRushMusicBgmStartHook);
DEFINE_HOOK(&Z2SeqMgr::bgmStop, BossRushMusicBgmStopHook);

namespace {

// spirit.ast
constexpr u32 kSpiritStreamId = 0x200005F;
constexpr u32 kNoStream = 0xFFFFFFFFu;
constexpr int kLeaveDelayFrames = 8;
constexpr int kFadeOutFrames = 20;
constexpr int kRetryDelayFrames = 30;
constexpr int kTransitionTimeoutFrames = 900;

bool s_inChamber = false;
int s_leaveTimer = 0;
int s_retryTimer = 0;
int s_transitionFrames = 0;
u32 s_yieldStreamId = kNoStream;

Z2AudioMgr* audio_mgr() {
    Z2AudioMgr* audio = Z2GetAudioMgr();
    return (audio != nullptr && mDoAud_zelAudio_c::isInitFlag()) ? audio : nullptr;
}

bool scene_bgm_started() {
    return !fopOvlpM_IsPeek() && !mDoAud_zelAudio_c::isBgmSet();
}

bool stream_available(Z2AudioMgr* audio) {
    Z2SoundInfo* info = &audio->mSoundInfo;
    const int type = info->getSoundType(kSpiritStreamId);
    const s32 entry = info->getStreamFileEntry(kSpiritStreamId, nullptr);
    if (type != 2 || entry < 0) {
        return false;
    }
    return true;
}

bool spirit_stream_playing(Z2AudioMgr* audio) {
    return audio->getStreamBgmID() == kSpiritStreamId;
}

bool yielded_stream_playing(Z2AudioMgr* audio) {
    return s_yieldStreamId != kNoStream && audio->getStreamBgmID() == s_yieldStreamId;
}

bool protecting_spirit_stream() {
    Z2AudioMgr* audio = audio_mgr();
    if (audio == nullptr || !spirit_stream_playing(audio) || !boss_rush_game_mode_is_active()) {
        return false;
    }
    return s_transitionFrames > 0 || (s_inChamber && is_in_boss_rush_chamber());
}

HookAction on_bgm_start_pre(ModContext*, void* args, void*, void*) {
    if (mods::arg<u32>(args, 1) == Z2BGM_DUNGEON_LV6 && protecting_spirit_stream()) {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction on_bgm_stop_pre(ModContext*, void* args, void*, void*) {
    if (!protecting_spirit_stream()) {
        return HOOK_CONTINUE;
    }
    Z2SeqMgr* seq = mods::arg<Z2SeqMgr*>(args, 0);
    const u32 fadeTime = mods::arg<u32>(args, 1);
    const s32 keepSub = mods::arg<s32>(args, 2);
    if (seq->mMainBgmHandle) {
        seq->mMainBgmHandle->stop(fadeTime);
    }
    if (keepSub == 0) {
        if (seq->mSubBgmHandle) {
            seq->mSubBgmHandle->stop(fadeTime);
        }
        seq->mMainBgmMaster.forceIn();
    }
    return HOOK_SKIP_ORIGINAL;
}

void start_music(Z2AudioMgr* audio) {
    audio->bgmAllUnMute(0);
    audio->setTwilightGateVol(1.0f);
    audio->unMuteSceneBgm(0);
    if (!stream_available(audio)) {
        s_retryTimer = kRetryDelayFrames;
        return;
    }
    audio->bgmStreamPrepare(kSpiritStreamId);
    audio->bgmStreamPlay();
    if (!spirit_stream_playing(audio)) {
        s_retryTimer = kRetryDelayFrames;
    }
}

void update_transition(Z2AudioMgr* audio, bool inChamber) {
    if (s_transitionFrames <= 0) {
        return;
    }
    if (!boss_rush_game_mode_is_active()) {
        s_transitionFrames = 0;
    } else if (inChamber && scene_bgm_started()) {
        s_transitionFrames = 0;
        return;
    } else {
        --s_transitionFrames;
    }
    if (s_transitionFrames == 0 && !inChamber && spirit_stream_playing(audio)) {
        audio->bgmStreamStop(kFadeOutFrames);
    }
    if (s_transitionFrames > 0 && spirit_stream_playing(audio) &&
        audio->mAllBgmMaster.getDest() < 1.0f) {
        audio->bgmAllUnMute(0);
    }
}

void update_music() {
    Z2AudioMgr* audio = audio_mgr();
    if (audio == nullptr) {
        return;
    }

    const bool inChamber = boss_rush_game_mode_is_active() && is_in_boss_rush_chamber();
    update_transition(audio, inChamber);
    if (!inChamber) {
        if (s_inChamber && --s_leaveTimer <= 0) {
            s_inChamber = false;
            if (spirit_stream_playing(audio)) {
                audio->bgmStreamStop(kFadeOutFrames);
            }
        }
        return;
    }

    s_leaveTimer = kLeaveDelayFrames;
    if (!s_inChamber) {
        s_inChamber = true;
        s_retryTimer = 0;
    }

    if (s_retryTimer > 0) {
        --s_retryTimer;
        return;
    }
    if (spirit_stream_playing(audio) || yielded_stream_playing(audio) || !scene_bgm_started()) {
        return;
    }
    start_music(audio);
}

void on_execute_post(ModContext*, void*, void*, void*) {
    update_music();
}

}  // namespace

ModResult init_boss_rush_music(const HookService* hook_svc, const LogService*, ModContext*) {
    if (hook_svc == nullptr) {
        return MOD_ERROR;
    }
    ModResult result = mods::hook::add_post<BossRushMusicExecuteHook>(hook_svc, on_execute_post);
    if (result == MOD_OK) {
        result = mods::hook::add_pre<BossRushMusicBgmStartHook>(hook_svc, on_bgm_start_pre);
    }
    if (result == MOD_OK) {
        result = mods::hook::add_pre<BossRushMusicBgmStopHook>(hook_svc, on_bgm_stop_pre);
    }
    return result;
}

void shutdown_boss_rush_music() {
    Z2AudioMgr* audio = audio_mgr();
    if (audio != nullptr && spirit_stream_playing(audio)) {
        audio->bgmStreamStop(kFadeOutFrames);
    }
    s_inChamber = false;
    s_leaveTimer = 0;
    s_retryTimer = 0;
    s_transitionFrames = 0;
    s_yieldStreamId = kNoStream;
}

void boss_rush_music_yield_to_stream(uint32_t streamId) {
    s_yieldStreamId = streamId;
}

void boss_rush_music_begin_chamber_transition() {
    Z2AudioMgr* audio = audio_mgr();
    if (audio == nullptr) {
        return;
    }
    s_transitionFrames = kTransitionTimeoutFrames;
    s_retryTimer = 0;
    audio->mSoundMgr.getSeqMgr()->stop(0);
    if (spirit_stream_playing(audio)) {
        return;
    }
    audio->mSoundMgr.getStreamMgr()->stop(0);
    start_music(audio);
}
