#pragma once

// [Reaper] Lift tuning file (yours to edit).
#include "lift_core.hpp"

// Lift tuning. Every field and its default is listed in struct Config in lift_core.hpp.
namespace lift::settings {
inline Config config() {
  Config c;

  // R2/L2 drive the lift at full power -- but only after max_height is set below.
  // Until then, jogging stays at the slow up_rate/down_rate so calibration is safe.
  c.full_power_manual_jog = true;

  // REQUIRED: measured safe upper travel in MOTOR degrees (read it off the controller's L: line).
  // c.max_height = ...;

  // Calibration mode: lets R2 raise the lift (slowly) with no top limit so you can measure travel.
  // Set this to false once max_height is set.
  c.allow_unbounded_jog = true;

  // Optional gravity compensation: c.gravity = GravityModel::Constant; c.kg = ...;
  // Optional gain schedule:        c.gains = {{low, kp, ki, kd}, {high, kp, ki, kd}};
  return c;
}

inline std::vector<Preset> presets() {
  // Add measured positions after configuring max_height. No guessed score heights.
  return {{"Start", 0}};
}
}  // namespace lift::settings
