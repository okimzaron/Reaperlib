#include "main.h"

#include <string>

const char* btn::name(Button button) {
  switch (button) {
    case pros::E_CONTROLLER_DIGITAL_L1:
      return "L1";
    case pros::E_CONTROLLER_DIGITAL_L2:
      return "L2";
    case pros::E_CONTROLLER_DIGITAL_R1:
      return "R1";
    case pros::E_CONTROLLER_DIGITAL_R2:
      return "R2";
    case pros::E_CONTROLLER_DIGITAL_UP:
      return "Up";
    case pros::E_CONTROLLER_DIGITAL_DOWN:
      return "Down";
    case pros::E_CONTROLLER_DIGITAL_LEFT:
      return "Left";
    case pros::E_CONTROLLER_DIGITAL_RIGHT:
      return "Right";
    case pros::E_CONTROLLER_DIGITAL_X:
      return "X";
    case pros::E_CONTROLLER_DIGITAL_B:
      return "B";
    case pros::E_CONTROLLER_DIGITAL_Y:
      return "Y";
    case pros::E_CONTROLLER_DIGITAL_A:
      return "A";
    default:
      return "none";
  }
}

bool RobotControls::held(Button button) { return button != btn::NONE && master.get_digital(button) == 1; }

bool RobotControls::pressed(Button button) {
  return button != btn::NONE && master.get_digital_new_press(button) == 1;
}

void RobotControls::drive(ez::Drive& chassis) {
  switch (DRIVE_STYLE) {
    case DriveStyle::SingleArcade:
      chassis.opcontrol_arcade_standard(ez::SINGLE);
      break;
    case DriveStyle::FlippedSplitArcade:
      chassis.opcontrol_arcade_flipped(ez::SPLIT);
      break;
    case DriveStyle::FlippedSingleArcade:
      chassis.opcontrol_arcade_flipped(ez::SINGLE);
      break;
    case DriveStyle::Tank:
      chassis.opcontrol_tank();
      break;
    case DriveStyle::SplitArcade:
    default:
      chassis.opcontrol_arcade_standard(ez::SPLIT);
      break;
  }
}

namespace {
// Names of the driver actions in DRIVER_ACTIONS order, for the overlap report.
constexpr const char* DRIVER_ACTION_NAMES[] = {"LIFT_UP",      "LIFT_DOWN",       "LIFT_RETURN",  "CLAW_FORWARD",
                                               "CLAW_REVERSE", "ROLLER_TO_COLOR", "TEST_AUTON_1", "TEST_AUTON_2"};
static_assert(sizeof(DRIVER_ACTION_NAMES) / sizeof(DRIVER_ACTION_NAMES[0]) ==
                  sizeof(RobotControls::DRIVER_ACTIONS) / sizeof(RobotControls::DRIVER_ACTIONS[0]),
              "Keep DRIVER_ACTION_NAMES in step with RobotControls::DRIVER_ACTIONS.");

void report_curve_overlap(const char* curve_name, btn::Button button) {
  if (button == btn::NONE) return;
  const int count = sizeof(RobotControls::DRIVER_ACTIONS) / sizeof(RobotControls::DRIVER_ACTIONS[0]);
  for (int i = 0; i < count; ++i)
    if (RobotControls::DRIVER_ACTIONS[i] == button)
      printf("[Controls] %s shares button %s with %s.\n", curve_name, btn::name(button), DRIVER_ACTION_NAMES[i]);
}

void set_if_bound(void (ez::Drive::*setter)(pros::controller_digital_e_t), ez::Drive& chassis,
                  btn::Button button) {
  if (button != btn::NONE) (chassis.*setter)(button);
}
}  // namespace

void RobotControls::apply(ez::Drive& chassis) {
  chassis.opcontrol_joystick_threshold_set(DRIVE_DEADBAND);
  chassis.opcontrol_joystick_min_output_set(DRIVE_MIN_OUTPUT);
  chassis.opcontrol_curve_buttons_toggle(CURVE_BUTTONS_ENABLED);
  if (CURVE_BUTTONS_ENABLED) {
    chassis.opcontrol_curve_buttons_left_set(CURVE_LEFT_DECREASE, CURVE_LEFT_INCREASE);
    chassis.opcontrol_curve_buttons_right_set(CURVE_RIGHT_DECREASE, CURVE_RIGHT_INCREASE);
    report_curve_overlap("CURVE_LEFT_DECREASE", CURVE_LEFT_DECREASE);
    report_curve_overlap("CURVE_LEFT_INCREASE", CURVE_LEFT_INCREASE);
    report_curve_overlap("CURVE_RIGHT_DECREASE", CURVE_RIGHT_DECREASE);
    report_curve_overlap("CURVE_RIGHT_INCREASE", CURVE_RIGHT_INCREASE);
  }

  set_if_bound(&ez::Drive::pid_tuner_button_increment_set, chassis, PID_TUNER_INCREASE);
  set_if_bound(&ez::Drive::pid_tuner_button_decrement_set, chassis, PID_TUNER_DECREASE);
  set_if_bound(&ez::Drive::pid_tuner_button_up_set, chassis, PID_TUNER_PAGE_UP);
  set_if_bound(&ez::Drive::pid_tuner_button_down_set, chassis, PID_TUNER_PAGE_DOWN);
  set_if_bound(&ez::Drive::pid_tuner_button_left_set, chassis, PID_TUNER_PAGE_LEFT);
  set_if_bound(&ez::Drive::pid_tuner_button_right_set, chassis, PID_TUNER_PAGE_RIGHT);
}
