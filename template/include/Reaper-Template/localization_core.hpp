#pragma once

// [Reaper] Localization engine with no VEX calls. Decides which distance readings to trust (range,
// confidence, angle, field map, sensor lag), feeds them to RW's particle filter, and checks wall resets.
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "Reaper-Template/field_map_core.hpp"
#include "Reaper-Template/rw/mcl.hpp"
#include "Reaper-Template/rw/utils.hpp"

namespace localization {
// Field center is (0,0); +X right, +Y forward, headings clockwise from +Y.
struct Pose {
  double x = 0, y = 0, theta = 0;
};
enum class Sensor : unsigned { front, right, back, left };
struct Mount {
  int port = 0;
  double x = 0, y = 0, angle = 0, scale = 1;
};
struct Config {
  std::array<Mount, 4> sensors{{{0, 0, 0, 0, 1}, {0, 0, 0, 90, 1}, {0, 0, 0, 180, 1}, {0, 0, 0, 270, 1}}};
  int particles = 200;
  double field_half_size = 70.25, max_range = 60, max_angle = 30, corner_margin = 0;
  double reset_max_jump = 12;
  unsigned sensor_delay_ms = 40;
  // Distance sensors report how sure they are (0-63) for readings over 200 mm. Below this, the reading is
  // ignored (angled or shiny surfaces, a beam half on an object). 0 turns the check off.
  int min_confidence = 60;
  // Field elements that can block a sensor's view of the wall (include/field_map.hpp). Empty = no checks.
  FieldMap field_map;
};
struct Sample {
  Pose pose;
  double motion_x = 0, motion_y = 0;
  std::uint32_t epoch = 0, ms = 0;
  bool ready = false;
};
struct Correction {
  double x = 0, y = 0;
  bool valid = false;
};
struct Diagnostics {
  Pose estimate;
  double spread_x = 0, spread_y = 0, speed_ips = 0, turn_dps = 0;
  unsigned accepted = 0;
  unsigned blocked = 0;  // sensors skipped last update because a mapped field element was in the way
  bool ready = false;
};
inline bool valid_config(const Config& c) {
  if (c.particles < 1 || c.particles > 1000 || !std::isfinite(c.field_half_size) || c.field_half_size <= 0 ||
      !std::isfinite(c.max_range) || c.max_range < 1.5 || c.max_range > 2000.0 / 25.4 ||
      !std::isfinite(c.max_angle) || c.max_angle < 0 || c.max_angle > 45 || !std::isfinite(c.corner_margin) ||
      c.corner_margin < 0 || c.corner_margin >= c.field_half_size || !std::isfinite(c.reset_max_jump) ||
      c.reset_max_jump < 0 || c.sensor_delay_ms > 150 || c.min_confidence < 0 || c.min_confidence > 63 ||
      !field_map_valid(c.field_map))
    return false;
  for (unsigned i = 0; i < 4; ++i) {
    const auto& m = c.sensors[i];
    if (m.port < 0 || m.port > 21 || !std::isfinite(m.x) || !std::isfinite(m.y) || !std::isfinite(m.angle) ||
        !std::isfinite(m.scale) || m.scale <= 0)
      return false;
    for (unsigned j = 0; j < i; ++j)
      if (m.port && m.port == c.sensors[j].port)
        return false;
  }
  return true;
}
// One engine per robot: RW's underlying particle arrays are a singleton.
// Caller serializes access; no sensor or RTOS calls in this class.
class Engine {
 public:
  explicit Engine(const Config& config) : config_(config) {}
  Diagnostics diagnostics() const { return diag_; }
  void reset(const Sample& s) {
    reaper_rw::mclInit(config_.particles, s.pose.x, s.pose.y, 1.0);
    previous_ = s;
    history_.fill(s);
    history_count_ = 1;
    head_ = 0;
    last_reading_.fill(-1);
    moved_ = 0;
    motion_dx_ = motion_dy_ = heading_delta_ = 0;
    filter_ms_ = s.ms;
    stationary_ms_ = s.ms;
    have_measurement_ = false;
    diag_ = {};
    diag_.estimate = s.pose;
    diag_.ready = s.ready;
    initialized_ = true;
  }
  Correction update(const Sample& s, const std::array<double, 4>& distances) {
    if (!s.ready || !finite(s)) {
      initialized_ = false;
      diag_.ready = false;
      return {};
    }
    if (!initialized_ || s.epoch != previous_.epoch) {
      reset(s);
      return {};
    }
    const auto dt = s.ms - previous_.ms;
    if (dt == 0)
      return {};
    if (dt > 150) {
      reset(s);
      return {};
    }  // stale history after a paused task
    const double dh = std::remainder(s.pose.theta - previous_.pose.theta, 360.0);
    const double dx = s.motion_x - previous_.motion_x, dy = s.motion_y - previous_.motion_y;
    diag_.speed_ips = std::hypot(dx, dy) * 1000 / dt;
    diag_.turn_dps = std::abs(dh) * 1000 / dt;
    if (diag_.speed_ips > 2 || diag_.turn_dps > 10)
      stationary_ms_ = s.ms;
    motion_dx_ += dx;
    motion_dy_ += dy;
    heading_delta_ += dh;
    moved_ += std::hypot(dx, dy) + std::abs(dh) * 0.05;
    previous_ = s;
    head_ = (head_ + 1) % history_.size();
    history_[head_] = s;
    history_count_ = std::min(history_count_ + 1, history_.size());
    if (s.ms - filter_ms_ < 30)
      return {};
    filter_ms_ = s.ms;
    reaper_rw::mclPredict(motion_dx_, motion_dy_, heading_delta_);
    motion_dx_ = motion_dy_ = heading_delta_ = 0;
    const Sample* then = nullptr;
    for (std::size_t i = 0; i < history_count_; ++i) {
      const auto& h = history_[(head_ + history_.size() - i) % history_.size()];
      if (s.ms - h.ms >= config_.sensor_delay_ms) {
        then = &h;
        break;
      }
    }
    diag_.accepted = 0;
    diag_.blocked = 0;
    if (then) {
      const double sx = s.motion_x - then->motion_x, sy = s.motion_y - then->motion_y;
      reaper_rw::MclReading readings[4];
      int count = 0;
      for (unsigned i = 0; i < 4; ++i) {
        const auto& m = config_.sensors[i];
        const double d = distances[i] * m.scale;
        const bool repeat = d == last_reading_[i];
        last_reading_[i] = d;
        if (!m.port || repeat || !distance_valid(d))
          continue;
        // A mapped field element between the sensor and the wall: the sensor sees it, not the wall.
        if (beam_blocked(config_.field_map, config_.field_half_size, s.pose.x - sx, s.pose.y - sy,
                         then->pose.theta, m.x, m.y, m.angle)) {
          ++diag_.blocked;
          continue;
        }
        double unused = 0;
        if (!reaper_rw::distanceResetPosition(s.pose.x - sx, s.pose.y - sy, then->pose.theta, m.x, m.y,
                                              m.angle, d, config_.field_half_size, config_.max_angle,
                                              config_.corner_margin, unused))
          continue;
        readings[count++] = {m.x, m.y, m.angle, d};
      }
      if (count && moved_ > 0.25 && diag_.turn_dps < 200) {
        if (reaper_rw::mclCorrect(then->pose.theta, readings, count, config_.field_half_size,
                                  diag_.speed_ips * 0.01, sx, sy)) {
          diag_.accepted = count;
          have_measurement_ = true;
          measurement_ms_ = s.ms;
        }
        moved_ = 0;
      }
    }
    reaper_rw::mclEstimate(diag_.estimate.x, diag_.estimate.y, diag_.spread_x, diag_.spread_y);
    diag_.estimate.theta = s.pose.theta;
    if (!have_measurement_ || s.ms - measurement_ms_ > 300)
      return {};
    double x = s.pose.x, y = s.pose.y;
    reaper_rw::mclNudge(x, y, diag_.estimate.x, diag_.estimate.y, diag_.spread_x, diag_.spread_y);
    return {x - s.pose.x, y - s.pose.y, true};
  }
  // True if a mapped field element is between `sensor` and the wall it faces, at pose `s`.
  bool sensor_blocked(const Sample& s, Sensor sensor) const {
    const unsigned i = static_cast<unsigned>(sensor);
    if (i >= 4)
      return false;
    const auto& m = config_.sensors[i];
    return beam_blocked(config_.field_map, config_.field_half_size, s.pose.x, s.pose.y, s.pose.theta, m.x, m.y,
                        m.angle);
  }
  // Explicit DSR is only allowed after a stationary sensor-delay window.
  Correction distance_reset(const Sample& s, Sensor sensor, double raw_inches) const {
    unsigned i = static_cast<unsigned>(sensor);
    if (i >= 4 || !initialized_ || !s.ready || !finite(s) || s.epoch != previous_.epoch ||
        s.ms - previous_.ms > 30 || s.ms - stationary_ms_ < config_.sensor_delay_ms + 30 ||
        std::hypot(s.motion_x - previous_.motion_x, s.motion_y - previous_.motion_y) > 0.02 ||
        std::abs(std::remainder(s.pose.theta - previous_.pose.theta, 360.0)) > 0.1 || diag_.speed_ips > 2 ||
        diag_.turn_dps > 10)
      return {};
    const auto& m = config_.sensors[i];
    const double d = raw_inches * m.scale;
    if (!m.port || !distance_valid(d))
      return {};
    double value = 0;
    int axis = reaper_rw::distanceResetPosition(s.pose.x, s.pose.y, s.pose.theta, m.x, m.y, m.angle, d,
                                                config_.field_half_size, config_.max_angle,
                                                config_.corner_margin, value);
    double delta = value - (axis == 1 ? s.pose.x : s.pose.y);
    if (!axis || !std::isfinite(value) || std::abs(value) >= config_.field_half_size ||
        (config_.reset_max_jump > 0 && std::abs(delta) > config_.reset_max_jump))
      return {};
    return {axis == 1 ? delta : 0, axis == 2 ? delta : 0, true};
  }

 private:
  static bool finite(const Sample& s) {
    return std::isfinite(s.pose.x) && std::isfinite(s.pose.y) && std::isfinite(s.pose.theta) &&
           std::isfinite(s.motion_x) && std::isfinite(s.motion_y);
  }
  bool distance_valid(double d) const { return std::isfinite(d) && d >= 1.5 && d <= config_.max_range; }
  Config config_;
  Diagnostics diag_;
  Sample previous_;
  std::array<Sample, 20> history_{};
  std::size_t head_ = 0, history_count_ = 0;
  std::array<double, 4> last_reading_{};
  double moved_ = 0, motion_dx_ = 0, motion_dy_ = 0, heading_delta_ = 0;
  std::uint32_t filter_ms_ = 0, stationary_ms_ = 0;
  bool initialized_ = false, have_measurement_ = false;
  std::uint32_t measurement_ms_ = 0;
};
}  // namespace localization
