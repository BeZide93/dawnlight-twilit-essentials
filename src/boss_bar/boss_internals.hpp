#pragma once
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_name.h"

#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"
#include "Z2AudioLib/Z2Creature.h"
#include "Z2AudioLib/Z2SoundObject.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "d/d_kankyo.h"

#define private public
#define protected public
#include "d/actor/d_a_e_gob.h"
#include "d/actor/d_a_e_mk.h"
#include "d/actor/d_a_b_bh.h"
#include "d/actor/d_a_b_tn.h"
#include "d/actor/d_a_b_ds.h"
#include "d/actor/d_a_b_yo.h"
#include "d/actor/d_a_e_fm.h"
#include "d/actor/d_a_b_gm.h"
#include "d/actor/d_a_b_bq.h"
#include "d/actor/d_a_b_ob.h"
#include "d/actor/d_a_b_oh.h"
#include "d/actor/d_a_b_dr.h"
#include "d/actor/d_a_e_vt.h"
#include "d/actor/d_a_e_pz.h"
#include "d/actor/d_a_b_zant.h"
#include "d/actor/d_a_b_gnd.h"
#include "d/actor/d_a_b_mgn.h"
#include "d/actor/d_a_e_th.h"
#include "d/actor/d_a_e_th_ball.h"
#include "d/actor/d_a_e_dt.h"
#include "d/actor/d_a_e_ph.h"
#undef private
#undef protected

namespace bbi {

inline f32 clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline void beastganon_read(fopAc_ac_c* a, int& act, int& hp, bool& isDown) {
    const daB_MGN_c* g = reinterpret_cast<const daB_MGN_c*>(a);
    act = g->mActionMode;
    hp = a->health;
    isDown = (g->mActionMode == daB_MGN_c::ACTION_DOWN_e ||
              g->mActionMode == daB_MGN_c::ACTION_DOWN_DAMAGE_e ||
              g->mActionMode == daB_MGN_c::ACTION_DOWN_BITE_DAMAGE_e ||
              g->mDownFlag != 0);
}

inline void beastganon_progress(fopAc_ac_c* a, int& aff, bool& down, int& downTimer) {
    const daB_MGN_c* g = reinterpret_cast<const daB_MGN_c*>(a);
    aff = g->field_0xaff;
    down = g->field_0xafd != 0;
    downTimer = g->field_0xaa0;
}

inline void beastganon_read(fopAc_ac_c* a, int& act, int& hp, bool& isDown, int& pwr, int& invuln,
                            int& aa8, int& a9c, int& b00, int& afd) {
    beastganon_read(a, act, hp, isDown);
    const daB_MGN_c* g = reinterpret_cast<const daB_MGN_c*>(a);
    pwr = g->mAtInfo.mAttackPower;
    invuln = g->mDamageInvulnerabilityTimer;
    aa8 = g->field_0xaa8;
    a9c = g->field_0xa9c;
    b00 = g->field_0xb00;
    afd = g->field_0xafd;
}

inline bool beastganon_defeated(fopAc_ac_c* a) {
    return false;
}
inline bool beastganon_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const daB_MGN_c*>(a)->mpMgnModelMorf != nullptr;
}
inline void beastganon_skip_opening(fopAc_ac_c* a) {
    daB_MGN_c* mgn = reinterpret_cast<daB_MGN_c*>(a);
    mgn->demo_skip(0);
    mgn->current.pos.set(0.0f, 0.0f, 90.0f);
    mgn->current.angle.set(0, (s16)0x8000, 0);
    mgn->shape_angle.set(0, (s16)0x8000, 0);
    mgn->field_0xb14 = (s16)0x8000;
    mgn->field_0xb16 = (s16)0x8000;
    mgn->field_0xb18 = 0;
    mgn->mtx_set();
    mgn->mDemoCamCenter.set(0.0f, -180.0f, -2090.0f);
    mgn->mDemoCamEye.set(0.0f, -200.0f, -3090.0f);
}

inline bool beastganon_engaged(fopAc_ac_c* a) {
    const daB_MGN_c* g = reinterpret_cast<const daB_MGN_c*>(a);
    if (g->mActionMode == daB_MGN_c::ACTION_OPENING_e) return false;
    if (g->mActionMode == daB_MGN_c::ACTION_DEATH_e) return false;
    return true;
}

