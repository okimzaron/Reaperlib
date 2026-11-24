#pragma once
#include "pros/misc.h"

namespace ez {
class Drive;
}

// Short button names for the settings below. btn::NONE leaves an action unbound.
namespace btn {
using Button = pros::controller_digital_e_t;
inline constexpr Button NONE = static_cast<Button>(0);
inline constexpr Button L1 = pros::E_CONTROLLER_DIGITAL_L1;
inline constexpr Button L2 = pros::E_CONTROLLER_DIGITAL_L2;
inline constexpr Button R1 = pros::E_CONTROLLER_DIGITAL_R1;
inline constexpr Button R2 = pros::E_CONTROLLER_DIGITAL_R2;
inline constexpr Button UP = pros::E_CONTROLLER_DIGITAL_UP;
inline constexpr Button DOWN = pros::E_CONTROLLER_DIGITAL_DOWN;
inline constexpr Button LEFT = pros::E_CONTROLLER_DIGITAL_LEFT;
inline constexpr Button RIGHT = pros::E_CONTROLLER_DIGITAL_RIGHT;
inline constexpr Button X = pros::E_CONTROLLER_DIGITAL_X;
inline constexpr Button B = pros::E_CONTROLLER_DIGITAL_B;
inline constexpr Button Y = pros::E_CONTROLLER_DIGITAL_Y;
inline constexpr Button A = pros::E_CONTROLLER_DIGITAL_A;
const char* name(Button button);
}  // namespace btn
enum class DriveStyle {
  SplitArcade,          // left stick forward/back, right stick turn
  SingleArcade,         // left stick does both
  FlippedSplitArcade,   // right stick forward/back, left stick turn
  FlippedSingleArcade,  // right stick does both
  Tank,                 // left stick left side, right stick right side
};

// MASTER CONTROLLER CONFIGURATION
// Every driver button lives here. Edit the SETTINGS section, then rebuild/upload. No button edits elsewhere.
// The build fails if two driver actions share a button (see the checks at the bottom of this file).
class RobotControls {
 public:
  using Button = btn::Button;

  // ========================== SETTINGS ==========================
  // Drive
  static constexpr DriveStyle DRIVE_STYLE = DriveStyle::SplitArcade;
  // Stick values below this (0-127) count as centered, so a resting stick never creeps the robot.
  static constexpr int DRIVE_DEADBAND = 3;
  // Smallest power a stick sends once past the deadband (0-126). A tiny push then goes straight to a power
  // that actually moves the robot. Start around 10-20 and raise it until a light push just moves the robot.
  // 0 = off (original behaviour). Full stick is always 127.
  static constexpr int DRIVE_MIN_OUTPUT = 0;

  // Lift (hold). Releasing holds the current height.
  static constexpr Button LIFT_UP = btn::R2;
  static constexpr Button LIFT_DOWN = btn::L2;
  static constexpr Button LIFT_RETURN = btn::LEFT;  // press: return to the bottom (also retries a failed lift start)

  // Claw (hold). Powers are 0-127.
  static constexpr Button CLAW_FORWARD = btn::R1;
  static constexpr int CLAW_FORWARD_POWER = 127;
  static constexpr Button CLAW_REVERSE = btn::L1;
  static constexpr int CLAW_REVERSE_POWER = 127;

  // Roller (hold): turn to the alliance colour picked on the brain screen and hold it. Release stops.
  static constexpr Button ROLLER_TO_COLOR = btn::A;

  // Manual auton runner: hold both to run / cancel the selected route (no field controller only).
  static constexpr Button TEST_AUTON_1 = btn::DOWN;
  static constexpr Button TEST_AUTON_2 = btn::B;

  // EZ-Template joystick curve adjust buttons. Changes are saved to the SD card.
  // These may share buttons with the driver actions above: startup prints any overlap to the terminal.
  // The defaults overlap LIFT_RETURN (LEFT) and ROLLER_TO_COLOR (A); set false to turn curve buttons off.
  static constexpr bool CURVE_BUTTONS_ENABLED = true;
  static constexpr Button CURVE_LEFT_DECREASE = btn::LEFT;
  static constexpr Button CURVE_LEFT_INCREASE = btn::RIGHT;
  static constexpr Button CURVE_RIGHT_DECREASE = btn::Y;
  static constexpr Button CURVE_RIGHT_INCREASE = btn::A;

  // EZ-Template PID tuner buttons. Only active while the PID tuner is switched on.
  static constexpr Button PID_TUNER_INCREASE = btn::A;
  static constexpr Button PID_TUNER_DECREASE = btn::Y;
  static constexpr Button PID_TUNER_PAGE_UP = btn::UP;
  static constexpr Button PID_TUNER_PAGE_DOWN = btn::DOWN;
  static constexpr Button PID_TUNER_PAGE_LEFT = btn::LEFT;
  static constexpr Button PID_TUNER_PAGE_RIGHT = btn::RIGHT;
  // ======================== END SETTINGS ========================

  /// True while the button is held. Always false for btn::NONE.
  static bool held(Button button);
  /// True once per press. Always false for btn::NONE.
  static bool pressed(Button button);
  /// Runs the selected DRIVE_STYLE for one loop.
  static void drive(ez::Drive& chassis);
  /// Hands the curve and PID tuner buttons to EZ-Template and reports overlaps. Call once in initialize().
  static void apply(ez::Drive& chassis);

  // ---- Compile-time checks ----
  static constexpr Button DRIVER_ACTIONS[] = {LIFT_UP,      LIFT_DOWN,       LIFT_RETURN,  CLAW_FORWARD,
                                              CLAW_REVERSE, ROLLER_TO_COLOR, TEST_AUTON_1, TEST_AUTON_2};
  static constexpr bool driver_buttons_unique() {
    constexpr int count = sizeof(DRIVER_ACTIONS) / sizeof(DRIVER_ACTIONS[0]);
    for (int i = 0; i < count; ++i)
      for (int j = i + 1; j < count; ++j)
        if (DRIVER_ACTIONS[i] != btn::NONE && DRIVER_ACTIONS[i] == DRIVER_ACTIONS[j])
          return false;
    return true;
  }
};

static_assert(RobotControls::driver_buttons_unique(),
              "robot_controls.hpp: two driver actions are mapped to the same button.");
static_assert(RobotControls::CLAW_FORWARD_POWER >= 0 && RobotControls::CLAW_FORWARD_POWER <= 127 &&
                  RobotControls::CLAW_REVERSE_POWER >= 0 && RobotControls::CLAW_REVERSE_POWER <= 127,
              "robot_controls.hpp: powers must be 0..127.");
static_assert(RobotControls::DRIVE_DEADBAND >= 0 && RobotControls::DRIVE_DEADBAND <= 127 &&
                  RobotControls::DRIVE_MIN_OUTPUT >= 0 && RobotControls::DRIVE_MIN_OUTPUT <= 126,
              "robot_controls.hpp: DRIVE_DEADBAND must be 0..127 and DRIVE_MIN_OUTPUT 0..126.");
static_assert(RobotControls::TEST_AUTON_1 != btn::NONE && RobotControls::TEST_AUTON_2 != btn::NONE,
              "robot_controls.hpp: the manual auton runner needs two buttons.");
