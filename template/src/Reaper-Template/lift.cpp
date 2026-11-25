// [Reaper] Lift controller task: reads the lift motors (or rotation sensor) and runs lift_core.hpp.

#include "main.h"
#include "Reaper-Template/lift_settings.hpp"
#include "Reaper-Template/piston_group.hpp"
#include <memory>
#include <atomic>

namespace lift {
namespace {
Config config = settings::config();
PositionSource position_source;
double external_zero = 0, left_zero = 0, right_zero = 0;
std::atomic<const char*> startup_error{"L: starting"};
std::unique_ptr<pros::Rotation> rotation;
Presets presets;
enum class Action { Disable, Enable, Hold, Move, Jog, Sequence };
struct Command {
  Action action = Action::Disable;
  double value = 0;
  std::uint32_t sequence = 0, enable_epoch = 0;
  bool enabled = false;
  std::shared_ptr<const std::vector<SequenceStep>> steps;
};
Command command;
Status published;
SequenceStatus published_sequence;
std::uint32_t applied_sequence = 0;
ez::Lock<pros::Mutex> mailbox;
pros::task_t worker = nullptr;
int previous_driver_direction = 0;
bool previous_driver_both = false;
int manual_claw = 0;
std::uint32_t claw_heartbeat = 0;

// All actuator writes are owned by run(); public commands only replace mailbox data.
bool post_locked(Action action, double value = 0,
                 std::shared_ptr<const std::vector<SequenceStep>> steps = {}) {
  if (!worker)
    return false;
  bool enabled = action == Action::Enable ? true : action == Action::Disable ? false : command.enabled;
  if (!enabled && action != Action::Disable)
    return false;
  if (action == Action::Disable || action == Action::Enable) {
    manual_claw = 0;
    claw_heartbeat = 0;
  }
  command = {
      action,  value, command.sequence + 1, command.enable_epoch + (action == Action::Enable ? 1u : 0u),
      enabled, steps};
  return true;
}
bool post(Action action, double value = 0) {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  return post_locked(action, value);
}

void run(void*) {
  ez::detail::mark_scheduler_running();
  Controller controller(config);
  FeedbackGuard feedback;
  SequenceClock sequence;
  std::shared_ptr<const std::vector<SequenceStep>> steps;
  std::uint32_t seen = 0, enable_seen = 0, previous_time = pros::millis() - 10, last_jog = 0,
                piston_revision = 0;
  int claw_power = 0;
  bool braked = false;
  auto finish = [&](SequenceState state, double height) {
    sequence.finish(state);
    claw_power = 0;
    if (controller.status.mode != Mode::Fault && controller.status.mode != Mode::Disabled &&
        std::isfinite(height))
      controller.hold(height);
    if (state == SequenceState::TimedOut)
      controller.status.timed_out = true;
  };
  while (true) {
    const auto now = pros::millis();
    double dt = (now - previous_time) / 1000.0;
    previous_time = now;
    const double left = LiftL.get_position() - left_zero, right = LiftR.get_position() - right_zero;
    bool valid = feedback.check(left, right, LiftL.is_installed() && LiftR.is_installed(), config, now);
    // Direction is fixed once, by the port signs in robot_hardware.hpp (the two motors face
    // each other, so one port is negative). After that both encoders count up as the lift rises.
    double height = (left + right) / 2;
    Fault reason = feedback.fault;
    if (position_source) {
      const double raw = position_source();
      if (!std::isfinite(raw) || std::abs(raw) >= 1e8) {
        valid = false;
        reason = Fault::Sensor;
      }
      height = raw - external_zero;
    }
    Command pending;
    int driver_claw;
    std::uint32_t driver_heartbeat;
    {
      ez::KillSafeGuard<pros::Mutex> guard(mailbox);
      pending = command;
      driver_claw = manual_claw;
      driver_heartbeat = claw_heartbeat;
    }
    const bool field_disabled = pros::competition::is_connected() && pros::competition::is_disabled();
    if (!valid)
      controller.fault(reason);
    if (field_disabled)
      controller.disable();
    if (pending.enable_epoch != enable_seen) {
      enable_seen = pending.enable_epoch;
      if (valid && !field_disabled && pending.enabled)
        controller.hold(height);
    }
    if (pending.sequence != seen) {
      seen = pending.sequence;
      braked = false;
      if (sequence.status.state == SequenceState::Running)
        finish(SequenceState::Cancelled, height);
      claw_power = 0;
      if (pending.action == Action::Disable)
        controller.disable();
      else if (valid && !field_disabled && pending.enabled && controller.status.mode != Mode::Disabled &&
               controller.status.mode != Mode::Fault) {
        switch (pending.action) {
          case Action::Enable:
            controller.hold(height);
            break;
          case Action::Hold:
            controller.hold(height);
            break;
          case Action::Move:
            controller.move(pending.value, height, now);
            break;
          case Action::Jog:
            controller.jog(static_cast<int>(pending.value), height);
            last_jog = now;
            break;
          case Action::Sequence:
            steps = pending.steps;
            controller.hold(height);
            sequence.start(seen);
            break;
          default:
            break;
        }
      } else if (pending.action == Action::Sequence) {
        sequence.start(seen);
        sequence.finish(SequenceState::Fault);
      }
    }
    if (sequence.status.state == SequenceState::Running &&
        (controller.status.mode == Mode::Fault || controller.status.mode == Mode::Disabled))
      finish(controller.status.mode == Mode::Fault ? SequenceState::Fault : SequenceState::Cancelled, height);
    if (valid && controller.status.mode == Mode::Jogging && now - last_jog > 100)
      controller.hold(height);
    if (sequence.status.state == SequenceState::Running) {
      const auto& step = (*steps)[sequence.status.step];
      if (sequence.enter(now)) {
        switch (step.type) {
          case StepType::Lift:
            if (!controller.move(step.value, height, now))
              finish(SequenceState::Fault, height);
            break;
          case StepType::Claw:
            if (!claw.is_installed())
              finish(SequenceState::Fault, height);
            else
              claw_power = static_cast<int>(step.value);
            break;
          case StepType::Pneumatic:
            piston_revision = step.pistons->set(step.value != 0);
            break;
          case StepType::Wait:
            break;
        }
      }
    }
    if (valid)
      controller.step(height, dt, now);
    if (sequence.status.state == SequenceState::Running) {
      const auto& step = (*steps)[sequence.status.step];
      bool done = false;
      if (controller.status.mode == Mode::Fault)
        finish(SequenceState::Fault, height);
      else if (controller.status.timed_out || sequence.expired(now, step.timeout_ms))
        finish(SequenceState::TimedOut, height);
      else {
        switch (step.type) {
          case StepType::Lift:
            done = controller.status.settled;
            break;
          case StepType::Claw:
            done = sequence.elapsed(now) >= step.duration_ms;
            break;
          case StepType::Wait:
            done = sequence.elapsed(now) >= step.duration_ms;
            break;
          case StepType::Pneumatic: {
            const auto state = step.pistons->status();
            if (!state.valid || state.revision != piston_revision)
              finish(SequenceState::Cancelled, height);
            else
              done = state.settled;
            break;
          }
        }
        if (done && sequence.status.state == SequenceState::Running) {
          if (step.type == StepType::Claw)
            claw_power = 0;
          sequence.advance(steps->size());
        }
      }
    }
    // A sequence failure can change the controller after step(); honor that reset now.
    const double power = controller.status.power;
    if (power == 0) {
      if (!braked) {
        LiftL.brake();
        LiftR.brake();
        braked = true;
      }
    } else {
      int motor = static_cast<int>(std::lround(power));
      LiftL.move(motor);
      LiftR.move(motor);
      braked = false;
    }
    if (sequence.status.state != SequenceState::Running)
      claw_power = (pending.enabled && !field_disabled && controller.status.mode != Mode::Fault &&
                    controller.status.mode != Mode::Disabled && now - driver_heartbeat <= 100)
                       ? driver_claw
                       : 0;
    if (sequence.status.state == SequenceState::Running &&
        (*steps)[sequence.status.step].type == StepType::Claw && !claw.is_installed())
      finish(SequenceState::Fault, height);
    if (RobotHardware::CLAW_PORT)
      claw.move(claw_power);
    controller.status.encoder_difference = std::abs(left - right);
    controller.status.command_id = seen;
    {
      ez::KillSafeGuard<pros::Mutex> guard(mailbox);
      published = controller.status;
      applied_sequence = seen;
      published_sequence = sequence.status;
    }
    pros::delay(10);
  }
}
}  // namespace

bool configure(const Config& value, PositionSource source) {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  if (worker || !valid_config(value))
    return false;
  config = value;
  position_source = std::move(source);
  return true;
}
const char* startup_message() {
  return startup_error.load();
}

namespace {
bool fail_start(const char* message) {
  startup_error.store(message);
  printf("[Lift] %s (configured motor ports %d, %d)\n", message, RobotHardware::LIFT_LEFT_PORT,
         RobotHardware::LIFT_RIGHT_PORT);
  return false;
}

const char* prepare_motor(pros::Motor& motor, double& zero, bool left) {
  if (pros::c::get_plugged_type(std::abs(motor.get_port())) != pros::c::E_DEVICE_MOTOR)
    return left ? "L: left missing" : "L: right absent";
  if (motor.set_encoder_units(pros::E_MOTOR_ENCODER_DEGREES) != 1)
    return left ? "L: left units" : "L: right units";
  if (motor.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD) != 1 || motor.brake() != 1)
    return left ? "L: left brake" : "L: right brake";
  zero = motor.get_position();
  if (!std::isfinite(zero) || zero == PROS_ERR)
    return left ? "L: left encoder" : "L: right enc";
  return nullptr;
}

bool initialize_with_attempts(int attempts) {
  if (worker)
    return true;
  const int left = std::abs(RobotHardware::LIFT_LEFT_PORT);
  const int right = std::abs(RobotHardware::LIFT_RIGHT_PORT);
  if (!left && !right)
    return fail_start("L: ports off");
  if (!left || !right || left > 21 || right > 21 || left == right)
    return fail_start("L: check ports");
  if (!valid_config(config))
    return fail_start("L: settings");
  for (const auto& p : settings::presets())
    if (!presets.set(p.name, p.position, config))
      return fail_start("L: preset");
  if (!position_source && RobotHardware::LIFT_ROTATION_PORT != 0) {
    const int port = RobotHardware::LIFT_ROTATION_PORT;
    const double scale = RobotHardware::LIFT_MOTOR_DEGREES_PER_SENSOR_DEGREE;
    if (!RobotHardware::lift_rotation_port_available() || !std::isfinite(scale) || scale == 0)
      return fail_start("L: rot config");
    rotation = std::make_unique<pros::Rotation>(port);
    position_source = [scale]() {
      const auto value = rotation->get_position();
      return value == PROS_ERR ? NAN : static_cast<double>(value) / 100.0 * scale;
    };
  }
  // Capture encoder offsets at the bottom. No hardware tare is required.
  const char* error = nullptr;
  for (int attempt = 0; attempt < attempts; ++attempt) {
    error = prepare_motor(LiftL, left_zero, true);
    if (!error)
      error = prepare_motor(LiftR, right_zero, false);
    if (!error)
      break;
    if (attempt + 1 < attempts)
      pros::delay(50);
  }
  if (error)
    return fail_start(error);
  if (position_source) {
    external_zero = position_source();
    if (!std::isfinite(external_zero) || std::abs(external_zero) >= 1e8)
      return fail_start("L: rot sensor");
  }
  if (RobotHardware::CLAW_PORT)
    claw.move(0);
  worker = pros::c::task_create(run, nullptr, TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT,
                                "Reaper Mechanisms");
  if (!worker)
    return fail_start("L: task failed");
  startup_error.store(nullptr);
  return true;
}
}  // namespace

