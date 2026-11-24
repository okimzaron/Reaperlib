#pragma once
#include <array>
#include <memory>
#include <vector>
#include "Reaper-Template/api.hpp"
#include "Reaper-Template/localization.hpp"
#include "field_map.hpp"
#include "Reaper-Template/roller.hpp"

namespace hardware {
enum class TrackerType { Rotation, ADI, ExpanderADI };
struct TrackerConfig {
  bool enabled;
  TrackerType type;
  int smart_port; // Rotation sensor or ADI expander port; unused for brain ADI.
  std::array<int, 2> adi_ports; // Brain/expander encoder channels, e.g. {'A','B'}.
  double wheel_diameter;
  double distance_to_center;
  double gear_ratio;
};
}

// MASTER HARDWARE CONFIGURATION
// Edit the SETTINGS section here, then rebuild/upload. No port edits elsewhere.
// Negative motor ports reverse that device. Do not also reverse commands unless intended.
class RobotHardware {
 public:
  // ========================== SETTINGS ==========================
  // Drivetrain: signed ports, wheel diameter in inches, effective wheel RPM.
  static inline const std::vector<int> DRIVE_LEFT_PORTS{0, 0};
  static inline const std::vector<int> DRIVE_RIGHT_PORTS{0, 0};
  // Use the measured sizes in Reaper-Template/wheel_sizes.hpp, e.g. ez::wheel::NEW_325 or ez::wheel::OLD_4.
  static constexpr double DRIVE_WHEEL_DIAMETER = ez::wheel::NEW_325;
  static constexpr double DRIVE_WHEEL_RPM = 450.0;

  // Mechanisms. Existing direction conventions are preserved.
  static constexpr int LIFT_LEFT_PORT = 0;  // 0 disables. Signed: negative reverses.
  static constexpr int LIFT_RIGHT_PORT = 0; // 0 disables. The motors face each other, so give this the
                                            // opposite sign of LEFT. R2 must make both count up.
  static constexpr int CLAW_PORT = 0;
  static constexpr int LIFT_ROTATION_PORT = 0; // 0 disables; use an unused smart port.
  // Signed conversion: sensor degrees -> existing MOTOR-equivalent lift degrees.
  static constexpr double LIFT_MOTOR_DEGREES_PER_SENSOR_DEGREE = 1.0;

  // Shared chassis IMUs: EZ heading/localization and VMP use these SAME entries.
  static constexpr int DRIVE_IMU_1_PORT = 0;
  static constexpr int DRIVE_IMU_2_PORT = 0; // 0 disables the second IMU.
  static constexpr int LIFT_IMU_PORT = 0;    // Optional VMP monitor; 0 disables.
  static constexpr bool VMP_CONSOLE_CSV = false;

  // Distance sensor face offsets: inches from turning center, +X right, +Y forward.
  // Heading is clockwise from forward. Scale is a distance calibration multiplier.
  //                              port, x,   y,   heading, scale
  static constexpr localization::Mount DISTANCE_FRONT{0, 0.0, 0.0,   0.0, 1.0};
  static constexpr localization::Mount DISTANCE_RIGHT{0, 0.0, 0.0,  90.0, 1.0};
  static constexpr localization::Mount DISTANCE_BACK {0, 0.0, 0.0, 180.0, 1.0};
  static constexpr localization::Mount DISTANCE_LEFT {0, 0.0, 0.0, 270.0, 1.0};
  static constexpr double FIELD_HALF_SIZE = 70.25;
  static constexpr unsigned DISTANCE_SENSOR_DELAY_MS = 40;
  static constexpr double DISTANCE_RESET_MAX_JUMP = 12.0;
  // Ignore readings the sensor itself is unsure of (0-63; only applies past 200 mm). 0 = off.
  static constexpr int DISTANCE_MIN_CONFIDENCE = 60;

  // Tracking wheels: enable only after setting a valid sensor type and its ports.
  // Disabled legacy settings are preserved; {13,0} is NOT a valid ADI encoder pair.
  // For a Rotation sensor on port 13, choose Rotation, smart_port=13, adi_ports={0,0}.
  // Fields: enabled, type, smart_port, ADI pair, diameter(in), offset(in), ratio.
  static constexpr hardware::TrackerConfig LEFT_TRACKER{
      false, hardware::TrackerType::ADI, 0, {0, 0}, ez::wheel::NEW_275, 4.0, 1.0};
  static constexpr hardware::TrackerConfig RIGHT_TRACKER{
      false, hardware::TrackerType::ADI, 0, {0, 0}, ez::wheel::NEW_275, 4.0, 1.0};

  // Colour roller: its own motor and optical sensor, independent of the intake and every other mechanism.
  // Port 0 disables that device. The optical sensor alone (motor 0) still reports colours for testing.
  // Colour thresholds live in Reaper-Template/roller_settings.hpp.
  static constexpr int ROLLER_MOTOR_PORT = 0;    // Signed: negative reverses.
  static constexpr int ROLLER_OPTICAL_PORT = 0;  // Optical sensor that faces the roller.
  static constexpr int ROLLER_POWER = 80;        // 1..127, speed while seeking a colour.
  // ======================== END SETTINGS ========================

  // One owner for each configured motor/chassis. Do not duplicate these objects.
  pros::Motor lift_left{LIFT_LEFT_PORT};
  pros::Motor lift_right{LIFT_RIGHT_PORT};
  pros::Motor claw{CLAW_PORT};
  // Declare tracker ownership before chassis so chassis is destroyed first.
  std::unique_ptr<ez::tracking_wheel> left_tracker;
  std::unique_ptr<ez::tracking_wheel> right_tracker;
  ez::Drive chassis{DRIVE_LEFT_PORTS, DRIVE_RIGHT_PORTS, drive_imu_ports(),
                    DRIVE_WHEEL_DIAMETER, DRIVE_WHEEL_RPM};

  RobotHardware() = default;
  RobotHardware(const RobotHardware&) = delete;
  RobotHardware& operator=(const RobotHardware&) = delete;
  void configure_devices(); // Before chassis.initialize(); creates enabled trackers once.
  static std::vector<int> drive_imu_ports();
  static std::array<int, 3> vmp_ports();
  static bool lift_rotation_port_available();
  static localization::Config localization_config();
 private:
  bool configured_ = false;
  static std::unique_ptr<ez::tracking_wheel> make_tracker(const hardware::TrackerConfig& config);
};

extern RobotHardware robot;