inline f32 beastganon_ratio(fopAc_ac_c* a) {
    const daB_MGN_c* g = reinterpret_cast<const daB_MGN_c*>(a);
    if (g->mActionMode == daB_MGN_c::ACTION_DEATH_e) return 0.0f;
    s16 hp = a->health; if (hp < 0) hp = 0; if (hp > 700) hp = 700;
    return clamp01(static_cast<f32>(hp) / 700.0f);
}

inline f32 darknut_ratio(fopAc_ac_c* a) {
    const daB_TN_c* t = reinterpret_cast<const daB_TN_c*>(a);
    if (t->mActionMode1 < 8) {
        return 1.0f - clamp01(static_cast<f32>(t->mNextBreakPart) / 12.0f);
    }
    int mx = t->field_0x700;
    if (mx <= 0) mx = 360;
    return 1.0f - clamp01(static_cast<f32>(t->field_0x6fc) / static_cast<f32>(mx));
}
inline void darknut_dbg(fopAc_ac_c* a, int& mode, int& brk, int& cur, int& mx) {
    const daB_TN_c* t = reinterpret_cast<const daB_TN_c*>(a);
    mode = t->mActionMode1; brk = t->mNextBreakPart; cur = t->field_0x6fc; mx = t->field_0x700;
}

inline f32 aeralfos_ratio(fopAc_ac_c* a) {
    if (a == nullptr) return 1.0f;
    int mx = a->field_0x560;
    if (mx <= 0) mx = 700;
    return 1.0f - clamp01(static_cast<f32>(mx - a->health) / static_cast<f32>(mx));
}

inline f32 blizzeta_ratio(fopAc_ac_c* a) {
    const daB_YO_c* y = reinterpret_cast<const daB_YO_c*>(a);
    int model = y->mModelNo;
    if (model >= 7) {
        int d = y->mLastPhaseDamage; if (d < 0) d = 0; if (d > 3) d = 3;
        return 1.0f - static_cast<f32>(d) / 3.0f;
    }
    if (model < 1) model = 1;
    return 1.0f - clamp01(static_cast<f32>(model - 1) / 6.0f);
}
inline void blizzeta_dbg(fopAc_ac_c* a, int& model, int& lpd) {
    const daB_YO_c* y = reinterpret_cast<const daB_YO_c*>(a);
    model = y->mModelNo; lpd = y->mLastPhaseDamage;
}
inline void blizzeta_read(fopAc_ac_c* a, int& model, int& lpd, int& act) {
    const daB_YO_c* y = reinterpret_cast<const daB_YO_c*>(a);
    model = y->mModelNo; lpd = y->mLastPhaseDamage; act = y->mAction;
}

inline f32 stallord_ratio(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    if (d->mBossPhase == 0) {
        int lvl = d->mBackboneLevel; if (lvl < 0) lvl = 0; if (lvl > 3) lvl = 3;
        return 1.0f - static_cast<f32>(lvl) / 3.0f;
    }
    s16 hp = a->health; if (hp < 0) hp = 0;
    return clamp01(static_cast<f32>(hp) / 1080.0f);
}
inline bool stallord_engaged(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    if (d->mDead) return false;
    if (d->mBossPhase != 0) return true;
    if (d->mBackboneLevel > 0) return true;
    if (d->mIsDemo || d->mIsOpeningDemo || d->mAction == 1) return false;
    return d->mAction == 2 || d->mAction == 4 || d->mAction == 5 ||
           d->mAction == 6 || d->mAction == 7;
}
inline void stallord_dbg(fopAc_ac_c* a, int& phase, int& bone, int& hp, int& act) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    phase = d->mBossPhase; bone = d->mBackboneLevel; hp = a->health; act = d->mAction;
}
inline void stallord_read(fopAc_ac_c* a, int& phase, int& bone, int& hp, int& act,
                          bool& dead, bool& demo) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    phase = d->mBossPhase; bone = d->mBackboneLevel; hp = a->health; act = d->mAction;
    dead = d->mDead;
    if (phase == 0) {
        demo = d->mIsDemo || d->mIsOpeningDemo || d->mAction == 1;
    } else {
        demo = d->mIsDemo || d->mAction == 0 || d->mAction == 5;
    }
}

