#pragma once

// [Reaper] Lift controller math with no VEX calls: PID, gravity compensation, gain scheduling, travel
// limits and fault detection. Kept device-free so it can be checked on a computer; lift.cpp feeds it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace lift {
enum class Mode { Disabled, Holding, Moving, Jogging, Fault };
enum class Fault { None, Sensor, EncoderMismatch, Configuration };
enum class GravityModel { None, Constant, Cosine };
struct GainPoint {
  double position, kp, ki, kd;
};
struct Config {
  // All position values remain MOTOR-equivalent degrees, including external feedback.
  double kp = .30, ki = 0, kd = .02;
  double integral_limit = 100, integral_zone = 0;
  double up_rate = 360, down_rate = 280, max_target_lead = 250;
  double max_up_power = 100, max_down_power = 80, return_power = 60;
  double slew_per_second = 400, tolerance = 5, settled_velocity = 10;
  std::uint32_t settle_ms = 150, move_timeout_ms = 4000;
  double max_height = 0;             // Unmeasured: positive automatic targets are rejected.
  bool allow_unbounded_jog = false;  // Explicit, temporary calibration opt-in only.
  double encoder_disagreement = 90;  // |left - right| in motor degrees; tune for measured backlash.
  std::uint32_t disagreement_ms = 150;
  GravityModel gravity = GravityModel::None;
  double kg = 0, horizontal = 0, degrees_per_motor_degree = 1;
  double feedforward_fraction = .8;
  std::vector<GainPoint> gains;
  // R2/L2 drive the lift at full power. Only takes effect once max_height is set:
  // while calibrating (max_height == 0) jogging always uses the slow up_rate/down_rate.
  bool full_power_manual_jog = false;
};
inline bool valid_config(const Config& c) {
  for (double x : {c.kp, c.ki, c.kd, c.integral_limit, c.integral_zone, c.up_rate, c.down_rate,
                   c.max_target_lead, c.max_up_power, c.max_down_power, c.return_power, c.slew_per_second,
                   c.tolerance, c.settled_velocity, c.max_height, c.encoder_disagreement})
    if (!std::isfinite(x) || x < 0)
      return false;
  if (c.max_up_power > 127 || c.max_down_power > 127 || c.return_power > 127 || c.tolerance <= 0 ||
      c.slew_per_second <= 0 || c.move_timeout_ms == 0 || c.settle_ms == 0 || c.disagreement_ms == 0 ||
      !std::isfinite(c.kg) || !std::isfinite(c.horizontal) || !std::isfinite(c.degrees_per_motor_degree) ||
      c.degrees_per_motor_degree == 0 || !std::isfinite(c.feedforward_fraction) ||
      c.feedforward_fraction < 0 || c.feedforward_fraction > 1)
    return false;
  if (c.gravity != GravityModel::None && c.gravity != GravityModel::Constant &&
      c.gravity != GravityModel::Cosine)
    return false;
  double last = -1;
  for (const auto& g : c.gains) {
    if (!std::isfinite(g.position) || g.position <= last || g.position < 0)
      return false;
    for (double k : {g.kp, g.ki, g.kd})
      if (!std::isfinite(k) || k < 0)
        return false;
    last = g.position;
  }
  return true;
}
inline bool valid_target(const Config& c, double x) {
  return std::isfinite(x) && x >= 0 && (x == 0 || (c.max_height > 0 && x <= c.max_height));
}
inline GainPoint gains_at(const Config& c, double x) {
  if (c.gains.empty())
    return {x, c.kp, c.ki, c.kd};
  if (x <= c.gains.front().position)
    return c.gains.front();
  for (std::size_t i = 1; i < c.gains.size(); ++i) {
    const auto &a = c.gains[i - 1], &b = c.gains[i];
    if (x <= b.position) {
      double t = (x - a.position) / (b.position - a.position);
      return {x, a.kp + t * (b.kp - a.kp), a.ki + t * (b.ki - a.ki), a.kd + t * (b.kd - a.kd)};
    }
  }
  return c.gains.back();
}
struct Preset {
  std::string name;
  double position;
};
class Presets {
 public:
  bool set(std::string name, double position, const Config& config) {
    if (name.empty() || !valid_target(config, position))
      return false;
    for (auto& p : values_)
      if (p.name == name) {
        p.position = position;
        sort();
        return true;
      }
    values_.push_back({name, position});
    sort();
    return true;
  }
  bool find(const std::string& name, double& target) const {
    for (const auto& p : values_)
      if (p.name == name) {
        target = p.position;
        return true;
      }
    return false;
  }
  std::string nearest(double position, double tolerance) const {
    std::string result;
    double distance = tolerance;
    for (const auto& p : values_)
      if (std::abs(p.position - position) <= distance) {
        result = p.name;
        distance = std::abs(p.position - position);
      }
    return result;
  }
  bool step(double position, double tolerance, bool up, double& target) const {
    if (up) {
      for (const auto& p : values_)
        if (p.position > position + tolerance) {
          target = p.position;
          return true;
        }
    } else {
      for (auto i = values_.rbegin(); i != values_.rend(); ++i)
        if (i->position < position - tolerance) {
          target = i->position;
          return true;
        }
    }
    return false;
  }

