#pragma once

// [Reaper] Roller colour tuning file (yours to edit).
#include "roller_core.hpp"

// Roller colour tuning. Ports, power and direction live in robot_hardware.hpp.
namespace roller::settings {

inline Thresholds thresholds() {
  Thresholds t;
  // Defaults are a starting point. Tune with the sensor mounted: the brain Home page and the terminal
  // show the live hue / saturation / proximity and the colour it decided on.
  // t.red_below = 20;  t.red_above = 340;
  // t.yellow_min = 40; t.yellow_max = 75;
  // t.blue_min = 180;  t.blue_max = 250;
  // t.min_saturation = 0.35;
  // t.min_proximity = 80;
  return t;
}

// How long a colour must be seen continuously before the roller believes it.
constexpr std::uint32_t CONFIRM_MS = 40;

// Optical sensor LED brightness, 0-100. A fixed light source makes readings repeatable.
constexpr int LED_PWM = 100;

// Default limit for roller::seek() when no timeout is given.
constexpr std::uint32_t DEFAULT_SEEK_TIMEOUT_MS = 2000;

}  // namespace roller::settings
