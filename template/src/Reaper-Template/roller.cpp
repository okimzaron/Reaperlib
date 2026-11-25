// [Reaper] Colour roller task. See roller.hpp for what it does and how to use it.

#include "Reaper-Template/roller.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>

#include "Reaper-Template/lock.hpp"
#include "Reaper-Template/roller_settings.hpp"
#include "pros/device.h"
#include "pros/misc.hpp"
#include "pros/motors.h"
#include "pros/optical.h"
#include "pros/rtos.hpp"

namespace roller {
namespace {

// A request from any task. The roller task picks up the newest one; `id` tells it a new one arrived.
struct Request {
  Mode mode = Mode::Idle;
  int power = 0;
  std::uint32_t timeout_ms = 0;
  std::uint32_t id = 0;
};

Config config{0, 0, 0};
Thresholds thresholds;
std::atomic<Color> target{Color::Red};

// Guards request, published and published_id. Nothing inside the lock may block (see lock.hpp).
ez::Lock<pros::Mutex> lock;
Request request;
Status published;
std::uint32_t published_id = 0;  // The request id `published` reflects.

pros::task_t worker = nullptr;

bool motor_configured() { return config.motor_port != 0; }
bool optical_configured() { return config.optical_port >= 1 && config.optical_port <= 21; }

std::uint32_t post(Mode mode, int power = 0, std::uint32_t timeout_ms = 0) {
  ez::KillSafeGuard<pros::Mutex> guard(lock);
  request = {mode, power, timeout_ms, request.id + 1};
  return request.id;
}

// Stops the roller only if nobody has sent a newer request since `id`.
void stop_if_current(std::uint32_t id) {
  ez::KillSafeGuard<pros::Mutex> guard(lock);
  if (request.id == id)
    request = {Mode::Idle, 0, 0, request.id + 1};
}

void optical_setup() {
  pros::c::optical_disable_gesture(config.optical_port);
  pros::c::optical_set_led_pwm(config.optical_port, settings::LED_PWM);
  pros::c::optical_set_integration_time(config.optical_port, 10);
}

void run(void*) {
  ez::detail::mark_scheduler_running();
  ColorFilter filter;
  Status s;
  s.mode = Mode::Idle;
  std::uint32_t handled_id = 0, seek_started = 0, seek_timeout = 0;
  int manual_power = 0;
  bool braked = false;
  bool sensor_was_ok = false, sensor_reported = false;
  Color last_printed = Color::None;

  while (true) {
    const std::uint32_t now = pros::millis();

    Request r;
    {
      ez::PlainGuard<pros::Mutex> guard(lock);  // this task is never deleted
      r = request;
    }

    // ---- Read the optical sensor ------------------------------------------------------------------
    s.sensor_ok = false;
    s.raw = Color::None;
    s.hue = s.saturation = NAN;
    s.proximity = -1;
    if (optical_configured() &&
        pros::c::get_plugged_type(config.optical_port) == pros::c::E_DEVICE_OPTICAL) {
      if (!sensor_was_ok)
        optical_setup();  // first contact, or the sensor was re-plugged
      s.hue = pros::c::optical_get_hue(config.optical_port);
      s.saturation = pros::c::optical_get_saturation(config.optical_port);
      s.proximity = pros::c::optical_get_proximity(config.optical_port);
      s.sensor_ok = std::isfinite(s.hue) && s.hue >= 0 && s.hue < 360 && std::isfinite(s.saturation) &&
                    s.saturation >= 0 && s.saturation <= 1 && s.proximity >= 0 && s.proximity <= 255;
    }
    if (s.sensor_ok) {
      s.raw = classify(s.hue, s.saturation, s.proximity, thresholds);
      s.seen = filter.update(s.raw, now, settings::CONFIRM_MS);
    } else {
      filter.reset();
      s.seen = Color::None;
    }
    const Color goal = target.load();
    s.on_target = s.sensor_ok && s.seen == goal;

    // ---- Pick up a new request ----------------------------------------------------------------------
    if (r.id != handled_id) {
      handled_id = r.id;
      if (s.result == Result::Running)
        s.result = Result::Cancelled;  // a newer request replaced an unfinished seek
      s.mode = r.mode;
      manual_power = std::clamp(r.power, -127, 127);
      if (r.mode == Mode::Seek) {
        s.result = Result::Running;
        seek_started = now;
        seek_timeout = r.timeout_ms;
      }
    }

    // ---- Decide the motor power ---------------------------------------------------------------------
    int power = 0;
    if (!motor_configured()) {
      if (s.mode == Mode::Seek)
        s.result = Result::Fault;
      s.mode = optical_configured() ? Mode::Idle : Mode::Disabled;
    } else if (pros::competition::is_disabled()) {
      // Field disabled: stop, and do not resume the old command when the robot is re-enabled.
      if (s.result == Result::Running)
        s.result = Result::Cancelled;
      if (s.mode != Mode::Idle && s.mode != Mode::Disabled)
        s.mode = Mode::Idle;
    } else {
      switch (s.mode) {
        case Mode::Manual:
          power = manual_power;
          break;
        case Mode::Seek:
          if (!s.sensor_ok) {
            s.result = Result::Fault;
            s.mode = Mode::Idle;
          } else if (s.on_target) {
            s.result = Result::Reached;
            s.mode = Mode::Idle;
          } else if (now - seek_started >= seek_timeout) {
            s.result = Result::TimedOut;
            s.mode = Mode::Idle;
          } else {
            power = config.power;  // keep turning: no colour, yellow or the wrong alliance colour
          }
          break;
        case Mode::Maintain:
          // Without a sensor reading, stand still rather than spin blindly; resume when it returns.
          power = (s.sensor_ok && !s.on_target) ? config.power : 0;
          break;
        default:
          break;
      }
    }

    if (motor_configured()) {
      if (power == 0) {
        if (!braked)
          pros::c::motor_brake(config.motor_port);
        braked = true;
      } else {
        pros::c::motor_move(config.motor_port, power);
        braked = false;
      }
    }
    s.power = power;

    {
      ez::PlainGuard<pros::Mutex> guard(lock);
      published = s;
      published_id = handled_id;
    }

    // ---- Terminal readout (outside the lock: printing can block) ------------------------------------
    if (optical_configured() && s.sensor_ok != sensor_was_ok) {
      if (!s.sensor_ok)
        printf("[Roller] Optical sensor on port %d is not responding.\n", config.optical_port);
      else if (sensor_reported)
        printf("[Roller] Optical sensor on port %d is back.\n", config.optical_port);
      sensor_reported = true;
    }
    sensor_was_ok = s.sensor_ok;
    if (s.sensor_ok && s.seen != last_printed) {
      printf("[Roller] Sees %s (hue %.0f, sat %.2f, prox %d)\n", color_name(s.seen), s.hue, s.saturation,
             s.proximity);
      last_printed = s.seen;
    }

    pros::delay(10);
  }
}

}  // namespace

bool initialize(Config requested) {
  if (worker)
    return false;
  if (requested.motor_port < -21 || requested.motor_port > 21 || requested.optical_port < 0 ||
      requested.optical_port > 21 ||
      (requested.optical_port != 0 && std::abs(requested.motor_port) == requested.optical_port) ||
      requested.power < 1 || requested.power > 127 || !thresholds_valid(settings::thresholds())) {
    printf("[Roller] Invalid configuration (motor %d, optical %d, power %d); roller disabled.\n",
           requested.motor_port, requested.optical_port, requested.power);
    return false;
  }
  config = requested;
  thresholds = settings::thresholds();
  if (!motor_configured() && !optical_configured())
    return true;  // nothing fitted: no task, no device calls
  if (motor_configured())
    pros::c::motor_set_brake_mode(config.motor_port, pros::E_MOTOR_BRAKE_HOLD);
  worker = pros::c::task_create(run, nullptr, TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Reaper Roller");
  if (!worker) {
    printf("[Roller] Could not start the roller task.\n");
    return false;
  }
  return true;
}

void target_set(Color color) {
  if (color == Color::Red || color == Color::Blue)
    target.store(color);
}
Color target_get() { return target.load(); }

void spin(int power) {
  if (power == 0)
    post(Mode::Idle);
  else
    post(Mode::Manual, power);
}

void stop() { post(Mode::Idle); }

bool seek(std::uint32_t timeout_ms) {
  if (!worker || !motor_configured() || !optical_configured())
    return false;
  post(Mode::Seek, 0, timeout_ms ? timeout_ms : settings::DEFAULT_SEEK_TIMEOUT_MS);
  return true;
}

bool wait(std::uint32_t timeout_ms) {
  std::uint32_t expected;
  {
    ez::KillSafeGuard<pros::Mutex> guard(lock);
    if (!worker || request.mode != Mode::Seek)
      return false;
    expected = request.id;
  }
  const std::uint32_t start = pros::millis();
  while (pros::millis() - start < timeout_ms) {
    {
      ez::KillSafeGuard<pros::Mutex> guard(lock);
      if (request.id != expected)
        return false;  // replaced by another command
      if (published_id == expected && published.result != Result::Running)
        return published.result == Result::Reached;
    }
    pros::delay(10);
  }
  return false;
}

bool roll_to_target(std::uint32_t timeout_ms) {
  const std::uint32_t limit = timeout_ms ? timeout_ms : settings::DEFAULT_SEEK_TIMEOUT_MS;
  return seek(limit) && wait(limit + 200);
}

bool maintain() {
  if (!worker || !motor_configured() || !optical_configured())
    return false;
  post(Mode::Maintain);
  return true;
}

void maintain_for(std::uint32_t duration_ms) {
  if (!maintain())
    return;
  std::uint32_t id;
  {
    ez::KillSafeGuard<pros::Mutex> guard(lock);
    id = request.id;
  }
  const std::uint32_t start = pros::millis();
  while (pros::millis() - start < duration_ms && !pros::competition::is_disabled()) pros::delay(10);
  stop_if_current(id);
}

void driver_update(bool held) {
  static bool was_held = false;
  static std::uint32_t driver_id = 0;
  if (held && !was_held && maintain()) {
    ez::KillSafeGuard<pros::Mutex> guard(lock);
    driver_id = request.id;
  } else if (!held && was_held) {
    stop_if_current(driver_id);
  }
  was_held = held;
}

Status status() {
  ez::KillSafeGuard<pros::Mutex> guard(lock);
  return published;
}

const char* mode_name(Mode mode) {
  switch (mode) {
    case Mode::Idle:
      return "IDLE";
    case Mode::Manual:
      return "MANUAL";
    case Mode::Seek:
      return "SEEK";
    case Mode::Maintain:
      return "MAINTAIN";
    default:
      return "OFF";
  }
}

const char* result_name(Result result) {
  switch (result) {
    case Result::Running:
      return "RUNNING";
    case Result::Reached:
      return "REACHED";
    case Result::TimedOut:
      return "TIMED OUT";
    case Result::Cancelled:
      return "CANCELLED";
    case Result::Fault:
      return "FAULT";
    default:
      return "-";
  }
}

}  // namespace roller
