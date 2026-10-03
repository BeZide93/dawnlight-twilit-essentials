#!/usr/bin/env python3
"""Compile actual stamina service/update code against small game/HUD fakes."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--sdk", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "src/stamina/stamina.cpp").read_text()


def section(start, end):
    return source[source.index(start):source.index(end)]


fixture = r'''
#include "twilit_essentials/stamina.h"
#include <cassert>
#include <cmath>
#include <limits>
using f32 = float;
struct ModContext {};
struct LogService {};
struct daAlink_c {
    enum { PROC_CUT_LARGE_JUMP_CHARGE = 1 };
    int mProcID = 0;
    bool dead = false, transition = false;
    bool checkDeadHP() { return dead; }
    bool checkSceneChangeAreaStart() const { return transition; }
    bool checkWolf() { return false; }
    bool checkPlayerGuard() { return false; }
} player;
daAlink_c* actor = &player;
auto daAlink_getAlinkActorClass() { return actor; }
auto daPy_getLinkPlayerActorClass() { return actor; }
bool playing = true, changingStage = false;
bool in_gameplay() { return playing; }
bool dComIfGp_isEnableNextStage() { return changingStage; }
bool g_configStaminaEnabled = true, g_configStaminaRefillOnStageChange = true;
bool g_configStaminaSlowHangRegen = true;
int g_configStaminaRegenDelay = 2, g_configStaminaRegen = 100;
int g_configStaminaExhaustRecover = 35;
float maximum = 100;
float stamina_max() { return maximum; }
int spentNotifications = 0, exhaustNotifications = 0;
int s_denyCooldown = 0;
void stamina_hud_notify_spend() { ++spentNotifications; }
void stamina_hud_notify_exhaust() { ++exhaustNotifications; }
void stamina_hud_notify_recover() {}
void stamina_hud_notify_drain() {}
void stamina_hud_refill(float) {}
void stamina_hud_reset(float) {}
void stamina_hud_update(float, float, bool) {}
void stamina_hud_begin_tick() {}
void stamina_hud_idle_tick() {}
bool stamina_hud_radial_style() { return false; }
void update_sprint_human() {}
void update_sprint_swim() {}
bool is_hidden_skill_proc_state(daAlink_c*) { return false; }
bool is_hang_rest_proc(daAlink_c*) { return false; }
float drain_rate(int) { return 0; }
float spin_charge_drain(daAlink_c*) { return 0; }
namespace stamina_impl { float main_ring_capacity(float v) { return v; } }
'''
fixture += section("static f32 s_stamina", "static f32 stamina_scaled_max_for_hearts")
fixture += section("static void refill_stamina()", "namespace stamina_impl {")
fixture += section("static f32 s_otherSpend", "static void spend(f32 cost)")
fixture += section("void update_stamina(", "template <class Entry>")
fixture += r'''
int main() {
    ModContext caller, other;
    auto state = TwilitEssentialsStaminaState TWILIT_ESSENTIALS_STAMINA_STATE_INIT;
    auto get = g_staminaService.get_state;
    auto consume = g_staminaService.try_consume;
    auto drain = g_staminaService.drain;
    assert(get(&caller, &state) == MOD_UNAVAILABLE && state.current == 0);
    assert(consume(&caller, 10) == MOD_UNAVAILABLE);
    s_serviceReady = true;
    assert(get(nullptr, &state) == MOD_INVALID_ARGUMENT);
    assert(get(&caller, nullptr) == MOD_INVALID_ARGUMENT);
    state.struct_size = 0;
    assert(get(&caller, &state) == MOD_INVALID_ARGUMENT);
    state = TWILIT_ESSENTIALS_STAMINA_STATE_INIT;
    for (float bad : {-1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
        assert(consume(&caller, bad) == MOD_INVALID_ARGUMENT);
        assert(drain(&caller, bad) == MOD_INVALID_ARGUMENT);
    }
    assert(consume(nullptr, 1) == MOD_INVALID_ARGUMENT && s_stamina == 100);
    g_configStaminaEnabled = false;
    assert(get(&caller, &state) == MOD_OK && !state.enabled);
    assert(drain(&caller, 1) == MOD_UNAVAILABLE);
    g_configStaminaEnabled = true;
    playing = false;
    assert(consume(&caller, 1) == MOD_UNAVAILABLE);
    assert(get(&caller, &state) == MOD_OK && !state.gameplay);
    playing = true;
    actor = nullptr;
    assert(drain(&caller, 1) == MOD_UNAVAILABLE);
    actor = &player;
    player.dead = true;
    assert(consume(&caller, 1) == MOD_UNAVAILABLE);
    player.dead = false;
    player.transition = true;
    assert(consume(&caller, 1) == MOD_UNAVAILABLE);
    player.transition = false;
    changingStage = true;
    assert(consume(&caller, 1) == MOD_UNAVAILABLE);
    changingStage = false;
    assert(s_stamina == 100 && spentNotifications == 0);
    assert(consume(&caller, 60) == MOD_OK && s_stamina == 40);
    assert(s_regenDelay == 60 && spentNotifications == 1 && s_otherSpend == 0);
    assert(consume(&other, 50) == MOD_CONFLICT && s_stamina == 40);
    assert(consume(&other, 40) == MOD_OK && s_stamina == 0 && s_exhausted);
    assert(exhaustNotifications == 1);
    assert(drain(&caller, 1) == MOD_CONFLICT);
    assert(consume(&caller, 0) == MOD_OK);
    s_stamina = 10; // Positive stamina does not bypass recovery lockout.
    assert(consume(&caller, 1) == MOD_CONFLICT);
    refill_stamina();
    assert(drain(&caller, 1000) == MOD_OK && s_stamina == 0 && s_exhausted);
    refill_stamina();
    maximum = 25; // Settings changed before the next TE tick.
    assert(consume(&caller, 30) == MOD_CONFLICT);
    assert(get(&caller, &state) == MOD_OK && state.current == 25);
    assert(consume(&caller, 5) == MOD_OK && s_stamina == 20);
    maximum = 100;
    refill_stamina();
    g_configStaminaRegenDelay = 0;
    assert(drain(&caller, 10) == MOD_OK);
    update_stamina(nullptr, nullptr);
    assert(s_stamina == 90); // No regeneration in a spending tick.
    update_stamina(nullptr, nullptr);
    assert(s_stamina > 90); // Regeneration resumes after spending stops.
    assert(drain(&caller, 1) == MOD_OK);
    float before = s_stamina;
    s_extraDrain = 2; // Native action costs remain additive.
    update_stamina(nullptr, nullptr);
    assert(s_stamina == before - 2);
    s_serviceReady = false;
    assert(drain(&caller, 1) == MOD_UNAVAILABLE);
    assert(get(&caller, &state) == MOD_UNAVAILABLE && state.maximum == 0);
}
'''
with tempfile.TemporaryDirectory(prefix="te-stamina-test-") as tmp:
    cpp = Path(tmp) / "test.cpp"
    binary = Path(tmp) / "test"
    cpp.write_text(fixture)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra",
                    "-I", str(root / "include"), "-I", str(args.sdk / "sdk/include"),
                    str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("Stamina service contract tests passed")
