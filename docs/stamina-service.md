# Stamina service

Other mods can use Twilit Essentials' stamina pool through the optional service
`com.dusklight.twilit_essentials.stamina`. Include
`include/twilit_essentials/stamina.h` together with the Dusklight SDK:

```cpp
#include "twilit_essentials/stamina.h"
IMPORT_OPTIONAL_SERVICE(TwilitEssentialsStaminaService, svc_te_stamina);
```

Only call the service on the game thread, and pass your own `mod_ctx`. Check
the optional service pointer before every use. You don't need to register
anything or provide callbacks. If no mod calls the service, TE's stamina works
exactly as it does without it.

## Functions

| Function | Purpose |
| --- | --- |
| `get_state(ctx, &state)` | Returns the current pool and gate flags. |
| `try_consume(ctx, amount)` | All-or-nothing payment for one action. |
| `drain(ctx, amount)` | Continuous payment, clamped at zero. |
| `restore(ctx, amount)` | Adds stamina, clamped to the maximum. |
| `deny(ctx)` | Plays TE's "not enough stamina" feedback. |
| `get_source(ctx, source, &info)` | Returns TE's toggle and cost setting for an activity. |

### get_state

Initialize a `TwilitEssentialsStaminaState` with
`TWILIT_ESSENTIALS_STAMINA_STATE_INIT`, then call `get_state`. A successful
query reports:

- `enabled`: whether stamina is turned on in TE.
- `gameplay`: whether spending is currently possible. This needs a living
  player, no pause, no event, and no stage transition.
- `exhausted`: TE's exhaustion lockout. It can stay set even when `current`
  is above zero.
- `current` and `maximum`: values in TE stamina units (normally 100 per ring).
- `recover_at`: the `current` value at which exhaustion ends.
- `regen_delay`: seconds left before regeneration starts again.

The service being present does not mean stamina is enabled. Before TE has
initialized, or after it has shut down, the query returns `MOD_UNAVAILABLE` and
a cleared state. TE fills only the fields that fit in the `struct_size` you
pass.

### try_consume and drain

- `try_consume(ctx, amount)` takes the whole amount immediately, or nothing.
  Use it for one-off actions, such as starting Flurry Rush. Only start the
  action if it returns `MOD_OK`.
- `drain(ctx, amount)` takes stamina immediately and stops at zero. For a
  continuous ability, pass `cost_per_second * elapsed_seconds` once per update.
  Use a bounded real gameplay delta that excludes pauses, not slowed simulation
  time. Call `get_state` afterwards and stop the ability when stamina is empty
  or exhausted. `MOD_OK` can mean that the last stamina was just used up.

Both functions take absolute TE units and don't apply TE's per-activity cost
multipliers (see `get_source`). A payment resets regeneration, updates the
HUD, and triggers exhaustion the same way TE's own actions do. Every caller
sees a deduction immediately. Even with a regeneration delay of zero, stamina
doesn't regenerate on the TE update after an external payment. TE's
hidden-skill attack refunds ignore external payments.

### restore

`restore(ctx, amount)` adds stamina, up to the current maximum, for example
for a potion or a refund. It does not end exhaustion directly. TE's normal
rule applies, so exhaustion ends once `current` reaches `recover_at`.

### deny

`deny(ctx)` plays TE's own denial feedback (error sound and red flash on the
meter). Call it when `try_consume` returns `MOD_CONFLICT` so that a blocked
ability feels the same as a blocked roll or attack. TE limits how often the
sound plays.

### get_source

Use `get_source` to apply the player's TE settings for an activity to your own
ability. Initialize a `TwilitEssentialsStaminaSourceInfo` with
`TWILIT_ESSENTIALS_STAMINA_SOURCE_INFO_INIT` and pass a
`TWILIT_ESSENTIALS_STAMINA_SOURCE_*` value:

- `enabled`: TE stamina is on and this activity costs stamina.
- `cost_multiplier`: the player's cost slider for this activity (1.0 = 100%,
  never below 0.05).

Example for a Bullet Time ability that uses TE's "Bullet Time" setting:

```cpp
TwilitEssentialsStaminaSourceInfo info = TWILIT_ESSENTIALS_STAMINA_SOURCE_INFO_INIT;
if (svc_te_stamina &&
    svc_te_stamina->get_source(mod_ctx, TWILIT_ESSENTIALS_STAMINA_SOURCE_BULLET_TIME, &info) == MOD_OK &&
    info.enabled) {
    svc_te_stamina->drain(mod_ctx, kCostPerSecond * info.cost_multiplier * dt);
}
```

Sources: `ATTACK`, `JUMP_ATTACK`, `SPIN_ATTACK`, `SPIN_CHARGE`, `HIDDEN_SKILL`,
`ROLL`, `SIDESTEP`, `BLOCK`, `CLIMB`, `CRAWL`, `HANG`, `SWIM`, `PUSH_PULL`,
`SPRINT`, `SWIM_SPRINT`, `WOLF_DASH`, `WOLF_SPRINT`, `BULLET_TIME`, `BOW_DRAW`, `SLINGSHOT`, `BALL_AND_CHAIN`. Some
activities share one toggle in TE's settings: jump attack, spin and spin
charge share one; roll and sidestep share one; climb and crawl share one;
sprint and swim sprint share one; wolf dash and wolf sprint share one. An
unknown source returns `MOD_INVALID_ARGUMENT`. `get_source` only reports
settings. It also works outside gameplay.

## Results

| Result | Meaning |
| --- | --- |
| `MOD_OK` | The call succeeded, or a valid amount of zero did nothing. |
| `MOD_UNAVAILABLE` | TE isn't initialized, stamina is off, there is no living player, the game is paused or in an event, or a stage transition is running. |
| `MOD_CONFLICT` | `try_consume`/`drain`: the player is exhausted, or (`try_consume` only) doesn't have enough stamina. Nothing was charged. |
| `MOD_INVALID_ARGUMENT` | Null caller or output, struct too small, negative or non-finite amount, or unknown source. Nothing was changed. |

An amount of zero succeeds during gameplay even while the player is exhausted.
If exhaustion should also block a free ability, check the state yourself.

Don't keep the service pointer across mod unloads; Dusklight manages the
import. If TE is missing or its stamina is off, fall back to your own system.
Don't use a fallback to get around TE's exhaustion or pause gates every time a
payment fails.

## Example

[examples/stamina-service-example](../examples/stamina-service-example) is a
complete standalone mod that uses the service. It drains stamina while Link
draws the bow and calls `deny` when he is exhausted.
