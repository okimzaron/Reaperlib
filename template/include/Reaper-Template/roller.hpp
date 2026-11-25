#pragma once

// [Reaper] Colour roller: one motor plus one optical sensor. Rolls until the selected alliance colour
// (red or blue) faces the sensor, and never stops on yellow. Its own task is the only code that moves the motor.
// Ports: robot_hardware.hpp. Colour thresholds: roller_settings.hpp. Guide: https://reaperlib.online/docs/#roller
#include <cstdint>
#include "Reaper-Template/roller_core.hpp"

// Colour roller: its own motor, its own optical sensor, its own background task.
//
// The roller is independent of every other mechanism (it is not tied to the intake). Every function below
// is safe to call from any task -- autonomous routes, opcontrol, your own tasks. Calls only post a
// request; the roller task is the only code that ever moves the roller motor, so callers never fight.
//
// Target colour = the alliance colour picked with the RED/BLUE button on the brain's Routes page.
// The roller turns until the TARGET colour is in front of the sensor and never stops on yellow (or on the
// other alliance's colour).
namespace roller {

// Supplied by RobotHardware::configure_devices(). Port 0 disables that device.
struct Config {
  int motor_port;    // Signed: a negative port reverses the motor.
  int optical_port;  // 1..21, or 0 for none (seeking is then unavailable; spin() still works).
  int power;         // 1..127, speed used while seeking.
};

enum class Mode { Disabled, Idle, Manual, Seek, Maintain };
enum class Result { None, Running, Reached, TimedOut, Cancelled, Fault };

struct Status {
  Mode mode = Mode::Disabled;
  Result result = Result::None;   // Outcome of the most recent seek().
  Color seen = Color::None;       // Debounced colour the roller is acting on.
  Color raw = Color::None;        // This cycle's undebounced classification.
  double hue = NAN, saturation = NAN;
  int proximity = -1;
  bool sensor_ok = false;         // Optical sensor plugged in and returning valid data.
  bool on_target = false;         // seen == target_get().
  int power = 0;                  // Last power sent to the motor.
};

/// Starts the roller task. Called once by RobotHardware::configure_devices().
bool initialize(Config config);

/// Alliance colour to roll to. Only Red or Blue are accepted.
void target_set(Color color);
Color target_get();

/// Spin at a fixed power (-127..127) until told otherwise. 0 is the same as stop().
void spin(int power);

/// Stop the motor (brake) and cancel any seek/maintain.
void stop();

/// Non-blocking: turn until the target colour is confirmed, then stop. Returns false if the roller or its
/// optical sensor is not configured. Check progress with status().result or block with wait().
bool seek(std::uint32_t timeout_ms = 0);  // 0 = settings::DEFAULT_SEEK_TIMEOUT_MS

/// Blocks until the current seek() finishes. True only if the target colour was reached.
bool wait(std::uint32_t timeout_ms = 3000);

/// seek() then wait(). Convenience for autonomous routes.
bool roll_to_target(std::uint32_t timeout_ms = 0);

/// Keep defending the target colour: turn whenever it is not in front of the sensor, stop when it is.
/// Runs until stop()/spin()/seek() replaces it.
bool maintain();

/// maintain() for a fixed time, then stop. Blocking; for autonomous routes.
void maintain_for(std::uint32_t duration_ms);

/// Driver helper, call every opcontrol loop: holding the button maintains the target colour, releasing it
/// stops the roller. Only acts on press/release edges, so it never overrides spin()/seek() calls made
/// elsewhere while the button is up.
void driver_update(bool held);

/// Latest snapshot, safe from any task including the brain-screen UI.
Status status();

const char* mode_name(Mode mode);
const char* result_name(Result result);

}  // namespace roller