inline bool blizzeta_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const daB_YO_c*>(a)->mpModel[0] != nullptr;
}
inline bool blizzeta_in_opening(fopAc_ac_c* a) {
    return reinterpret_cast<const daB_YO_c*>(a)->mAction == 0;
}

inline bool stallord_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const daB_DS_c*>(a)->mpMorf != nullptr;
}
inline bool stallord_in_opening(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    return d->mAction == 1 || d->mIsOpeningDemo;
}

inline bool stallord_p1_death_demo(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    return d->mBossPhase == 0 && d->mAction == 3 &&
           d->mMode >= 10 && d->mDead;
}
inline bool stallord_p2_in_opening(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    return d->mBossPhase != 0 && d->mBossPhase != 100 &&
           d->mAction == 0;
}
inline bool stallord_p2_ready(fopAc_ac_c* a) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    return d->mBossPhase != 0 && d->mBossPhase != 100 && d->mAction >= 1;
}
inline void stallord_prep_phase2_wait(fopAc_ac_c* a) {
    daB_DS_c* d = reinterpret_cast<daB_DS_c*>(a);
    d->mSetFirstPos();
    d->attention_info.distances[fopAc_attn_BATTLE_e] = 0;
    d->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
    fopAcM_SetGroup(d, 2);
    d->gravity = 0.0f;
    d->speed.y = 0.0f;
    d->setActionMode(1, 0);
}
inline void stallord_read_bitsw(fopAc_ac_c* a, int& sw1, int& sw2, int& sw3) {
    const daB_DS_c* d = reinterpret_cast<const daB_DS_c*>(a);
    sw1 = d->bitSw; sw2 = d->bitSw2; sw3 = d->bitSw3;
}

inline bool fyrus_defeated(fopAc_ac_c* a);
inline f32 fyrus_ratio(fopAc_ac_c* a) {
    const e_fm_class* f = reinterpret_cast<const e_fm_class*>(a);
    int downs = f->mDownCnt; if (downs < 0) downs = 0; if (downs > 3) downs = 3;
    if (downs >= 3) {
        return fyrus_defeated(a) ? 0.0f : (1.0f / 3.0f);
    }
    s16 hp = a->health;
    f32 frac = (hp <= 0) ? 0.0f : (hp >= 50 ? 1.0f : static_cast<f32>(hp) / 50.0f);
    return clamp01(((3.0f - static_cast<f32>(downs)) + frac) / 3.0f);
}
inline void fyrus_dbg(fopAc_ac_c* a, int& downs, int& hp, int& act, int& demo) {
    const e_fm_class* f = reinterpret_cast<const e_fm_class*>(a);
    downs = f->mDownCnt; hp = a->health; act = f->mAction; demo = f->mDemoCamMode;
}
inline bool fyrus_engaged(fopAc_ac_c* a) {
    const e_fm_class* f = reinterpret_cast<const e_fm_class*>(a);
    if (a->health <= 0) return false;
    if (f->mDemoCamMode != 0) return false;
    s16 act = f->mAction;
    return act >= 0 && act <= 10;
}
inline bool fyrus_defeated(fopAc_ac_c* a) {
    return reinterpret_cast<const e_fm_class*>(a)->mAction == 12;
}
inline bool fyrus_model_ready(fopAc_ac_c* a)   { return reinterpret_cast<const e_fm_class*>(a)->mpFmModelMorf != nullptr; }
inline bool fyrus_in_intro(fopAc_ac_c* a) {
    const e_fm_class* f = reinterpret_cast<const e_fm_class*>(a);
    return f->mAction == 11 || f->mDemoCamMode != 0;
}
inline void fyrus_force_fight_start(fopAc_ac_c* a) {
    e_fm_class* f = reinterpret_cast<e_fm_class*>(a);
    f->mAction = 0;
    f->mMode = -10;
    f->mTimers[0] = 20;
    f->mDemoCamMode = 0;
    Z2GetAudioMgr()->bgmStart(Z2BGM_BOSSFIREMAN_0, 0, 0);
    f->field_0x5c8 = 2;
    f->mKankyoBlend = 0.0f;
    f->field_0x792 = 1;
    if (f->mpCoreBtk != nullptr) {
        f->mpCoreBtk->setPlaySpeed(1.0f);
    }
    f->field_0x770 = 1;
    f->field_0x1b080 = 1;
    f->mPlayTexAnmNo = 0;
}