bool initialize() {
  return initialize_with_attempts(10);
}
void enable() {
  previous_driver_direction = 0;
  previous_driver_both = false;
  post(Action::Enable);
}
void disable() {
  post(Action::Disable);
}
void hold_current() {
  post(Action::Hold);
}
void stop() {
  hold_current();
}
bool move_to(double target) {
  return valid_target(config, target) && post(Action::Move, target);
}
void return_to_start() {
  move_to(0.0);
}
void driver_update(bool up, bool down, bool return_pressed) {
  if (startup_message()) {
    // Deliberate re-zero only, with both jog buttons released.
    if (return_pressed && !up && !down && initialize_with_attempts(1))
      enable();
    return;
  }
  int direction = up == down ? 0 : (up ? 1 : -1);
  bool both = up && down;
  if (both) {
    if (!previous_driver_both)
      hold_current();
  } else if (direction != 0)
    post(Action::Jog, direction);
  else if (return_pressed)
    return_to_start();
  else if (previous_driver_direction != 0)
    hold_current();
  previous_driver_direction = direction;
  previous_driver_both = both;
}
void claw_driver_update(int power) {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  manual_claw = std::clamp(power, -127, 127);
  claw_heartbeat = pros::millis();
  if (power != 0 && command.action == Action::Sequence)
    post_locked(Action::Hold);
}
Status get_status() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  return published;
}
bool wait_until_settled(std::uint32_t timeout) {
  std::uint32_t expected;
  {
    ez::KillSafeGuard<pros::Mutex> guard(mailbox);
    if (!worker || command.action == Action::Sequence)
      return false;
    expected = command.sequence;
  }
  auto start = pros::millis();
  while (true) {
    {
      ez::KillSafeGuard<pros::Mutex> guard(mailbox);
      if (command.sequence != expected)
        return false;
      if (applied_sequence == expected) {
        if (published.mode == Mode::Disabled || published.mode == Mode::Fault || published.timed_out)
          return false;
        if (published.settled)
          return true;
      }
      if (pros::millis() - start >= timeout) {
        post_locked(Action::Hold);
        return false;
      }
    }
    pros::delay(10);
  }
}
bool set_preset(const std::string& name, double value) {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  return presets.set(name, value, config);
}
bool move_to(const std::string& name) {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  double value;
  return presets.find(name, value) && post_locked(Action::Move, value);
}
bool next() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  double value;
  return applied_sequence == command.sequence &&
         presets.step(published.position, config.tolerance, true, value) && post_locked(Action::Move, value);
}
bool previous() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  double value;
  return applied_sequence == command.sequence &&
         presets.step(published.position, config.tolerance, false, value) && post_locked(Action::Move, value);
}
std::string reached_preset() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  return applied_sequence == command.sequence && published.settled && !published.timed_out &&
                 published.mode == Mode::Holding
             ? presets.nearest(published.position, config.tolerance)
             : std::string{};
}
bool start_sequence(const std::vector<SequenceStep>& steps) {
  if (steps.empty() || steps.size() > 64)
    return false;
  for (const auto& s : steps) {
    if (s.timeout_ms == 0 || !std::isfinite(s.value))
      return false;
    switch (s.type) {
      case StepType::Lift:
        if (!valid_target(config, s.value))
          return false;
        break;
      case StepType::Claw:
        if (std::abs(s.value) > 127 || s.duration_ms == 0 || s.duration_ms >= s.timeout_ms)
          return false;
        break;
      case StepType::Wait:
        if (s.duration_ms >= s.timeout_ms)
          return false;
        break;
      case StepType::Pneumatic:
        if (!s.pistons || s.pistons->empty() || s.pistons->travel_ms() >= s.timeout_ms ||
            (s.value != 0 && s.value != 1))
          return false;
        break;
      default:
        return false;
    }
  }
  auto copy = std::make_shared<const std::vector<SequenceStep>>(steps);
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  if (manual_claw != 0 && pros::millis() - claw_heartbeat <= 100)
    return false;
  return post_locked(Action::Sequence, 0, copy);
}
void cancel_sequence() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  if (command.action == Action::Sequence)
    post_locked(Action::Hold);
}
SequenceStatus sequence_status() {
  ez::KillSafeGuard<pros::Mutex> guard(mailbox);
  return published_sequence;
}
bool wait_sequence(std::uint32_t timeout) {
  std::uint32_t expected;
  {
    ez::KillSafeGuard<pros::Mutex> guard(mailbox);
    if (command.action != Action::Sequence)
      return false;
    expected = command.sequence;
  }
  auto start = pros::millis();
  while (true) {
    {
      ez::KillSafeGuard<pros::Mutex> guard(mailbox);
      if (command.sequence != expected)
        return false;
      if (published_sequence.command_id == expected) {
        if (published_sequence.state == SequenceState::Succeeded)
          return true;
        if (published_sequence.state != SequenceState::Running)
          return false;
      }
      if (pros::millis() - start >= timeout) {
        post_locked(Action::Hold);
        return false;
      }
    }
    pros::delay(10);
  }
}
}  // namespace lift
