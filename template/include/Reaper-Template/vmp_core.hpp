#pragma once

// [Reaper] VMP math with no VEX calls: turns one IMU's acceleration and rotation into still / moving,
// hits, and tilt.
#include <array>
#include <cmath>
#include <cstdint>

// VMP core: pure signal processing for one IMU. No PROS calls and no motor commands.
namespace vmp {

enum class State { disabled, invalid, warming, active, quiet, stale };

inline const char* state_name(State s) {
  switch (s) {
    case State::disabled:
      return "disabled";
    case State::invalid:
      return "invalid";
    case State::warming:
      return "warming";
    case State::active:
      return "active";
    case State::quiet:
      return "quiet";
    default:
      return "stale";
  }
}

struct Sample {
  std::uint32_t ms = 0;
  std::array<double, 3> accel{{0, 0, 0}};  // g, sensor-local axes; includes gravity
  std::array<double, 3> gyro{{0, 0, 0}};   // degrees/second, sensor-local axes
  bool valid = false;
};

struct Snapshot {
  Sample raw;
  std::array<double, 3> accel_band{{0, 0, 0}}, gyro_band{{0, 0, 0}};
  double accel_rms = 0;   // "shake": smoothed filtered acceleration, g
  double gyro_rms = 0;    // "rotation jitter": smoothed filtered rotation rate, deg/s
  double accel_peak = 0;  // biggest jolt in the last peak_window_ms, g
  double dt_ms = 0;
  State state = State::disabled;

  // Hits: each separate jolt above impact_g counts once.
  std::uint32_t impacts = 0;
  std::uint32_t last_impact_ms = 0;
  double last_impact_g = 0;

  // Tilt from the gravity direction, relative to the first still orientation after startup.
  double tilt_deg = NAN;
  bool tilt_reference = false;
};

struct Config {
  double quiet_accel_g = 0.025;
  double quiet_gyro_dps = 1.5;
  std::uint32_t quiet_hold_ms = 500;
  double impact_g = 1.0;
  std::uint32_t peak_window_ms = 2000;
};

// Signal chain per axis: high-pass 2 Hz -> low-pass 20 Hz -> exponential energy mean (250 ms).
// This is a motion-activity measure, not a gravity-compensated vibration estimate.
// Gravity direction for tilt: separate 1 Hz low-pass of the raw acceleration.
class Monitor {
 public:
  void configure(const Config& config) { config_ = config; }

  // Clears the signal state. The tilt reference survives (a sensor glitch mid-match must not re-zero tilt
  // while the robot may be leaning); clear it with tilt_reference_reset().
  void reset(State state = State::invalid) {
    initialized_ = false;
    in_impact_ = false;
    age_ = quiet_ = ea_ = eg_ = 0;
    const auto impacts = out_.impacts;
    const auto last_ms = out_.last_impact_ms;
    const auto last_g = out_.last_impact_g;
    out_ = Snapshot{};
    out_.state = state;
    out_.impacts = impacts;
    out_.last_impact_ms = last_ms;
    out_.last_impact_g = last_g;
    out_.tilt_reference = have_reference_;
    slow_ = {};
    band_ = {};
  }

  void tilt_reference_reset() {
    have_reference_ = false;
    out_.tilt_reference = false;
    out_.tilt_deg = NAN;
  }