 private:
  std::vector<Preset> values_;
  void sort() {
    std::sort(values_.begin(), values_.end(),
              [](const Preset& a, const Preset& b) { return a.position < b.position; });
  }
};
struct Status {
  Mode mode = Mode::Disabled;
  double position = 0, target = 0, power = 0;
  bool settled = false, timed_out = false;
  Fault fault = Fault::None;
  double velocity = 0, encoder_difference = 0;
  bool upper_limit_configured = false;
  std::uint32_t command_id = 0;
};
class FeedbackGuard {
 public:
  bool check(double left, double right, bool installed, const Config& c, std::uint32_t now) {
    fault = Fault::None;
    if (!installed || !std::isfinite(left) || !std::isfinite(right)) {
      fault = Fault::Sensor;
      return false;
    }
    if (c.encoder_disagreement > 0 && std::abs(left - right) > c.encoder_disagreement) {
      if (!mismatch_) {
        mismatch_ = true;
        since_ = now;
      }
      if (now - since_ >= c.disagreement_ms) {
        fault = Fault::EncoderMismatch;
        return false;
      }
    } else
      mismatch_ = false;
    return true;
  }
  Fault fault = Fault::None;

 private:
  bool mismatch_ = false;
  std::uint32_t since_ = 0;
};
class Controller {
 public:
  explicit Controller(Config config = {}) : config_(config) {
    status.upper_limit_configured = config.max_height > 0;
    if (!valid_config(config))
      fault(Fault::Configuration);
  }
  Status status;
  void disable() {
    status.mode = Mode::Disabled;
    reset();
  }
  void fault(Fault reason = Fault::Sensor) {
    status.mode = Mode::Fault;
    status.fault = reason;
    reset();
  }
  void hold(double height) {
    if (!std::isfinite(height)) {
      fault();
      return;
    }
    status.mode = Mode::Holding;
    status.fault = Fault::None;
    status.target = limit(height);
    status.timed_out = false;
    previous_height_ = height;
    reset();
  }
  bool move(double target, double height, std::uint32_t now) {
    if (!valid_target(config_, target))
      return false;
    hold(height);
    if (status.mode == Mode::Fault)
      return false;
    status.target = target;
    status.mode = Mode::Moving;
    move_started_ = now;
    return true;
  }
  void jog(int direction, double height) {
    if (direction > 0 && config_.max_height == 0 && !config_.allow_unbounded_jog) {
      hold(height);
      return;
    }
    if (status.mode != Mode::Jogging || direction != jog_direction_)
      hold(height);
    if (status.mode == Mode::Fault)
      return;
    jog_direction_ = direction;
    status.mode = Mode::Jogging;
    status.settled = false;
  }
  double step(double height, double dt, std::uint32_t now) {
    if (!std::isfinite(height)) {
      fault();
      return 0;
    }
    status.position = height;
    if (status.mode == Mode::Disabled || status.mode == Mode::Fault)
      return 0;
    if (!std::isfinite(dt) || dt <= 0 || dt > .1) {
      hold(height);
      status.timed_out = true;
      return 0;
    }
    double velocity = (height - previous_height_) / dt;
    previous_height_ = height;
    status.velocity = velocity;
    if (status.mode == Mode::Moving && now - move_started_ >= config_.move_timeout_ms) {
      hold(height);
      status.timed_out = true;
      return 0;
    }
    if (status.mode == Mode::Jogging) {
      // Full power only once a measured top limit exists. Without one there is nothing to stop the
      // lift at the top, so calibration jogs stay at the gentle rate-limited speed below.
      if (config_.full_power_manual_jog && config_.max_height > 0) {
        status.target = limit(height);
        const bool at_bottom = height <= config_.tolerance;
        const bool at_top = height >= config_.max_height;
        if (jog_direction_ > 0)
          status.power = at_top ? 0 : 127;
        else
          status.power = at_bottom ? 0 : -127;
        return status.power;
      }
      status.target += (jog_direction_ > 0 ? config_.up_rate : -config_.down_rate) * dt;
      status.target = limit(
          std::clamp(status.target, height - config_.max_target_lead, height + config_.max_target_lead));
    }
    const double error = status.target - height;
    const bool near = std::abs(error) <= config_.tolerance;
    if (near && status.mode != Mode::Jogging && std::abs(velocity) <= config_.settled_velocity) {
      if (!settling_) {
        settling_ = true;
        settled_since_ = now;
      }
      status.settled = now - settled_since_ >= config_.settle_ms;
      if (status.settled && status.mode == Mode::Moving)
        status.mode = Mode::Holding;
    } else {
      settling_ = false;
      status.settled = false;
    }
    double gravity = 0;
    if (config_.gravity != GravityModel::None) {
      gravity = config_.kg;
      if (config_.gravity == GravityModel::Cosine)
        gravity *= std::cos((height - config_.horizontal) * config_.degrees_per_motor_degree *
                            3.141592653589793 / 180.0);
    }
    const double down = status.target <= config_.tolerance ? config_.return_power : config_.max_down_power;
    gravity = std::clamp(gravity, -down * config_.feedforward_fraction,
                         config_.max_up_power * config_.feedforward_fraction);
    // Preserve hardware HOLD behavior when feedforward is off. With it on, keep PID alive.
    if (near && gravity == 0) {
      status.power = 0;
      integral_ = 0;
      return 0;
    }
    const auto g = gains_at(config_, height);
    if (config_.integral_zone > 0 && std::abs(error) > config_.integral_zone)
      integral_ = 0;
    const bool integrate = config_.integral_zone == 0 || std::abs(error) <= config_.integral_zone;
    const double candidate =
        integrate ? std::clamp(integral_ + error * dt, -config_.integral_limit, config_.integral_limit)
                  : integral_;
    double raw = g.kp * error + g.ki * candidate - g.kd * velocity + gravity;
    // Saturation includes feedforward; never subtract an error we did not integrate.
    if ((raw <= config_.max_up_power && raw >= -down) || (raw > config_.max_up_power && error < 0) ||
        (raw < -down && error > 0))
      integral_ = candidate;
    raw = g.kp * error + g.ki * integral_ - g.kd * velocity + gravity;
    if (!std::isfinite(raw)) {
      fault(Fault::Configuration);
      return 0;
    }
    const double requested = std::clamp(raw, -down, config_.max_up_power);
    const double slew = config_.slew_per_second * dt;
    status.power += std::clamp(requested - status.power, -slew, slew);
    status.power = std::clamp(status.power, -down, config_.max_up_power);
    if ((height <= config_.tolerance && status.power < 0) ||
        (config_.max_height > 0 && height >= config_.max_height && status.power > 0)) {
      status.power = 0;
      integral_ = 0;
    }
    return status.power;
  }
  double integral() const { return integral_; }

 private:
  Config config_;
  double integral_ = 0, previous_height_ = 0;
  int jog_direction_ = 0;
  bool settling_ = false;
  std::uint32_t move_started_ = 0, settled_since_ = 0;
  double limit(double x) const {
    return config_.max_height > 0 ? std::clamp(x, 0.0, config_.max_height) : std::max(0.0, x);
  }
  void reset() {
    integral_ = 0;
    status.power = 0;
    status.settled = false;
    settling_ = false;
  }
};
}  // namespace lift
