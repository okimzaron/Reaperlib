#pragma once
#include <array>
#include <cstdint>
#include "Reaper-Template/vmp_core.hpp"

// [Reaper] VMP: watches up to three IMUs at 100 Hz and turns them into "is the robot still?", "did it get hit?",
// "is it tipping?" and "is the lift swaying?". It only observes: it never calibrates or resets an IMU and
// never moves a motor. Tuning lives in vmp_settings.hpp; live readings are on the brain's Sensors page.
//
// Slots: 0 = IMU 1, 1 = IMU 2, 2 = lift IMU (all from robot_hardware.hpp). Port 0 = unused.
// Every function is safe to call from any task.
namespace vmp {

/// Call once from initialize(), after the IMUs have calibrated.
bool start(const std::array<int, 3>& ports, bool console_csv = false);

/// Latest per-slot readings.
std::array<Snapshot, 3> snapshot();

/// True when every configured chassis IMU (IMU 1 / IMU 2) reports QUIET (robot still). False if neither IMU 1 nor IMU 2 is configured.
bool is_quiet();

/// Blocks until the robot is still (is_quiet) or the timeout passes. True = confirmed still.
/// Returns false at once when neither IMU 1 nor IMU 2 is configured, so routes never sit waiting for nothing.
/// Use before reading the start position or a localization::distance_reset().
bool wait_quiet(std::uint32_t timeout_ms = 1500);

/// Reaper one-call "stop and be still": chassis.pid_wait() (the motion finishes), then wait_quiet()
/// (the robot physically stops rocking). Use it in place of the pair before a wall reset or anything
/// that needs a dead-still robot. Without IMU 1 / IMU 2 it waits a fixed 80 ms instead.
/// True = confirmed still; false = timed out (or no IMU), carry on anyway or skip the reset.
bool settle(std::uint32_t timeout_ms = 600);

/// True if IMU 1 or IMU 2 registered a hit at or after `since_ms` (a pros::millis() time).
bool impact_since(std::uint32_t since_ms);

struct Impact {
  bool any = false;      // false until the first hit
  std::uint32_t ms = 0;  // pros::millis() of the hit
  double g = 0;          // size of the jolt
  int slot = -1;
};
/// Most recent hit on IMU 1 or IMU 2.
Impact last_impact();

enum class Alert { none, sway, tipping };
struct Warning {
  Alert alert = Alert::none;  // tipping wins over sway
  double tilt_deg = NAN;      // largest drive-IMU tilt (NaN until a reference exists)
  double lift_sway_dps = NAN; // lift IMU rotation jitter (NaN without a lift IMU)
};
/// Current tip / sway warning.
Warning warning();
const char* alert_name(Alert alert);

/// Re-zero tilt: the robot's current orientation (once it is still) becomes "flat".
void tilt_reference_reset();

}  // namespace vmp
