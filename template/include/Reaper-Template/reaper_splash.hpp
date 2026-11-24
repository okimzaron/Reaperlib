#pragma once
#include <cstdint>

// Boot overlay on LVGL's top layer. Prepare the dashboard first, then start
// before sensor calibration. The full animation runs on a 3000 ms timeline:
// swing 1400, windup 160, strike 90, crack 350, falling/fading shards 1000.
// Cleanup occurs on the first display timer callback at/after the deadline.
// This visual deadline never cancels or shortens sensor calibration.
namespace reaper_splash {
/// Shows the splash. Repeated calls are ignored. Startup wait is bounded.
void start();
/// Compatibility notification; returns immediately. The animation completes
/// on its own deadline even if initialization finishes late or never calls this.
void finish();
/// Legacy name: now sets TOTAL animation duration, including the finale.
/// Default 3000 ms; clamped to at least 1620 ms. Call before start().
void min_duration_set(uint32_t ms);
/// True while the overlay is queued or visible.
bool active();
}  // namespace reaper_splash
