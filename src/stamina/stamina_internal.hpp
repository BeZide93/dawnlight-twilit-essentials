#pragma once

namespace stamina_impl {

constexpr float kExhaustedSpeedMul = 0.75f;

constexpr float kScaleMinHearts = 3.0f;
constexpr float kScaleMaxHearts = 20.0f;
constexpr float kScaleBaseValue = 100.0f;

constexpr int kRingMax = 3;
constexpr float kRingCapacity = 100.0f;

float main_ring_capacity(float maxValue);

constexpr float kSprintAnimSpeedFraction = 0.5f;

inline float sprint_anim_speed_mul(float speed_setting) {
    return 1.0f + (speed_setting - 1.0f) * kSprintAnimSpeedFraction;
}

float max_value();
float current_value();
bool is_empty();
bool in_gameplay();

void report_drain(float amount);

float cost_scaled(float base_cost, int pct);

bool sprint_wind_allowed();
void sprint_wind_report(unsigned int emitterId);

}
