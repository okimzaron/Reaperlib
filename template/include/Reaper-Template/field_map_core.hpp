#pragma once

// [Reaper, idea from RCL-Tracking] Field map geometry with no VEX calls: does a sensor's beam pass a
// goal or barrier before it reaches the wall? Your obstacles go in include/field_map.hpp.
#include <algorithm>
#include <cmath>
#include <vector>

// Field obstacles for localization: pure geometry, no PROS calls.
//
// A distance sensor aimed at a wall is useless if a field element (a goal, a loader, a barrier) sits between
// it and the wall: it reads the element, not the wall. With a map of those elements, localization skips any
// sensor whose beam would pass near one before reaching the wall.
//
// Coordinates match the rest of Reaper: inches, field center (0, 0), +X right, +Y forward, headings clockwise
// from +Y.
namespace localization {

struct Circle {  // a post, goal leg, round goal...
  double x, y, r;
};
struct Segment {  // a straight barrier, the long edge of a goal...
  double x1, y1, x2, y2;
};

struct FieldMap {
  std::vector<Circle> circles;
  std::vector<Segment> segments;
  // Extra clearance, inches, added around every obstacle: covers position error and the beam's width.
  double margin = 2.0;
};

inline bool field_map_valid(const FieldMap& map) {
  if (!std::isfinite(map.margin) || map.margin < 0 || map.margin > 24)
    return false;
  for (const auto& c : map.circles)
    if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.r) || c.r < 0)
      return false;
  for (const auto& s : map.segments)
    if (!std::isfinite(s.x1) || !std::isfinite(s.y1) || !std::isfinite(s.x2) || !std::isfinite(s.y2))
      return false;
  return true;
}

namespace detail {
// Shortest distance between segments P0-P1 and Q0-Q1 (Ericson, "Real-Time Collision Detection" 5.1.9).
inline double segment_distance(double p0x, double p0y, double p1x, double p1y, double q0x, double q0y, double q1x,
                               double q1y) {
  const double d1x = p1x - p0x, d1y = p1y - p0y, d2x = q1x - q0x, d2y = q1y - q0y;
  const double rx = p0x - q0x, ry = p0y - q0y;
  const double a = d1x * d1x + d1y * d1y, e = d2x * d2x + d2y * d2y, f = d2x * rx + d2y * ry;
  constexpr double eps = 1e-12;
  double s = 0, t = 0;
  if (a <= eps && e <= eps) {
    s = t = 0;
  } else if (a <= eps) {
    s = 0;
    t = std::clamp(f / e, 0.0, 1.0);
  } else {
    const double c = d1x * rx + d1y * ry;
    if (e <= eps) {
      t = 0;
      s = std::clamp(-c / a, 0.0, 1.0);
    } else {
      const double b = d1x * d2x + d1y * d2y, denom = a * e - b * b;
      s = denom > eps ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
      t = (b * s + f) / e;
      if (t < 0) {
        t = 0;
        s = std::clamp(-c / a, 0.0, 1.0);
      } else if (t > 1) {
        t = 1;
        s = std::clamp((b - c) / a, 0.0, 1.0);
      }
    }
  }
  const double dx = (p0x + d1x * s) - (q0x + d2x * t), dy = (p0y + d1y * s) - (q0y + d2y * t);
  return std::hypot(dx, dy);
}
}  // namespace detail

// True if the beam of a sensor mounted at (mount_x, mount_y) on the robot, facing mount_angle (0 = front,
// 90 = right), passes within an obstacle's radius + map.margin before it reaches the field wall.
inline bool beam_blocked(const FieldMap& map, double field_half_size, double robot_x, double robot_y,
                         double heading_deg, double mount_x, double mount_y, double mount_angle) {
  if (map.circles.empty() && map.segments.empty())
    return false;
  constexpr double PI = 3.141592653589793;
  const double h = heading_deg * PI / 180.0;
  // Sensor position on the field (robot frame +x right, +y forward, rotated clockwise by the heading).
  const double sx = robot_x + mount_x * std::cos(h) + mount_y * std::sin(h);
  const double sy = robot_y - mount_x * std::sin(h) + mount_y * std::cos(h);
  const double b = (heading_deg + mount_angle) * PI / 180.0;
  const double bx = std::sin(b), by = std::cos(b);

  // Beam length to the nearest wall.
  const double tx = std::fabs(bx) > 1e-9 ? ((bx > 0 ? field_half_size : -field_half_size) - sx) / bx : 1e9;
  const double ty = std::fabs(by) > 1e-9 ? ((by > 0 ? field_half_size : -field_half_size) - sy) / by : 1e9;
  const double length = std::max(0.0, std::min(tx, ty));
  const double ex = sx + bx * length, ey = sy + by * length;

  for (const auto& c : map.circles)
    if (detail::segment_distance(sx, sy, ex, ey, c.x, c.y, c.x, c.y) <= c.r + map.margin)
      return true;
  for (const auto& s : map.segments)
    if (detail::segment_distance(sx, sy, ex, ey, s.x1, s.y1, s.x2, s.y2) <= map.margin)
      return true;
  return false;
}

}  // namespace localization
