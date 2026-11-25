#pragma once

// [Reaper] Sequences: lift, claw, piston and wait steps that run in order in the background, so a
// route can keep driving while a mechanism does its thing. Guide: https://reaperlib.online/docs/#sequences
#include <cstdint>
#include <cstddef>
namespace lift {
enum class SequenceState { Idle, Running, Succeeded, Cancelled, TimedOut, Fault };
struct SequenceStatus {
  SequenceState state = SequenceState::Idle;
  std::size_t step = 0;
  std::uint32_t command_id = 0;
};
// Caller owns the actuators; this class never creates a competing motor task.
class SequenceClock {
 public:
  SequenceStatus status;
  void start(std::uint32_t id) {
    status = {SequenceState::Running, 0, id};
    entered_ = false;
  }
  bool enter(std::uint32_t now) {
    if (entered_)
      return false;
    entered_ = true;
    started_ = now;
    return true;
  }
  std::uint32_t elapsed(std::uint32_t now) const { return now - started_; }
  bool expired(std::uint32_t now, std::uint32_t timeout) const { return elapsed(now) >= timeout; }
  void advance(std::size_t count) {
    ++status.step;
    entered_ = false;
    if (status.step >= count)
      status.state = SequenceState::Succeeded;
  }
  void finish(SequenceState state) {
    status.state = state;
    entered_ = false;
  }

 private:
  bool entered_ = false;
  std::uint32_t started_ = 0;
};
}  // namespace lift
