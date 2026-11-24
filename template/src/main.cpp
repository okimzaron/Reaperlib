#include "main.h"

// ---------------------------------------------------------------------------------------------------------
// Manual autonomous runner (TEST_AUTON_1 + TEST_AUTON_2 in robot_controls.hpp; no field controller only).
//
// The auton runs in its own task. That task never deletes itself: when the route finishes it raises
// `test_auton_done` and parks. Only this file deletes it, so the task handle is always valid while it is
// stored here (asking PROS about a task that already deleted itself reads freed memory).
// ---------------------------------------------------------------------------------------------------------
namespace {
pros::task_t test_auton_task = nullptr;
std::atomic<bool> test_auton_done{false};
bool test_auton_combo_latched = false;
pros::motor_brake_mode_e_t test_auton_previous_brake = MOTOR_BRAKE_COAST;

bool test_auton_is_running() { return test_auton_task != nullptr && !test_auton_done.load(); }

// Puts every mechanism back to a safe, idle state after a manual run ends or is cancelled.
void test_auton_release_robot() {
  lift::hold_current();
  roller::stop();
  ez::as::auton_selector.selection_lock(false);
  chassis.pid_targets_reset();
  chassis.drive_mode_set(ez::DISABLE, true);
  chassis.drive_set(0, 0);
  chassis.drive_brake_set(test_auton_previous_brake);
}

void test_auton_task_fn(void*) {
  autonomous();
  test_auton_done.store(true);
  while (true) pros::delay(1000);  // park; test_auton_end() deletes this task
}

// Deletes the auton task (finished or not) and releases the robot. Safe to call at any time.
void test_auton_end() {
  if (test_auton_task == nullptr) return;
  pros::c::task_delete(test_auton_task);
  test_auton_task = nullptr;
  test_auton_release_robot();
}

void test_auton_start() {
  if (test_auton_task != nullptr) return;
  roller::stop();
  test_auton_previous_brake = chassis.drive_brake_get();
  test_auton_done.store(false);
  test_auton_task = pros::c::task_create(test_auton_task_fn, nullptr, TASK_PRIORITY_DEFAULT,
                                         TASK_STACK_DEPTH_DEFAULT, "Reaper Test Auton");
  if (test_auton_task == nullptr) {
    printf("[Reaper] Failed to start test auton task.\n");
    test_auton_release_robot();
  } else {
    printf("[Reaper] Test auton started. Press %s + %s again to cancel.\n",
           btn::name(RobotControls::TEST_AUTON_1), btn::name(RobotControls::TEST_AUTON_2));
  }
}

void test_auton_shortcut_iterate() {
  // A route that finished on its own: tidy up so driving resumes.
  if (test_auton_task != nullptr && test_auton_done.load()) {
    test_auton_end();
    printf("[Reaper] Test auton finished.\n");
  }

  const bool combo =
      RobotControls::held(RobotControls::TEST_AUTON_1) && RobotControls::held(RobotControls::TEST_AUTON_2);
  if (combo && !test_auton_combo_latched) {
    test_auton_combo_latched = true;
    if (test_auton_task != nullptr) {  // cancellation always wins
      test_auton_end();
      printf("[Reaper] Test auton cancelled.\n");
    } else {
      test_auton_start();
    }
  } else if (!combo) {
    test_auton_combo_latched = false;
  }
}
// Caps driver drive speed while VMP reports tipping or lift sway (limits in vmp_settings.hpp), then puts
// back whatever cap was set before.
void vmp_drive_speed_limit() {
  static bool limited = false;
  static int saved_cap = 127;
  const auto alert = vmp::warning().alert;
  const int cap = alert == vmp::Alert::tipping ? vmp::settings::TIP_DRIVE_SPEED_LIMIT
                  : alert == vmp::Alert::sway  ? vmp::settings::SWAY_DRIVE_SPEED_LIMIT
                                               : 127;
  if (cap < 127) {
    if (!limited) {
      saved_cap = chassis.opcontrol_speed_max_get();
      limited = true;
    }
    chassis.opcontrol_speed_max_set(std::min(cap, saved_cap));
  } else if (limited) {
    chassis.opcontrol_speed_max_set(saved_cap);
    limited = false;
  }
}
}  // namespace

