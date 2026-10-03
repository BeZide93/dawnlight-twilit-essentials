# Stamina Service Example

This is a minimal standalone mod that shows how to use the Twilit Essentials
stamina service (see [docs/stamina-service.md](../../docs/stamina-service.md)).
While Link pulls back the bowstring or holds an arrow drawn, the mod drains
stamina through `drain`. If TE refuses the payment because Link is exhausted,
the mod calls `deny` once, so the player gets TE's usual sound and red flash.

The whole mod is in `src/mod.cpp`:

1. Include `twilit_essentials/stamina.h` and import the service as optional.
2. In `mod_update`, check that the service pointer isn't null.
3. Call `drain(mod_ctx, cost_per_second * tick_seconds)` once per update while
   the ability is active.
4. If the result is `MOD_CONFLICT`, call `deny` once.

The mod doesn't hook anything, and it does nothing when TE isn't installed.

Twilit Essentials already has the same behaviour built in as the
"Bow drawing" stamina activity. Turn that toggle off while you try this
example, otherwise drawing the bow costs stamina twice.

In your own mod, copy `include/twilit_essentials/stamina.h` into the project
and add its folder to the include path.
