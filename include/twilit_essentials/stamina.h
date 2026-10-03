#pragma once

#include <mods/service.hpp>

#define TWILIT_ESSENTIALS_STAMINA_SERVICE_ID "com.dusklight.twilit_essentials.stamina"

// Game-thread only. Values use TE stamina units (normally 100 per ring).
// No caller-owned pointers or callbacks are retained.
struct TwilitEssentialsStaminaState {
    uint32_t struct_size;
    uint32_t enabled;
    uint32_t gameplay; // Player present, alive, unpaused, outside events/transitions.
    uint32_t exhausted; // Includes TE's recovery lockout, even above zero.
    float current;
    float maximum;
};

#define TWILIT_ESSENTIALS_STAMINA_STATE_INIT \
    {sizeof(TwilitEssentialsStaminaState), 0, 0, 0, 0.0f, 0.0f}

struct TwilitEssentialsStaminaService {
    ServiceHeader header;
    // Initialize out_state with TWILIT_ESSENTIALS_STAMINA_STATE_INIT.
    // MOD_UNAVAILABLE before initialization/after shutdown; output is cleared.
    ModResult (*get_state)(ModContext* caller, TwilitEssentialsStaminaState* out_state);
    // Immediate, all-or-nothing deduction; MOD_CONFLICT if insufficient/exhausted.
    ModResult (*try_consume)(ModContext* caller, float amount);
    // Immediate deduction clamped at zero; MOD_OK also when this exhausts stamina.
    // Caller supplies rate * elapsed seconds, once per update, then checks state.
    ModResult (*drain)(ModContext* caller, float amount);
};

// Mutations return MOD_UNAVAILABLE when disabled or outside gameplay,
// MOD_INVALID_ARGUMENT for null caller, negative/nonfinite amount or bad output.
// Zero is a no-op during active gameplay, including while exhausted.
// Positive deductions reset TE regeneration and notify its HUD. Normal TE action
// costs remain additive. Service presence alone does not mean stamina is enabled.
MOD_DECLARE_SERVICE(TwilitEssentialsStaminaService, svc_te_stamina,
    TWILIT_ESSENTIALS_STAMINA_SERVICE_ID, 1u, 0u);
