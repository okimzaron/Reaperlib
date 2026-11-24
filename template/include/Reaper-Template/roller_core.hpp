#pragma once
#include <cmath>
#include <cstdint>

// Pure colour logic for the roller: no PROS calls, so it can be reasoned about (and tested) on its own.
namespace roller {

// What the optical sensor can report. Only Red and Blue are valid targets; Yellow is recognised so the
// roller knows it is NOT on a target colour and keeps turning past it.
enum class Color { None, Red, Yellow, Blue };

inline const char* color_name(Color color) {
  switch (color) {
    case Color::Red:
      return "RED";
    case Color::Yellow:
      return "YELLOW";
    case Color::Blue:
      return "BLUE";
    default:
      return "NONE";
  }
}

// Hue windows are in degrees (0-360, red wraps around 0). Tune these on the real roller with the sensor at
// its mounted distance: watch the Home page / terminal readout while holding each colour in front of it.
struct Thresholds {
  double min_saturation = 0.35;  // Below this the colour is too washed out to trust (field tiles, metal).
  int min_proximity = 80;        // 0-255. Below this nothing is close enough to the sensor.
  double red_below = 20;         // Red is hue <= red_below ...
  double red_above = 340;        // ... or hue >= red_above.
  double yellow_min = 40, yellow_max = 75;
  double blue_min = 180, blue_max = 250;
};

inline bool thresholds_valid(const Thresholds& t) {
  auto hue_ok = [](double h) { return std::isfinite(h) && h >= 0 && h <= 360; };
  return std::isfinite(t.min_saturation) && t.min_saturation >= 0 && t.min_saturation <= 1 &&
         t.min_proximity >= 0 && t.min_proximity <= 255 && hue_ok(t.red_below) && hue_ok(t.red_above) &&
         hue_ok(t.yellow_min) && hue_ok(t.yellow_max) && hue_ok(t.blue_min) && hue_ok(t.blue_max) &&
         t.yellow_min <= t.yellow_max && t.blue_min <= t.blue_max;
}

// Classifies one optical reading. Anything ambiguous is None, never a guess.
inline Color classify(double hue, double saturation, int proximity, const Thresholds& t) {
  if (!std::isfinite(hue) || hue < 0 || hue >= 360 || !std::isfinite(saturation) ||
      saturation < t.min_saturation || saturation > 1 || proximity < t.min_proximity || proximity > 255)
    return Color::None;
  if (hue <= t.red_below || hue >= t.red_above)
    return Color::Red;
  if (hue >= t.yellow_min && hue <= t.yellow_max)
    return Color::Yellow;
  if (hue >= t.blue_min && hue <= t.blue_max)
    return Color::Blue;
  return Color::None;
}

// Debounces the raw classification: a colour must be seen continuously for `confirm_ms` before it becomes
// the stable colour. This stops a single noisy reading from stopping (or restarting) the roller.
class ColorFilter {
 public:
  void reset() {
    stable_ = Color::None;
    candidate_ = Color::None;
    have_candidate_ = false;
  }

  Color update(Color raw, std::uint32_t now, std::uint32_t confirm_ms) {
    if (raw == stable_) {
      have_candidate_ = false;
      return stable_;
    }
    if (!have_candidate_ || raw != candidate_) {
      candidate_ = raw;
      candidate_since_ = now;
      have_candidate_ = true;
    }
    if (static_cast<std::uint32_t>(now - candidate_since_) >= confirm_ms) {
      stable_ = candidate_;
      have_candidate_ = false;
    }
    return stable_;
  }

  Color stable() const { return stable_; }

 private:
  Color stable_ = Color::None;
  Color candidate_ = Color::None;
  bool have_candidate_ = false;
  std::uint32_t candidate_since_ = 0;
};

}  // namespace roller
