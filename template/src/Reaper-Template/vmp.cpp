// [Reaper] VMP task. See vmp.hpp for what it watches and how to use it.

#include "Reaper-Template/vmp.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>

#include "Reaper-Template/lock.hpp"
#include "Reaper-Template/subsystems.hpp"  // chassis, for settle()
#include "Reaper-Template/vmp_settings.hpp"
#include "pros/imu.h"
#include "pros/rtos.hpp"

namespace vmp {
namespace {
constexpr int DRIVE_SLOTS = 2;  // slots 0 and 1 = IMU 1 and IMU 2 (chassis); slot 2 = lift IMU
constexpr int LIFT_SLOT = 2;

std::array<int, 3> ports_{{0, 0, 0}};
std::array<Monitor, 3> monitors_;  // owned by the observe task after start()
std::atomic<bool> reset_tilt_{false};

// Guards published_ and warning_. Nothing inside may block (see lock.hpp).
ez::Lock<pros::Mutex> lock_;
std::array<Snapshot, 3> published_;
Warning warning_;

pros::task_t task_ = nullptr;
pros::task_t logger_ = nullptr;
bool started_ = false;

bool drive_slot_used(int i) { return ports_[i] != 0; }

Sample read_imu(int port, std::uint32_t now) {
  Sample s;
  s.ms = now;
  errno = 0;
  const auto status = pros::c::imu_get_status(port);
  if (errno != 0 || status == pros::E_IMU_STATUS_ERROR || (status & pros::E_IMU_STATUS_CALIBRATING))
    return s;
  errno = 0;
  const auto a = pros::c::imu_get_accel(port);
  const bool accel_ok = errno == 0;
  errno = 0;
  const auto g = pros::c::imu_get_gyro_rate(port);
  s.accel = {{a.x, a.y, a.z}};
  s.gyro = {{g.x, g.y, g.z}};
  s.valid = accel_ok && errno == 0;
  return s;
}

// Tip / sway warning with hysteresis, from this cycle's snapshots.
Warning evaluate(const std::array<Snapshot, 3>& rows, Alert previous) {
  Warning w;
  for (int i = 0; i < DRIVE_SLOTS; ++i) {
    const auto& r = rows[i];
    if (drive_slot_used(i) && r.tilt_reference && std::isfinite(r.tilt_deg) &&
        (!std::isfinite(w.tilt_deg) || r.tilt_deg > w.tilt_deg))
      w.tilt_deg = r.tilt_deg;
  }
  const auto& lift = rows[LIFT_SLOT];
  if (ports_[LIFT_SLOT] != 0 && (lift.state == State::active || lift.state == State::quiet))
    w.lift_sway_dps = lift.gyro_rms;

  const double tip_limit = previous == Alert::tipping ? settings::TIP_CLEAR_DEG : settings::TIP_WARN_DEG;
  const double sway_limit = previous == Alert::sway ? settings::SWAY_CLEAR_DPS : settings::SWAY_GYRO_DPS;
  if (std::isfinite(w.tilt_deg) && w.tilt_deg >= tip_limit)
    w.alert = Alert::tipping;
  else if (std::isfinite(w.lift_sway_dps) && w.lift_sway_dps >= sway_limit)
    w.alert = Alert::sway;
  return w;
}

void observe(void*) {
  ez::detail::mark_scheduler_running();
  std::uint32_t wake = pros::millis();
  Alert previous = Alert::none;
  while (true) {
    if (reset_tilt_.exchange(false))
      for (auto& m : monitors_) m.tilt_reference_reset();

    std::array<Snapshot, 3> next;
    for (int i = 0; i < 3; ++i) {
      if (ports_[i] == 0) {
        next[i].state = State::disabled;
        continue;
      }
      next[i] = monitors_[i].update(read_imu(ports_[i], pros::millis()));
    }
    const Warning w = evaluate(next, previous);
    previous = w.alert;
    {
      ez::PlainGuard<pros::Mutex> guard(lock_);  // this task is never deleted
      published_ = next;
      warning_ = w;
    }
    pros::Task::delay_until(&wake, 10);
  }
}

void log_csv(void*) {
  // Separate low-priority task: console IO cannot block the sampling task.
  std::puts(
      "VMP,ms,slot,state,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,accel_rms_g,gyro_rms_dps,peak_g,impacts,tilt_deg,dt_ms");
  while (true) {
    const auto rows = snapshot();
    for (int i = 0; i < 3; ++i) {
      const auto& r = rows[i];
      std::printf("VMP,%lu,%d,%s,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.4f,%.3f,%.4f,%lu,%.2f,%.1f\n",
                  static_cast<unsigned long>(r.raw.ms), i, state_name(r.state), r.raw.accel[0], r.raw.accel[1],
                  r.raw.accel[2], r.raw.gyro[0], r.raw.gyro[1], r.raw.gyro[2], r.accel_rms, r.gyro_rms,
                  r.accel_peak, static_cast<unsigned long>(r.impacts), r.tilt_deg, r.dt_ms);
    }
    pros::delay(100);  // 10 Hz export of the latest 100 Hz snapshot
  }
}

// Signed difference so it stays right across millis() rollover and for since_ms in the future.
bool at_or_after(std::uint32_t t, std::uint32_t since) { return static_cast<std::int32_t>(t - since) >= 0; }
}  // namespace

bool start(const std::array<int, 3>& ports, bool console_csv) {
  if (started_)
    return false;
  bool any = false;
  for (int i = 0; i < 3; ++i) {
    if (ports[i] < 0 || ports[i] > 21)
      return false;
    for (int j = 0; j < i; ++j)
      if (ports[i] != 0 && ports[i] == ports[j])
        return false;
    any = any || ports[i] != 0;
  }
  Config config;
  config.quiet_accel_g = settings::QUIET_ACCEL_G;
  config.quiet_gyro_dps = settings::QUIET_GYRO_DPS;
  config.quiet_hold_ms = settings::QUIET_HOLD_MS;
  config.impact_g = settings::IMPACT_G;
  config.peak_window_ms = settings::PEAK_WINDOW_MS;

  ports_ = ports;
  for (int i = 0; i < 3; ++i) {
    monitors_[i].configure(config);
    monitors_[i].reset(ports[i] ? State::invalid : State::disabled);
    published_[i] = monitors_[i].snapshot(pros::millis());
  }
  started_ = true;
  if (!any)
    return true;  // no task and no device API calls for fresh-install ports
  task_ = pros::c::task_create(observe, nullptr, TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "VMP monitor");
  if (console_csv)
    logger_ = pros::c::task_create(log_csv, nullptr, TASK_PRIORITY_DEFAULT - 1, TASK_STACK_DEPTH_DEFAULT, "VMP CSV");
  return task_ != nullptr;
}

std::array<Snapshot, 3> snapshot() {
  std::array<Snapshot, 3> r;
  {
    ez::KillSafeGuard<pros::Mutex> guard(lock_);
    r = published_;
  }
  const auto now = pros::millis();
  for (auto& row : r)
    if (row.raw.valid && now - row.raw.ms > 100)
      row.state = State::stale;
  return r;
}

bool is_quiet() {
  const auto rows = snapshot();
  bool any = false;
  for (int i = 0; i < DRIVE_SLOTS; ++i) {
    if (!drive_slot_used(i))
      continue;
    any = true;
    if (rows[i].state != State::quiet)
      return false;
  }
  return any;
}

bool wait_quiet(std::uint32_t timeout_ms) {
  if (!task_ || (!drive_slot_used(0) && !drive_slot_used(1)))
    return false;
  const std::uint32_t start_ms = pros::millis();
  while (true) {
    if (is_quiet())
      return true;
    if (pros::millis() - start_ms >= timeout_ms)
      return false;
    pros::delay(10);
  }
}

bool settle(std::uint32_t timeout_ms) {
  chassis.pid_wait();  // the PID's exit conditions: the motion is over as far as the encoders/heading know
  if (!task_ || (!drive_slot_used(0) && !drive_slot_used(1))) {
    pros::delay(80);  // no chassis IMU to ask: a short fixed pause instead
    return false;
  }
  return wait_quiet(timeout_ms);  // the robot has actually stopped rocking
}

Impact last_impact() {
  const auto rows = snapshot();
  Impact best;
  for (int i = 0; i < DRIVE_SLOTS; ++i) {
    const auto& r = rows[i];
    if (!drive_slot_used(i) || r.impacts == 0)
      continue;
    if (!best.any || at_or_after(r.last_impact_ms, best.ms)) {
      best.any = true;
      best.ms = r.last_impact_ms;
      best.g = r.last_impact_g;
      best.slot = i;
    }
  }
  return best;
}

bool impact_since(std::uint32_t since_ms) {
  const Impact last = last_impact();
  return last.any && at_or_after(last.ms, since_ms);
}

Warning warning() {
  ez::KillSafeGuard<pros::Mutex> guard(lock_);
  return warning_;
}

const char* alert_name(Alert alert) {
  switch (alert) {
    case Alert::tipping:
      return "TIPPING";
    case Alert::sway:
      return "LIFT SWAY";
    default:
      return "none";
  }
}

void tilt_reference_reset() { reset_tilt_.store(true); }

}  // namespace vmp