inline bool dekutoad_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const daE_DT_c*>(a)->mpMorf != nullptr;
}
inline bool dekutoad_in_opening(fopAc_ac_c* a) {
    const daE_DT_c* d = reinterpret_cast<const daE_DT_c*>(a);
    return d->mAction == 0xA || d->mDemoMode != 0;
}
inline void dekutoad_force_fight_start(fopAc_ac_c* a) {
    daE_DT_c* d = reinterpret_cast<daE_DT_c*>(a);
    d->mAction = 0;
    d->mMode = 0;
    d->mDemoMode = 0;
    d->field_0x714 = 0;
    d->field_0x781 = false;
    d->gravity = -5.0f;
    d->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
}

inline f32 armogohma_ratio(fopAc_ac_c* a) {
    const b_gm_class* g = reinterpret_cast<const b_gm_class*>(a);
    return 1.0f - clamp01(static_cast<f32>(g->mHitCount) / 3.0f);
}
inline bool armogohma_engaged(fopAc_ac_c* a) {
    const b_gm_class* g = reinterpret_cast<const b_gm_class*>(a);
    if (g->mHitCount > 0) return true;
    if (g->mDemoMode != 0) return false;
    return g->mAction != 0;
}
inline void armogohma_dbg(fopAc_ac_c* a, int& hits, int& act, int& demo) {
    const b_gm_class* g = reinterpret_cast<const b_gm_class*>(a);
    hits = g->mHitCount; act = g->mAction; demo = g->mDemoMode;
}

inline f32 deathsword_ratio(fopAc_ac_c* a) {
    const daE_VA_c* v = reinterpret_cast<const daE_VA_c*>(a);
    if (v->mAction == daE_VA_c::ACTION_OPACI_DEATH_e || g_dComIfG_gameInfo.info.getMemory().getBit().isStageBossEnemy2()) {
        return 0.0f;
    }
    const f32 total = 800.0f;
    int raw = v->field_0x1364;
    if (raw < 0) raw = 0;
    if (raw > (int)total) raw = (int)total;
    return 1.0f - clamp01(static_cast<f32>(raw) / total);
}
inline bool deathsword_defeated(fopAc_ac_c* a) {
    const daE_VA_c* v = reinterpret_cast<const daE_VA_c*>(a);
    return v->mAction == daE_VA_c::ACTION_OPACI_DEATH_e || g_dComIfG_gameInfo.info.getMemory().getBit().isStageBossEnemy2();
}
inline bool deathsword_engaged(fopAc_ac_c* a) {
    const daE_VA_c* v = reinterpret_cast<const daE_VA_c*>(a);
    if (v->mAction == daE_VA_c::ACTION_OPACI_DEATH_e) return false;
    return v->mAction >= daE_VA_c::ACTION_TRANS_WAIT_e;
}
inline bool deathsword_ropes_cut(fopAc_ac_c* a) {
    const daE_VA_c* v = reinterpret_cast<const daE_VA_c*>(a);
    return v->mAction >= daE_VA_c::ACTION_CLEAR_WAIT_e;
}
inline bool deathsword_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const daE_VA_c*>(a)->mpMorf != nullptr;
}
inline bool deathsword_in_intro(fopAc_ac_c* a) {
    return reinterpret_cast<const daE_VA_c*>(a)->mAction < daE_VA_c::ACTION_CLEAR_WAIT_e;
}
inline void deathsword_dbg(fopAc_ac_c* a, int& dmg, int& delta, int& action) {
    const daE_VA_c* v = reinterpret_cast<const daE_VA_c*>(a);
    dmg = v->field_0x1364; delta = v->field_0x1368; action = v->mAction;
}