// ---------------------------------------------------------------------------------------------------------
// Competition entry points
// ---------------------------------------------------------------------------------------------------------
void initialize() {
  register_routes();

  // Restore the route saved on the SD card. This deliberately does NOT start EZ-Template's LLEMU screen:
  // the touch dashboard owns the brain screen, and LLEMU would flash up before the Reaper splash.
  ez::as::auton_selector_initialize();
  ez::as::auton_selector.selected_auton_print();

  motor_monitor::start();
  ez::as::auton_selector.dashboard_configure(team_theme::LOGO, motor_monitor::snapshot);  // name/logo/colors: team_theme.hpp
  ez::as::auton_selector.ui_initialize();
  reaper_splash::start();  // complete swing/strike/shatter in 3 s during calibration
  robot.configure_devices();
  printf("[Reaper] Reaper %s (EZ-Template %s) initializing...\n", REAPER_VERSION_STRING,
         EZ_TEMPLATE_VERSION_STRING);
  lift::initialize();

  RobotControls::apply(chassis);  // curve + PID tuner buttons from robot_controls.hpp
  chassis.opcontrol_drive_activebrake_set(0.0);
  chassis.opcontrol_curve_default_set(0.0, 0.0);
  default_constants();

  chassis.initialize(false);  // false = skip EZ's IMU loading bar (the Reaper splash replaces it)

  // VMP stage 1 only observes. All-zero assignments leave monitoring disabled.
  if (!vmp::start(RobotHardware::vmp_ports(), RobotHardware::VMP_CONSOLE_CSV))
    printf("[VMP] Invalid/duplicate ports or monitoring already started.\n");

  if (!localization::start(chassis, RobotHardware::localization_config()))
    printf("[Reaper] Localization configuration rejected.\n");

  reaper_splash::finish();  // non-blocking; never starts an additional animation

  master.rumble(chassis.drive_imu_calibrated() ? "." : "---");
  if (!controller_ui::start()) printf("[Controller UI] Could not start display task.\n");
}

void disabled() {
  test_auton_end();  // a manual run must never outlive the robot being disabled
  lift::disable();
  roller::stop();
  ez::as::auton_selector.selection_lock(false);
}

void competition_initialize() {}

void autonomous() {
  chassis.pid_targets_reset();                // Resets PID targets to 0
  chassis.drive_imu_reset();                  // Reset gyro position to 0
  chassis.drive_sensor_reset();               // Reset drive sensors to 0
  chassis.odom_xyt_set(0_in, 0_in, 0_deg);    // Routes should set their real field position (see https://reaperlib.online/docs/#autons)
  chassis.drive_brake_set(MOTOR_BRAKE_HOLD);  // Set motors to hold. This helps autonomous consistency

  lift::enable();

  // RW uses the same heading and pose as EZ motion control.
  // Pose resets automatically recenter the RW particle filter.
  roller::stop();
  ez::as::auton_selector.selected_auton_call();
  roller::stop();
}

void opcontrol() {
  chassis.drive_brake_set(MOTOR_BRAKE_COAST);
  lift::enable();

  while (true) {
    if (!pros::competition::is_connected())
      test_auton_shortcut_iterate();
    else
      test_auton_end();  // never let a manual run continue under field control

    // While a manually launched auton is running it owns the robot. Keep looping so the test-auton
    // buttons can cancel it, but send no driver commands that would overwrite the autonomous outputs.
    if (test_auton_is_running()) {
      pros::delay(ez::util::DELAY_TIME);
      continue;
    }

    // Every button below comes from include/robot_controls.hpp -- change bindings there, not here.
    using C = RobotControls;
    vmp_drive_speed_limit();
    C::drive(chassis);

    lift::driver_update(C::held(C::LIFT_UP), C::held(C::LIFT_DOWN), C::pressed(C::LIFT_RETURN));

    lift::claw_driver_update(C::held(C::CLAW_REVERSE)   ? -C::CLAW_REVERSE_POWER
                             : C::held(C::CLAW_FORWARD) ? C::CLAW_FORWARD_POWER
                                                        : 0);

    roller::driver_update(C::held(C::ROLLER_TO_COLOR));

    pros::delay(ez::util::DELAY_TIME);  // This is used for timer calculations! Keep this ez::util::DELAY_TIME
  }
}
