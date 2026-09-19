#include "puppet_zelda_pattern.hpp"
#include "m_Do/m_Do_ext.h"
#include "d/actor/d_a_e_hzelda.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

bool g_configPuppetZeldaPatternEnabled = false;
bool g_configPuppetZeldaAlwaysShortest = false;

enum {
    ACTION_WAIT = 0,
    ACTION_ATTACK_A = 1,
    ACTION_ATTACK_B = 2,
    ACTION_ATTACK_C = 3,
    ACTION_DAMAGE = 4,
};

static fpc_ProcID s_lastZeldaId = fpcM_ERROR_PROCESS_ID_e;
static s16 s_prevAction = ACTION_WAIT;
static int s_zeldaPatternStep = 0;

static const bool s_isBallStep[7] = {
    true,
    false,
    true,
    false,
    false,
    false,
    true
};

ModResult init_puppet_zelda_pattern(const HookService*, ModError*) {
    s_lastZeldaId = fpcM_ERROR_PROCESS_ID_e;
    s_prevAction = ACTION_WAIT;
    s_zeldaPatternStep = 0;
    return MOD_OK;
}

void update_puppet_zelda_pattern(const LogService* svc_log, ModContext* mod_ctx) {
    if (!g_configPuppetZeldaPatternEnabled) {
        return;
    }

    e_hzelda_class* zelda = (e_hzelda_class*)fopAcM_SearchByName(fpcNm_E_HZELDA_e);
    if (zelda == nullptr) {
        s_lastZeldaId = fpcM_ERROR_PROCESS_ID_e;
        s_prevAction = ACTION_WAIT;
        s_zeldaPatternStep = 0;
        return;
    }

    fpc_ProcID zeldaId = fopAcM_GetID(zelda);
    if (zeldaId != s_lastZeldaId) {
        s_lastZeldaId = zeldaId;
        s_prevAction = ACTION_WAIT;
        s_zeldaPatternStep = 0;
        if (svc_log && mod_ctx) {
        }
    }

    if (s_prevAction == ACTION_WAIT && zelda->mAction != ACTION_WAIT && zelda->mAction != ACTION_DAMAGE) {
        int currentStep = s_zeldaPatternStep;
        if (s_isBallStep[currentStep]) {
            zelda->mAction = ACTION_ATTACK_C;
            if (svc_log && mod_ctx) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[PuppetZelda] Step %d/7: Forced Energy Ball", currentStep + 1);
            }
        } else {
            if (g_configPuppetZeldaAlwaysShortest) {
                zelda->mAction = ACTION_ATTACK_A;
            } else {
                zelda->mAction = (cM_rndF(1.0f) < 0.5f) ? ACTION_ATTACK_A : ACTION_ATTACK_B;
            }

            if (svc_log && mod_ctx) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[PuppetZelda] Step %d/7: Non-Ball Attack (%s)",
                         currentStep + 1,
                         (zelda->mAction == ACTION_ATTACK_A) ? "Sword Dive" : "Triangle");
            }
        }
        s_zeldaPatternStep = (s_zeldaPatternStep + 1) % 7;
    }

    s_prevAction = zelda->mAction;
}
