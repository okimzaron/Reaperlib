// [Reaper] Motor monitor task. See motor_monitor.hpp.

#include "main.h"

namespace motor_monitor {
namespace {
std::vector<ez::MotorStatus> configured, published;
pros::Mutex mutex;
pros::task_t worker = nullptr;
std::uint32_t sampled_at = 0;
bool sampled = false;

std::vector<ez::MotorStatus> inventory() {
  std::vector<ez::MotorStatus> entries;
  int i = 0;
  for (int port : RobotHardware::DRIVE_LEFT_PORTS)
    entries.push_back({"Drive L" + std::to_string(++i), port});
  i = 0;
  for (int port : RobotHardware::DRIVE_RIGHT_PORTS)
    entries.push_back({"Drive R" + std::to_string(++i), port});
  entries.push_back({"Lift L", RobotHardware::LIFT_LEFT_PORT});
  entries.push_back({"Lift R", RobotHardware::LIFT_RIGHT_PORT});
  entries.push_back({"Claw", RobotHardware::CLAW_PORT});
  entries.push_back({"Roller", RobotHardware::ROLLER_MOTOR_PORT});
  std::vector<int> sensors{RobotHardware::IMU_1_PORT, RobotHardware::IMU_2_PORT,
    RobotHardware::LIFT_IMU_PORT, RobotHardware::DISTANCE_FRONT.port,
    RobotHardware::DISTANCE_RIGHT.port, RobotHardware::DISTANCE_BACK.port,
    RobotHardware::DISTANCE_LEFT.port, RobotHardware::ROLLER_OPTICAL_PORT};
  for (const auto& tracker : {RobotHardware::LEFT_TRACKER, RobotHardware::RIGHT_TRACKER})
    if (tracker.enabled && tracker.type != hardware::TrackerType::ADI)
      sensors.push_back(tracker.smart_port);
  return ez::configured_motors(std::move(entries), sensors);
}

void run(void*) {
  while (true) {
    auto next = configured;
    for (auto& entry : next) {
      if (ez::motor_configuration_error(entry.state)) continue;
      const auto device = pros::c::get_plugged_type(entry.port);
      const bool motor = device == pros::c::E_DEVICE_MOTOR;
      entry.temperature = motor ? pros::c::motor_get_temperature(entry.port) : NAN;
      entry.state = ez::motor_reading_state(motor, device == pros::c::E_DEVICE_NONE, entry.temperature);
    }
    // Device I/O never holds the publication lock or runs in the LVGL thread.
    if (mutex.take(10)) {
      published = std::move(next);
      sampled_at = pros::millis();
      sampled = true;
      mutex.give();
    }
    pros::delay(500);
  }
}
}
bool start() {
  if (worker) return true;
  configured = inventory();
  published = configured;
  worker = pros::c::task_create(run, nullptr, TASK_PRIORITY_DEFAULT - 1,
      TASK_STACK_DEPTH_DEFAULT, "Motor monitor");
  return worker != nullptr;
}
std::vector<ez::MotorStatus> snapshot() {
  if (!mutex.take(0)) {
    auto result = configured;
    ez::mark_motor_snapshot_stale(result);
    return result;
  }
  auto result = published;
  const bool stale = !sampled || pros::millis() - sampled_at > 1500;
  mutex.give();
  if (stale) ez::mark_motor_snapshot_stale(result);
  return result;
}
}
