#pragma once
#include <cstdint>

namespace ez::detail {
// LVGL supplies gesture recognition; this only gates completed clicks and
// rejects short contact bounce. Unsigned elapsed time handles tick rollover.
class TouchGate {
 public:
  void press(const void* target, std::uint32_t now) {
    pressed_ = true;
    armed_ = (!have_click_ || now - last_click_ >= 120) ? target : nullptr;
    contact_ = target;
  }
  void release(const void* target) {
    if (contact_ == target) pressed_ = false;
  }
  void cancel(const void* target) {
    if (contact_ == target) {
      pressed_ = false;
      armed_ = contact_ = nullptr;
    }
  }
  bool click(const void* target, std::uint32_t now) {
    if (!target || pressed_ || armed_ != target) return false;
    armed_ = contact_ = nullptr;
    have_click_ = true;
    last_click_ = now;
    return true;
  }
  bool pressed() const { return pressed_; }
 private:
  const void* armed_ = nullptr;
  const void* contact_ = nullptr;
  std::uint32_t last_click_ = 0;
  bool pressed_ = false, have_click_ = false;
};
}