inline f32 dangoro_ratio(fopAc_ac_c* a) {
    const e_gob_class* g = reinterpret_cast<const e_gob_class*>(a);
    int thrown = g->field_0x6d8;
    if (thrown <= 0) return 1.0f;
    if (thrown > 3) thrown = 3;
    int bounce = g->field_0x6d6 + 1;
    if (bounce < 1) bounce = 1;
    if (bounce > 3) bounce = 3;
    f32 used = (static_cast<f32>((thrown - 1) * 3 + bounce)) / 9.0f;
    return 1.0f - clamp01(used);
}
inline void dangoro_dbg(fopAc_ac_c* a, int& thrown, int& bounce) {
    const e_gob_class* g = reinterpret_cast<const e_gob_class*>(a);
    thrown = g->field_0x6d8; bounce = g->field_0x6d6;
}
inline bool dangoro_defeated(fopAc_ac_c* a) {
    return reinterpret_cast<const e_gob_class*>(a)->field_0x6da != 0;
}
inline bool dangoro_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const e_gob_class*>(a)->mpModelMorf != nullptr;
}
inline bool dangoro_awaiting_lower(fopAc_ac_c* a) {
    return reinterpret_cast<const e_gob_class*>(a)->mAction != 2;
}
inline void dangoro_force_fight_start(fopAc_ac_c* a) {
    e_gob_class* g = reinterpret_cast<e_gob_class*>(a);
    a->current.pos.set(0.0f, 1000.0f, -300.0f);
    a->old.pos = a->current.pos;
    a->speed.set(0.0f, 0.0f, 0.0f);
    a->shape_angle.x = 0;
    a->shape_angle.z = 0;
    g->mAction = 2;
    g->mMode = -1;
}

inline f32 morpheel_ratio(fopAc_ac_c* a) {
    const b_ob_class* o = reinterpret_cast<const b_ob_class*>(a);
    const bool phase2 = (o->mAction >= 100) || (o->mFishBattleMode != 0);
    if (!phase2) {
        s16 hp = a->health; if (hp < 0) hp = 0;
        return clamp01(static_cast<f32>(hp) / 30.0f);
    }
    int fin = o->mHangFinishCount; if (fin < 0) fin = 0; if (fin > 3) fin = 3;
    int hit = o->mHangHitCount;   if (hit < 0) hit = 0; if (hit > 4) hit = 4;
    return 1.0f - clamp01(static_cast<f32>(fin * 4 + hit) / 12.0f);
}
inline void morpheel_dbg(fopAc_ac_c* a, int& act, int& fish, int& fin, int& hit,
                         int& demo, int& hp, int& coreMode) {
    const b_ob_class* o = reinterpret_cast<const b_ob_class*>(a);
    act = o->mAction; fish = o->mFishBattleMode; fin = o->mHangFinishCount; hit = o->mHangHitCount;
    demo = o->mDemoAction; hp = a->health; coreMode = o->mCoreBattleMode;
}
inline bool morpheel_model_ready(fopAc_ac_c* a) {
    return reinterpret_cast<const b_ob_class*>(a)->mpCoreMorf != nullptr;
}
inline bool morpheel_tentacles_ready(fopAc_ac_c* a) {
    const b_ob_class* o = reinterpret_cast<const b_ob_class*>(a);
    for (int i = 0; i < 8; i++) {
        fopAc_ac_c* oh = fopAcM_SearchByID(o->mTentacleActorIDs[i]);
        if (oh == nullptr) return false;
        if (reinterpret_cast<const b_oh_class*>(oh)->mAction == OH_ACTION_START) return false;
    }
    return true;
}

inline bool morpheel_is_phase2(fopAc_ac_c* a) {
    const b_ob_class* o = reinterpret_cast<const b_ob_class*>(a);
    return (o->mAction >= 100) || (o->mFishBattleMode != 0);
}

inline bool diababa_head_risen(fopAc_ac_c* bq) {
    return reinterpret_cast<const b_bq_class*>(bq)->mDisableDraw == 0;
}

inline void argorok_read(fopAc_ac_c* a, int& mode, int& ph, int& parts, int& hp,
                         int& weekHits, int& arg0, int& anm) {
    const daB_DR_c* d = reinterpret_cast<const daB_DR_c*>(a);
    mode = d->mActionMode; ph = d->field_0x7d1; parts = d->mBreakPartsNo;
    hp = a->health; weekHits = d->field_0x7e8; arg0 = d->arg0; anm = d->mAnm;
}
inline bool argorok_flat(int anm) {
    return anm == 0x37 || anm == 0x13 || anm == 0x1F;
}

inline void argorok_force_arg0(fopAc_ac_c* a, int value) {
    daB_DR_c* d = reinterpret_cast<daB_DR_c*>(a);
    d->arg0 = value;
}

