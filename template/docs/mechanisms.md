# Mechanisms

## Claw

Set `CLAW_PORT` in `robot_hardware.hpp`. The claw belongs to the lift task, so:

- In driver control, send power with `lift::claw_driver_update(power)`, every loop.
  Send `0` when no button is held. If no update arrives for 100 ms, the claw stops.
- In autons, use a [sequence](#sequences) for timed claw moves.
- Don't call `claw.move()` yourself. It bypasses the lift task and can fight a
  running sequence.

## Pistons

`ez::Piston` (from EZ-Template) drives one solenoid:

```cpp
ez::Piston clamp('A');                 // brain 3-wire port A
ez::Piston wing('B', 5);               // port B on a 3-wire expander in smart port 5
ez::Piston hang('C', true);            // starts with the pin high

clamp.set(true);                       // extend (relative to its start state)
clamp.get();                           // last commanded state
```

To drive a piston from the controller, give it a button in `robot_controls.hpp`
first, and add it to `DRIVER_ACTIONS` there so the duplicate check covers it:

```cpp
// robot_controls.hpp, SETTINGS section
static constexpr Button CLAMP_TOGGLE = btn::Y;

// src/main.cpp, inside the opcontrol loop
clamp.button_toggle(C::held(C::CLAMP_TOGGLE));   // press to toggle
```

(If you add to `DRIVER_ACTIONS`, add the matching name to `DRIVER_ACTION_NAMES` in
`robot_controls.cpp`; the build tells you if the two lists get out of step.)

Declare pistons as globals (outside any function), so they live for the whole program.

## Piston groups

`ez::PistonGroup` fires several pistons together and estimates when they've
finished moving:

```cpp
#include "Reaper-Template/piston_group.hpp"

ez::Piston clamp_left('A');
ez::Piston clamp_right('B');
ez::PistonGroup clamp{{&clamp_left, &clamp_right}, 250};   // 250 ms to travel

clamp.set(true);
clamp.status().settled;   // true once 250 ms have passed since the command
```

"Settled" is only a timer. The cylinder has no sensor, so time your real stroke
and use that number. If something else changes a member piston while a group
command is running, the group notices and stops counting it as done.

## Sequences

A sequence runs lift, claw, piston and wait steps **in order, in the background**.
While it runs it owns the lift and claw, so nothing else drives them at the same time.

```cpp
using lift::StepType;
std::vector<lift::SequenceStep> score = {
  //  type,               value, duration_ms, timeout_ms, pistons
    {StepType::Claw,       127,   300,         500},              // grab: 127 power for 300 ms
    {StepType::Lift,       780,   0,           4500},             // raise to 780 (measured)
    {StepType::Pneumatic,  1,     0,           1000, &clamp},     // fire the group
    {StepType::Wait,       0,     200,         400},              // pause 200 ms
    {StepType::Claw,      -70,    250,         500},              // release
    {StepType::Lift,       0,     0,           4500},             // back down
};

if (!lift::start_sequence(score)) return;   // false = rejected (bad step)
// ... the drive can move while this runs ...
if (!lift::wait_sequence(15000)) return;    // false = cancelled, timed out or faulted
```

### Step rules

| Type | `value` | `duration_ms` | Done when |
| --- | --- | --- | --- |
| `Lift` | Target height (motor degrees, within limits) | unused | The lift settles |
| `Claw` | Power, -127 to 127 | Required, and less than the timeout | The time is up |
| `Pneumatic` | `1` = set, `0` = unset | unused | The group's travel time has passed |
| `Wait` | unused | Required, and less than the timeout | The time is up |

- Every step needs a `timeout_ms`. A `Lift` step also stops at the lift's own
  `move_timeout_ms`, whichever comes first.
- At most 64 steps. The whole sequence is checked before it starts. One bad step
  rejects all of it, and anything already running keeps going.
- Claw steps are timed. They don't confirm that you actually grabbed anything.

### What cancels a sequence

- Driver input on the lift (R2/L2/Left) or the claw
- `lift::hold_current()`, `lift::stop()`, `lift::cancel_sequence()`, a new
  `move_to()` or sequence, or `lift::disable()` (which runs when the robot is disabled)

When a sequence ends early, the claw stops and the lift holds where it is. **Pistons
stay in their last state**, because a cylinder can't be stopped halfway.

Check how it ended:

```cpp
auto s = lift::sequence_status();
// s.state: Idle, Running, Succeeded, Cancelled, TimedOut, Fault
// s.step:  which step it's on
```

## Roller

A color roller with its **own** motor and optical sensor, run by its own background
task. It is independent of the intake and every other mechanism, and every roller
function is safe to call from anywhere: routes, `opcontrol()`, or your own tasks.
Calls only post a request; the roller task is the only code that moves the motor.

The sensor recognises three colors: **red**, **yellow** and **blue**. The roller turns
until your alliance color is in front of the sensor. It never stops on yellow or on
the other alliance's color.

### Setup

1. In `robot_hardware.hpp` set `ROLLER_MOTOR_PORT` (negative reverses it),
   `ROLLER_OPTICAL_PORT` and `ROLLER_POWER` (seek speed, 1-127).
2. Pick **RED** or **BLUE** on the brain's Routes page (or call
   `roller::target_set(roller::Color::Blue)`).
3. **Check the colors.** Open the brain's **Motors** page: its second line becomes a
   live readout, e.g. `Optical: YELLOW  h52 s0.61 p140  [IDLE]` (color, hue,
   saturation, proximity, mode). Hold each roller color in front of the sensor and
   check it says RED, YELLOW and BLUE. The terminal also prints
   `[Roller] Sees ...` every time the color changes. You can fit just the optical
   sensor (motor port `0`) to do this before the motor is mounted.
4. If a color reads wrong, adjust the hue windows in
   `include/Reaper-Template/roller_settings.hpp`. Defaults: red hue ≤ 20 or ≥ 340,
   yellow 40-75, blue 180-250, saturation ≥ 0.35, proximity ≥ 80.

### Driver control

**Hold X**: roll to the alliance color and keep it there. **Release X**: stop. The
driver helper only acts when X is pressed or released, so it never overrides roller
commands from elsewhere while X is up.

### From code

```cpp
roller::roll_to_target(1500);   // blocking: turn until alliance color, max 1.5 s. True if reached.

roller::seek(1500);             // non-blocking version...
// ...drive somewhere...
roller::wait();                 // ...then wait for it. True if reached.

roller::maintain();             // keep defending the color until stop()/spin()/seek()
roller::maintain_for(1500);     // same, blocking, for 1.5 s, then stop
roller::spin(-60);              // plain power, no color logic
roller::stop();

auto s = roller::status();      // s.seen, s.on_target, s.hue, s.result ...
```

If the optical sensor stops responding, a seek ends with `Result::Fault` and
`maintain()` stands still until the sensor is back. The roller also stops whenever
the field disables the robot.
