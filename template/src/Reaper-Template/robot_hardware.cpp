// [Reaper] Builds the devices from robot_hardware.hpp and checks those settings when you build.

#include "main.h"

RobotHardware robot;

std::vector<int> RobotHardware::imu_ports() {
  std::vector<int> ports;
  if (IMU_1_PORT != 0) ports.push_back(IMU_1_PORT);
  if (IMU_2_PORT != 0) ports.push_back(IMU_2_PORT);
  return ports;
}
static_assert(RobotHardware::IMU_1_PORT >= 0 && RobotHardware::IMU_1_PORT <= 21,
              "IMU_1_PORT must be 0 (not used) or 1..21.");
static_assert(RobotHardware::IMU_2_PORT >= 0 && RobotHardware::IMU_2_PORT <= 21,
              "IMU_2_PORT must be 0 (not used) or 1..21.");
static_assert(RobotHardware::IMU_1_PORT == 0 || RobotHardware::IMU_1_PORT != RobotHardware::IMU_2_PORT,
              "IMU 1 and IMU 2 need different ports.");

static_assert(RobotHardware::ROLLER_MOTOR_PORT >= -21 && RobotHardware::ROLLER_MOTOR_PORT <= 21,
              "ROLLER_MOTOR_PORT must be 0 (disabled) or +/-1..21.");
static_assert(RobotHardware::ROLLER_OPTICAL_PORT >= 0 && RobotHardware::ROLLER_OPTICAL_PORT <= 21,
              "ROLLER_OPTICAL_PORT must be 0 (disabled) or 1..21.");
static_assert(RobotHardware::ROLLER_OPTICAL_PORT == 0 ||
                  RobotHardware::ROLLER_OPTICAL_PORT != (RobotHardware::ROLLER_MOTOR_PORT < 0
                                                             ? -RobotHardware::ROLLER_MOTOR_PORT
                                                             : RobotHardware::ROLLER_MOTOR_PORT),
              "The roller motor and its optical sensor need different ports.");
static_assert(RobotHardware::ROLLER_POWER >= 1 && RobotHardware::ROLLER_POWER <= 127,
              "ROLLER_POWER must be 1..127.");

std::array<int, 3> RobotHardware::vmp_ports() {
  return {IMU_1_PORT, IMU_2_PORT, LIFT_IMU_PORT};
}
localization::Config RobotHardware::localization_config() {
  localization::Config config;
  config.sensors = {DISTANCE_FRONT, DISTANCE_RIGHT, DISTANCE_BACK, DISTANCE_LEFT};
  config.field_half_size = FIELD_HALF_SIZE;
  config.sensor_delay_ms = DISTANCE_SENSOR_DELAY_MS;
  config.reset_max_jump = DISTANCE_RESET_MAX_JUMP;
  config.min_confidence = DISTANCE_MIN_CONFIDENCE;
  config.field_map = field_map::obstacles();  // include/field_map.hpp
  return config;
}
std::unique_ptr<ez::tracking_wheel> RobotHardware::make_tracker(const hardware::TrackerConfig& config) {
  if (!config.enabled) return nullptr;
  const std::vector<int> adi{config.adi_ports[0], config.adi_ports[1]};
  switch (config.type) {
    case hardware::TrackerType::Rotation:
      return std::make_unique<ez::tracking_wheel>(config.smart_port, config.wheel_diameter,
                                                 config.distance_to_center, config.gear_ratio);
    case hardware::TrackerType::ExpanderADI:
      return std::make_unique<ez::tracking_wheel>(config.smart_port, adi, config.wheel_diameter,
                                                 config.distance_to_center, config.gear_ratio);
    case hardware::TrackerType::ADI:
      return std::make_unique<ez::tracking_wheel>(adi, config.wheel_diameter,
                                                 config.distance_to_center, config.gear_ratio);
  }
  return nullptr;
}
void RobotHardware::configure_devices() {
  if (configured_) return;
  left_tracker = make_tracker(LEFT_TRACKER);
  right_tracker = make_tracker(RIGHT_TRACKER);
  if (left_tracker) chassis.odom_tracker_left_set(left_tracker.get());
  if (right_tracker) chassis.odom_tracker_right_set(right_tracker.get());
  roller::initialize({ROLLER_MOTOR_PORT, ROLLER_OPTICAL_PORT, ROLLER_POWER});
  configured_ = true;
}

// The optional lift sensor must not steal an existing smart device's port.
bool RobotHardware::lift_rotation_port_available() {
  const int requested = LIFT_ROTATION_PORT;
  if (requested < 1 || requested > 21) return false;
  std::vector<int> used = DRIVE_LEFT_PORTS;
  used.insert(used.end(), DRIVE_RIGHT_PORTS.begin(), DRIVE_RIGHT_PORTS.end());
  for (int port : {LIFT_LEFT_PORT, LIFT_RIGHT_PORT, CLAW_PORT, IMU_1_PORT, IMU_2_PORT, LIFT_IMU_PORT,
                  ROLLER_MOTOR_PORT, ROLLER_OPTICAL_PORT}) used.push_back(port);
  for (const auto& mount : {DISTANCE_FRONT, DISTANCE_RIGHT, DISTANCE_BACK, DISTANCE_LEFT})
    used.push_back(mount.port);
  for (const auto& tracker : {LEFT_TRACKER, RIGHT_TRACKER})
    if (tracker.enabled && tracker.type != hardware::TrackerType::ADI)
      used.push_back(tracker.smart_port);
  for (int port : used) if (std::abs(port) == requested) return false;
  return true;
}