inline bool argorok_in_phase2_cutscene(fopAc_ac_c* a) {
    const daB_DR_c* d = reinterpret_cast<const daB_DR_c*>(a);
    if (d->mpModelMorf == nullptr) return false;
    if (d->mBreakPartsNo < 2) return false;

    if (fopAcM_SearchByName(fpcNm_B_DRE_e) != nullptr || d->parentActorID != 0) {
        return true;
    }

    if (d->mMoveMode >= 1000 || d->arg0 == 0xFE || dComIfGs_isZoneSwitch(23, fopAcM_GetRoomNo(a))) {
        return true;
    }

    if (d->field_0x7d1 == 2 && (d->mAnm == 0x41 || d->current.pos.y < d->home.pos.y + 4000.0f)) {
        return true;
    }

    return false;
}

inline fpc_ProcID argorok_get_parent(fopAc_ac_c* a) {
    const daB_DR_c* d = reinterpret_cast<const daB_DR_c*>(a);
    return d->parentActorID;
}

inline void argorok_force_phase2(fopAc_ac_c* a) {
    daB_DR_c* d = reinterpret_cast<daB_DR_c*>(a);
    d->arg0 = 1;
    d->field_0x7d1 = 2;
    d->parentActorID = 0;
    d->mBreakPartsNo = 3;
    d->mTargetHeight = 6000.0f + d->home.pos.y;
    d->current.pos.set(d->home.pos.x, d->home.pos.y + 6000.0f, d->home.pos.z);
    d->old.pos = d->current.pos;
    d->speed.set(0.0f, 0.0f, 0.0f);
    d->speedF = 0.0f;
    d->mBoot_c_trance.zero();
    d->field_0x724 = 100.0f;
    d->field_0x750 = 200;
    d->field_0x7d6 = 0;
    d->mActionMode = 1;
    d->mMoveMode = 2;
    d->mTimer[2] = 120;
    if (d->mpModelMorf != nullptr && dComIfG_getObjectRes("B_DR", 0x35) != nullptr) {
        d->setBck(0x35, 2, 5.0f, 1.0f);
    }
    d->chkPartCreate(1);
}

inline void peahat_snap_phase2(fopAc_ac_c* a) {
    daE_PH_c* ph = reinterpret_cast<daE_PH_c*>(a);
    if (ph->mAction == 4 || ph->mAction == 5) {
        fopAcM_delete(ph);
    } else if (ph->mAction == 2) {
        ph->field_0x5b2 = 1;
        ph->current.pos = ph->home.pos;
        ph->old.pos = ph->home.pos;
        ph->speed.set(0.0f, 0.0f, 0.0f);
        ph->speedF = 0.0f;
        ph->mCAction = 0;
        ph->field_0x5ae = 0;
        ph->mCamAction = 4;
        ph->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        ph->attention_info.distances[fopAc_attn_BATTLE_e] = 0x52;
    }
}

inline bool phantomzant_ignore(fopAc_ac_c* a) {
    const daE_PZ_c* p = reinterpret_cast<const daE_PZ_c*>(a);
    return p->arg0 >= 10 || p->mActionMode == daE_PZ_c::ACTION_DEAD_e;
}
inline bool phantomzant_engaged(fopAc_ac_c* a) {
    const daE_PZ_c* p = reinterpret_cast<const daE_PZ_c*>(a);
    if (a->health <= 1) return false;
    return p->mActionMode != daE_PZ_c::ACTION_OPENING_DEMO_e &&
           p->mActionMode != daE_PZ_c::ACTION_DEAD_e;
}
inline void zant_read(fopAc_ac_c* a, int& phase, int& act, int& lastAct,
                      int& cycle, int& hp, bool& bigDmg) {
    const daB_ZANT_c* z = reinterpret_cast<const daB_ZANT_c*>(a);
    phase = z->mFightPhase; act = z->mAction; lastAct = z->mLastAction;
    cycle = z->mFightCycle; hp = a->health; bigDmg = z->mTakenBigDmg != 0;
}
inline void ganondorf_read(fopAc_ac_c* a, int& actionMode, int& moveMode,
                           int& demoCam, int& drawHorse, int& hp, int& knockdowns) {
    const b_gnd_class* g = reinterpret_cast<const b_gnd_class*>(a);
    actionMode = g->mActionMode; moveMode = g->mMoveMode; demoCam = g->mDemoCamMode;
    drawHorse = g->mDrawHorse;   hp = a->health;
    knockdowns = g->field_0x1e0c;
}
inline bool ganondorf_horse_demo_pending(fopAc_ac_c* a) {
    const b_gnd_class* g = reinterpret_cast<const b_gnd_class*>(a);
    return g->mNoDrawTimer != 0 || (g->mActionMode >= 1 && g->mActionMode <= 6);
}
inline void ganondorf_force_ground_duel(fopAc_ac_c* a) {
    b_gnd_class* g = reinterpret_cast<b_gnd_class*>(a);
    g->mNoDrawTimer = 0;
    g->mActionMode  = 10;
    g->mMoveMode    = 0;
    g->mDrawHorse   = 0;
    g->mDemoCamMode = 0;
    if (a->health <= 0) a->health = 100;
}

