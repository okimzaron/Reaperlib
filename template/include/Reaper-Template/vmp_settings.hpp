#pragma once

// [Reaper] VMP tuning file (yours to edit).
#include <cstdint>

// VMP (vibration / motion monitor) tuning. IMU ports live in robot_hardware.hpp.
// Every number here can be read live on the brain's Sensors page (S), so tune from what you see there.
namespace vmp::settings {

// ---- "Robot is still" (vmp::wait_quiet, the QUIET state) ----
constexpr double QUIET_ACCEL_G = 0.025;     // shake below this ...
constexpr double QUIET_GYRO_DPS = 1.5;      // ... and rotation jitter below this ...
constexpr std::uint32_t QUIET_HOLD_MS = 500;  // ... for this long = quiet.

// ---- Hits (vmp::impact_since, the Hits counter) ----
// A hit is a jolt in the chassis IMUs' (IMU 1 / IMU 2) filtered acceleration (gravity removed). Watch "peak" on the Sensors
// page while driving normally and while bumping a wall, then set this between the two.
constexpr double IMPACT_G = 1.0;
constexpr std::uint32_t PEAK_WINDOW_MS = 2000;  // "peak" on the Sensors page = biggest jolt in this window
// Localization ignores distance-sensor corrections for this long after a hit (the robot may have been
// shoved, and sensors may be seeing another robot). 0 turns this off.
constexpr std::uint32_t LOCALIZATION_HOLDOFF_MS = 300;

// ---- Tipping (IMU 1 / IMU 2) ----
// Tilt is measured against the robot's orientation the first time it sits still after startup, so the
// IMU can be mounted any way round. Start the program with the robot flat on the field.
constexpr double TIP_WARN_DEG = 15.0;
constexpr double TIP_CLEAR_DEG = 11.0;  // warning ends below this (stops it flickering on/off)

// ---- Lift sway (lift IMU, LIFT_IMU_PORT) ----
constexpr double SWAY_GYRO_DPS = 45.0;   // rotation jitter of the lift IMU that counts as swaying
constexpr double SWAY_CLEAR_DPS = 30.0;  // sway warning ends below this

// ---- What happens on a warning (driver control) ----
constexpr bool WARN_RUMBLE = true;         // controller rumbles once when a tip or sway warning starts
constexpr int TIP_DRIVE_SPEED_LIMIT = 70;  // drive speed cap (0-127) while tipping; 127 = no cap
constexpr int SWAY_DRIVE_SPEED_LIMIT = 100;  // drive speed cap while the lift sways; 127 = no cap

}  // namespace vmp::settings