  Snapshot update(const Sample& s) {
    for (double v : s.accel)
      if (!std::isfinite(v))
        return invalid(s.ms);
    for (double v : s.gyro)
      if (!std::isfinite(v))
        return invalid(s.ms);
    if (!s.valid)
      return invalid(s.ms);
    if (!initialized_) {
      seed(s);
      return out_;
    }
    const std::uint32_t elapsed = s.ms - out_.raw.ms;  // handles millis rollover
    if (elapsed == 0)
      return out_;  // no duplicated timestamp counted
    if (elapsed > 100) {
      reset();
      seed(s);
      return out_;
    }

    const double dt = elapsed / 1000.0;
    const double hp = 1 - std::exp(-2 * PI * 2 * dt);
    const double lp = 1 - std::exp(-2 * PI * 20 * dt);
    const double gravity_lp = 1 - std::exp(-2 * PI * 1 * dt);
    const double energy = 1 - std::exp(-dt / .25);

    double a2 = 0, g2 = 0;
    for (int i = 0; i < 6; ++i) {
      const double x = i < 3 ? s.accel[i] : s.gyro[i - 3];
      slow_[i] += hp * (x - slow_[i]);
      band_[i] += lp * ((x - slow_[i]) - band_[i]);
      if (i < 3) {
        out_.accel_band[i] = band_[i];
        a2 += band_[i] * band_[i];
        gravity_[i] += gravity_lp * (x - gravity_[i]);
      } else {
        out_.gyro_band[i - 3] = band_[i];
        g2 += band_[i] * band_[i];
      }
    }
    ea_ += energy * (a2 - ea_);
    eg_ += energy * (g2 - eg_);
    out_.raw = s;
    out_.dt_ms = elapsed;
    out_.accel_rms = std::sqrt(ea_);
    out_.gyro_rms = std::sqrt(eg_);

    const double jolt = std::sqrt(a2);
    update_peak(jolt, s.ms);
    update_impact(jolt, s.ms);

    age_ += dt;
    if (age_ < 1.0 - 1e-9) {
      out_.state = State::warming;
      quiet_ = 0;
    } else {
      if (out_.accel_rms < config_.quiet_accel_g && out_.gyro_rms < config_.quiet_gyro_dps)
        quiet_ += dt;
      else
        quiet_ = 0;
      out_.state = quiet_ >= config_.quiet_hold_ms / 1000.0 - 1e-9 ? State::quiet : State::active;
    }

    // First still moment after startup defines "flat".
    if (!have_reference_ && out_.state == State::quiet && length(gravity_) > 0.5) {
      reference_ = gravity_;
      have_reference_ = true;
    }
    out_.tilt_reference = have_reference_;
    out_.tilt_deg = have_reference_ ? angle_between(gravity_, reference_) : NAN;
    return out_;
  }

  Snapshot snapshot(std::uint32_t now) const {
    Snapshot r = out_;
    if (initialized_ && now - r.raw.ms > 100)
      r.state = State::stale;
    return r;
  }

 private:
  static constexpr double PI = 3.141592653589793;

  Snapshot invalid(std::uint32_t ms) {
    reset();
    out_.raw.ms = ms;
    return out_;
  }

  void seed(const Sample& s) {
    initialized_ = true;
    out_.raw = s;
    out_.state = State::warming;
    for (int i = 0; i < 3; ++i) {
      slow_[i] = s.accel[i];
      slow_[i + 3] = s.gyro[i];
      gravity_[i] = s.accel[i];
    }
  }

  void update_peak(double jolt, std::uint32_t now) {
    if (jolt >= out_.accel_peak || now - peak_ms_ > config_.peak_window_ms) {
      out_.accel_peak = jolt;
      peak_ms_ = now;
    }
  }

  // Counts one hit per separate jolt: it must drop below 70 % of the threshold before another counts.
  void update_impact(double jolt, std::uint32_t now) {
    if (!in_impact_ && config_.impact_g > 0 && jolt >= config_.impact_g) {
      in_impact_ = true;
      ++out_.impacts;
      out_.last_impact_ms = now;
      out_.last_impact_g = jolt;
    } else if (in_impact_) {
      if (jolt > out_.last_impact_g)
        out_.last_impact_g = jolt;
      if (jolt < 0.7 * config_.impact_g)
        in_impact_ = false;
    }
  }

  static double length(const std::array<double, 3>& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

  static double angle_between(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    const double la = length(a), lb = length(b);
    if (la <= 0 || lb <= 0)
      return NAN;
    double c = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb);
    c = c > 1 ? 1 : (c < -1 ? -1 : c);
    return std::acos(c) * 180.0 / PI;
  }

  Config config_;
  bool initialized_ = false, in_impact_ = false, have_reference_ = false;
  double age_ = 0, quiet_ = 0, ea_ = 0, eg_ = 0;
  std::uint32_t peak_ms_ = 0;
  std::array<double, 6> slow_{}, band_{};
  std::array<double, 3> gravity_{}, reference_{};
  Snapshot out_;
};

}  // namespace vmp