inline f32 ganondorf_ratio(fopAc_ac_c* a) {
    const b_gnd_class* g = reinterpret_cast<const b_gnd_class*>(a);
    s16 hp = a->health; if (hp < 0) hp = 0;
    f32 cap = (g->mDrawHorse != 0 || (g->mActionMode >= 1 && g->mActionMode <= 6)) ? 24.0f : 100.0f;
    return clamp01(static_cast<f32>(hp) / cap);
}

inline f32 zant_ratio(fopAc_ac_c* a) {
    const daB_ZANT_c* z = reinterpret_cast<const daB_ZANT_c*>(a);
    if (z->mFightPhase == daB_ZANT_c::PHASE_OP || z->mAction == daB_ZANT_c::ACT_OPENING)
        return 1.0f;
    s16 hp = a->health; if (hp < 0) hp = 0;
    f32 cap = (z->mFightPhase == daB_ZANT_c::PHASE_LAST) ? 600.0f : 280.0f;
    return clamp01(static_cast<f32>(hp) / cap);
}

inline f32 argorok_ratio(fopAc_ac_c* a) {
    const daB_DR_c* d = reinterpret_cast<const daB_DR_c*>(a);
    if (d->field_0x7d1 != 2) return 1.0f - clamp01(static_cast<f32>(d->mBreakPartsNo) / 2.0f);
    s16 hp = a->health; if (hp < 0) hp = 0;
    return clamp01(static_cast<f32>(hp) / 24.0f);
}

inline bool darkhammer_awaiting_walkin(fopAc_ac_c* a) {
    const e_th_class* t = reinterpret_cast<const e_th_class*>(a);
    return t->mAction == 22 || t->mNoDraw != 0;
}
inline void darkhammer_force_fight_start(fopAc_ac_c* a) {
    e_th_class* t = reinterpret_cast<e_th_class*>(a);
    if (t->mNoDraw != 0 || t->mAction == 22) {
        t->current.pos = t->home.pos;
        t->old.pos = t->home.pos;
        t->mNoDraw = 0;
        t->mAction = 0;
        t->mMode = 0;
    }

    fopAc_ac_c* ballAc = fopAcM_SearchByID(t->mBallID);
    if (ballAc == nullptr) {
        ballAc = fopAcM_SearchByName(fpcNm_E_TH_BALL_e);
    }
    if (ballAc != nullptr) {
        e_th_ball_class* ball = reinterpret_cast<e_th_ball_class*>(ballAc);
        ball->mPlayerGet = 0;
    }

    g_dComIfG_gameInfo.info.onSwitch(106, fopAcM_GetRoomNo(t));
}

inline bool ook_intro_pending(fopAc_ac_c* a) {
    const e_mk_class* m = reinterpret_cast<const e_mk_class*>(a);
    return m->action == e_mk_class::ACT_S_DEMO;
}
inline void ook_skip_intro(fopAc_ac_c* a) {
    e_mk_class* m = reinterpret_cast<e_mk_class*>(a);
    m->demoMode = e_mk_class::DEMO_MODE_FINISH;
    m->demoSubMode = 0;
    m->action = e_mk_class::ACT_MOVE;
    m->mode = 0;
}

inline s16  diababa_demo_mode(fopAc_ac_c* a) { return reinterpret_cast<const b_bq_class*>(a)->mDemoMode; }
inline s16  diababa_action(fopAc_ac_c* a)    { return reinterpret_cast<const b_bq_class*>(a)->mAction; }
inline void diababa_mark_skipped(fopAc_ac_c* a) { reinterpret_cast<b_bq_class*>(a)->mDemoMode = 1000; }

}
