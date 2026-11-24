#include "Reaper-Template/localization.hpp"
#include "Reaper-Template/lock.hpp"
#include "Reaper-Template/vmp.hpp"
#include "Reaper-Template/vmp_settings.hpp"
#include "pros/distance.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
namespace localization {
namespace {
ez::Drive* drive = nullptr;
Config config;
std::unique_ptr<Engine> engine;
ez::Lock<pros::Mutex> mutex;
pros::Task* task = nullptr;
std::atomic<ResetResult> last_result{ResetResult::None};
Sample sample() {
  const auto s = drive->localization_sample();
  return {{s.position.x, s.position.y, s.position.theta}, s.motion_x, s.motion_y, s.epoch, s.ms, s.ready};
}
double read(unsigned i) {
  const int port = config.sensors[i].port;
  if (port < 1 || port > 21)
    return -1;
  const auto mm = pros::c::distance_get(port);
  if (mm <= 0 || mm > 2000)
    return -1;
  // Confidence only exists past 200 mm (closer, the sensor reports a fixed value), so only gate there.
  if (config.min_confidence > 0 && mm > 200) {
    const auto confidence = pros::c::distance_get_confidence(port);
    if (confidence == PROS_ERR || confidence < config.min_confidence)
      return -1;
  }
  return mm / 25.4;
}

// Records why a reset ended and returns whether it was applied.
bool report(ResetResult result) {
  last_result.store(result);
  return result == ResetResult::Applied;
}
void run() {
  std::uint32_t wake = pros::millis();
  while (true) {
    std::array<double, 4> readings;
    for (unsigned i = 0; i < 4; ++i)
      readings[i] = read(i);
    // Right after a hit the robot may have been shoved and the sensors may be seeing another robot, so the
    // filter keeps running but its corrections are not applied (checked outside the lock: vmp has its own).
    const std::uint32_t holdoff = vmp::settings::LOCALIZATION_HOLDOFF_MS;
    const bool recently_hit = holdoff > 0 && vmp::impact_since(pros::millis() - holdoff);
    {
      ez::PlainGuard<pros::Mutex> guard(mutex);
      const auto s = sample();
      const auto correction = engine->update(s, readings);
      if (!recently_hit && correction.valid && (correction.x != 0 || correction.y != 0))
        drive->localization_correct(s.epoch, correction.x, correction.y);
    }
    pros::Task::delay_until(&wake, 10);
  }
}
}  // namespace
bool start(ez::Drive& chassis, const Config& requested) {
  if (drive || !valid_config(requested))
    return false;
  config = requested;
  drive = &chassis;
  engine.reset(new Engine(config));
  bool any = false;
  for (const auto& m : config.sensors)
    any = any || m.port != 0;
  if (any)
    task = new pros::Task(run);
  return true;
}
void set_pose(Pose p) {
  if (!drive || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.theta))
    return;
  drive->odom_xyt_set(p.x, p.y, p.theta);
}
Pose getPose() {
  if (!drive)
    return {};
  const auto s = sample();
  return s.pose;
}
Diagnostics diagnostics() {
  ez::KillSafeGuard<pros::Mutex> guard(mutex);
  return engine ? engine->diagnostics() : Diagnostics{};
}
bool distance_reset(Sensor sensor, std::uint32_t average_ms) {
  const unsigned i = static_cast<unsigned>(sensor);
  if (!drive || !task || i >= 4)
    return report(ResetResult::NotRunning);

  double reading = -1;
  if (average_ms == 0) {
    reading = read(i);
    if (reading < 0)
      return report(ResetResult::NoReading);
  } else {
    // Sample at the sensor's own update rate so the same reading isn't counted several times.
    constexpr std::uint32_t SENSOR_PERIOD_MS = 35;
    const Sample before = sample();
    std::vector<double> readings;
    const std::uint32_t start = pros::millis();
    do {
      const double r = read(i);
      if (r >= 0)
        readings.push_back(r);
      pros::delay(SENSOR_PERIOD_MS);
    } while (pros::millis() - start < average_ms);
    const Sample after = sample();

    if (std::hypot(after.motion_x - before.motion_x, after.motion_y - before.motion_y) > 0.1 ||
        std::fabs(std::remainder(after.pose.theta - before.pose.theta, 360.0)) > 0.5)
      return report(ResetResult::RobotMoved);
    if (readings.empty())
      return report(ResetResult::NoReading);
    if (readings.size() < 3)
      return report(ResetResult::TooFewReadings);
    std::sort(readings.begin(), readings.end());
    if (readings.back() - readings.front() > 1.0)
      return report(ResetResult::Unsteady);
    const std::size_t n = readings.size();
    reading = n % 2 ? readings[n / 2] : (readings[n / 2 - 1] + readings[n / 2]) / 2.0;  // median
  }

  ez::KillSafeGuard<pros::Mutex> guard(mutex);
  const auto s = sample();
  if (engine->sensor_blocked(s, sensor))
    return report(ResetResult::Blocked);
  const auto c = engine->distance_reset(s, sensor, reading);
  if (!c.valid || !drive->localization_correct(s.epoch, c.x, c.y, true))
    return report(ResetResult::Refused);
  engine->reset(sample());
  return report(ResetResult::Applied);
}

ResetResult last_reset_result() { return last_result.load(); }

const char* reset_result_name(ResetResult result) {
  switch (result) {
    case ResetResult::Applied:
      return "applied";
    case ResetResult::NotRunning:
      return "localization not running";
    case ResetResult::NoReading:
      return "no usable reading";
    case ResetResult::TooFewReadings:
      return "too few readings";
    case ResetResult::Unsteady:
      return "readings unsteady";
    case ResetResult::RobotMoved:
      return "robot moved";
    case ResetResult::Blocked:
      return "field element in the way";
    case ResetResult::Refused:
      return "refused (angle, not settled, or jump too big)";
    default:
      return "none";
  }
}
}  // namespace localization
