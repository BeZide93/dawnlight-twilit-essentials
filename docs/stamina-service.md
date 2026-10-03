# Stamina service (v1.0)

External mods can optionally import `com.dusklight.twilit_essentials.stamina`
using `include/twilit_essentials/stamina.h` and the Dusklight SDK:

```cpp
#include "twilit_essentials/stamina.h"
IMPORT_OPTIONAL_SERVICE(TwilitEssentialsStaminaService, svc_te_stamina);
```

Call the service only on the game thread with your own `mod_ctx`. Check the
optional service pointer before every use. No callbacks or registrations are
needed. Existing TE behavior is unchanged when nobody calls the service.

Initialize a `TwilitEssentialsStaminaState` with
`TWILIT_ESSENTIALS_STAMINA_STATE_INIT`, then call `get_state`. A successful query
reports whether stamina is enabled, whether gameplay permits spending, the
current/maximum values, and TE's exhaustion recovery lockout. Import presence
alone does not mean stamina is active. Before initialization/after shutdown,
the query returns `MOD_UNAVAILABLE` with a cleared state.

- `try_consume(ctx, amount)`: all-or-nothing, immediate payment for an action
  such as starting Flurry Rush. Start the action only on `MOD_OK`.
- `drain(ctx, amount)`: immediate continuous payment, clamped at zero. For Bullet
  Time, pass `cost_per_second * elapsed_seconds` once per client update. Use a
  bounded real gameplay delta, excluding pauses, rather than slowed simulation
  time. Query afterward and stop when empty or exhausted; `MOD_OK` can mean that
  the last remaining stamina was consumed.

Both functions accept absolute TE units, without applying TE's per-action cost
multipliers. They reset regeneration, update HUD feedback, and apply normal TE
exhaustion. Multiple clients see deductions immediately. With zero regeneration
delay, external spending still prevents regeneration on the next TE update.
External spending is not included in TE's hidden-skill attack refunds.

| Result | Meaning |
| --- | --- |
| `MOD_OK` | Payment applied, or a valid zero-cost no-op. |
| `MOD_UNAVAILABLE` | Not initialized, stamina disabled, no living player, paused/event gameplay, or a stage transition. |
| `MOD_CONFLICT` | Exhaustion lockout, or insufficient stamina for `try_consume`; no payment applied. |
| `MOD_INVALID_ARGUMENT` | Null caller/output, undersized state, or a negative/nonfinite amount; no payment applied. |

A zero amount succeeds during active gameplay even while exhausted; callers
that want exhaustion to block a free ability must inspect the state explicitly.
Keep the optional pointer/import managed by Dusklight; do not retain it across
mod unloads. Fall back to the client's own system when TE is missing or its
stamina is disabled, but do not bypass exhaustion or pause gates through a
fallback on every unsuccessful payment.

This is a provider-only patch. It does not connect Dawnlight, add ability costs
to TE settings, or suppress TE's normal attack costs during Flurry Rush. Normal
TE costs remain additive; a fixed-price sequence with attack-cost suppression
would require a separate scoped interface.

An in-game compatibility test with an importing mod is still required.
