#include "main.h"

// Manual PID constants and motion defaults. Routes live in src/routes/.
void default_constants() {
  // P, I, D, and Start I
  chassis.pid_drive_constants_set(5.0, 0.0, 0.0);
  chassis.pid_heading_constants_set(5.0, 0.0, 0.0);
  chassis.pid_turn_constants_set(4.8, 0.0, 5.3, 0.0);
  chassis.pid_swing_constants_set(5.0, 0.0, 0.0);
  chassis.pid_odom_angular_constants_set(5.0, 0.0, 0.0);
  chassis.pid_odom_boomerang_constants_set(5.0, 0.0, 0.0);

  // Exit conditions
  chassis.pid_turn_exit_condition_set(90_ms, 3_deg, 250_ms, 7_deg, 500_ms, 500_ms);
  chassis.pid_swing_exit_condition_set(90_ms, 3_deg, 250_ms, 7_deg, 500_ms, 500_ms);
  chassis.pid_drive_exit_condition_set(90_ms, 1_in, 250_ms, 3_in, 500_ms, 500_ms);
  chassis.pid_odom_turn_exit_condition_set(90_ms, 3_deg, 250_ms, 7_deg, 500_ms, 750_ms);
  chassis.pid_odom_drive_exit_condition_set(90_ms, 1_in, 250_ms, 3_in, 500_ms, 750_ms);

  // Chain threshold: how close pid chain wait - releases
  chassis.pid_turn_chain_constant_set(5_deg);
  chassis.pid_swing_chain_constant_set(5_deg);
  chassis.pid_drive_chain_constant_set(5_in);

  // Slew constants
  chassis.slew_turn_constants_set(10_deg, 55);
  chassis.slew_drive_constants_set(3_in, 50);
  chassis.slew_swing_constants_set(3_in, 80);

  // The amount that turns are prioritized over driving in odom motions
  // - if you have tracking wheels, you can run this higher.  1.0 is the max
  chassis.odom_turn_bias_set(0.9);

  chassis.odom_look_ahead_set(7_in);
  chassis.odom_boomerang_distance_set(16_in);
  chassis.odom_boomerang_dlead_set(0.625);

  chassis.pid_angle_behavior_set(ez::shortest);
}

