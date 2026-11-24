#pragma once
#include "lift_core.hpp"
#include "sequence_core.hpp"
#include <functional>
namespace ez {
class PistonGroup;
}
namespace lift {
using PositionSource = std::function<double()>;  // Absolute MOTOR-equivalent degrees; NaN on failure.
// Startup only, before initialize(). External feedback is zeroed in software at initialize().
bool configure(const Config&, PositionSource source = {});
bool initialize();              // Physically place the lift at its bottom first.
const char* startup_message();  // Null when ready, otherwise the failed startup check.
void enable();                  // Explicitly recovers a fault only when feedback is valid.
void disable();
void hold_current();
void stop();  // Cancels sequence/movement; holds current height.
bool move_to(double motor_degrees);
void return_to_start();
void driver_update(bool up, bool down, bool return_pressed);
void claw_driver_update(int power);  // Replaces direct claw.move() in opcontrol; heartbeat protected.
Status get_status();
bool wait_until_settled(std::uint32_t timeout_ms = 4500);
bool set_preset(const std::string&, double motor_degrees);
bool move_to(const std::string&);
bool next();
bool previous();
std::string reached_preset();  // Empty unless settled successfully, within tolerance.
enum class StepType { Lift, Claw, Pneumatic, Wait };
struct SequenceStep {
  StepType type;
  double value = 0;                 // Lift motor degrees, claw power, or pneumatic bool (0/1).
  std::uint32_t duration_ms = 0;    // Timed claw/wait steps.
  std::uint32_t timeout_ms = 4000;  // Required finite deadline for every step.
  ez::PistonGroup* pistons = nullptr;
};
// A sequence exclusively owns lift + claw until complete/cancelled. Pistons retain
// their commanded state on cancel; a cylinder cannot be stopped halfway in software.
bool start_sequence(const std::vector<SequenceStep>&);
void cancel_sequence();
SequenceStatus sequence_status();
bool wait_sequence(std::uint32_t timeout_ms = 15000);
}  // namespace lift
