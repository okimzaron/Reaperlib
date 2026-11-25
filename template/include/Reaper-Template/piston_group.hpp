#pragma once

// [Reaper] Piston groups: several ez::Piston that fire together as one. Guide: https://reaperlib.online/docs/#pistons
#include "piston.hpp"
#include "lock.hpp"
#include <vector>
#include <algorithm>
#include <initializer_list>
#include <cstdint>
namespace ez {
// Keep groups/static pistons alive for the lifetime of any sequence using them.
// Completion is an elapsed-time estimate, never a cylinder position measurement.
class PistonGroup {
 public:
  struct Status {
    std::uint32_t revision;
    bool settled;
    bool valid;
  };
  explicit PistonGroup(std::initializer_list<Piston*> pistons, std::uint32_t travel_ms = 250)
      : travel_(travel_ms) {
    for (auto* p : pistons)
      if (p && std::find(pistons_.begin(), pistons_.end(), p) == pistons_.end())
        pistons_.push_back(p);
  }
  std::uint32_t set(bool state) {
    KillSafeGuard<pros::Mutex> guard(lock_);
    snapshots_.clear();
    for (auto* p : pistons_) {
      p->set(state);
      snapshots_.push_back(p->revision());
    }
    started_ = pros::millis();
    ++revision_;
    return revision_;
  }
  Status status() {
    KillSafeGuard<pros::Mutex> guard(lock_);
    bool valid = !pistons_.empty() && snapshots_.size() == pistons_.size();
    if (valid)
      for (std::size_t i = 0; i < pistons_.size(); ++i)
        if (pistons_[i]->revision() != snapshots_[i] || !pistons_[i]->command_succeeded())
          valid = false;
    return {revision_, valid && pros::millis() - started_ >= travel_, valid};
  }
  std::uint32_t travel_ms() const { return travel_; }
  bool empty() const { return pistons_.empty(); }

 private:
  Lock<pros::Mutex> lock_;
  std::vector<Piston*> pistons_;
  std::vector<std::uint32_t> snapshots_;
  const std::uint32_t travel_;
  std::uint32_t started_ = 0, revision_ = 0;
};
}  // namespace ez
