# Lift

Reaper's lift module runs the lift and claw from one background task, every 10 ms.
Your code only sends it commands ("go to 400", "hold here", "jog up"). It never
writes to the lift motors directly. Because there's only one task driving the lift,
two pieces of code can't fight over it.

Positions are in **motor degrees**: the average of the two lift motor encoders,
`(left - right) / 2`, measured from where the lift was when the program started.

## Files

| File | What to set |
| --- | --- |
| `include/robot_hardware.hpp` | `LIFT_LEFT_PORT`, `LIFT_RIGHT_PORT`, `CLAW_PORT`, optional `LIFT_ROTATION_PORT` |
| `include/Reaper-Template/lift_settings.hpp` | Travel limit, presets, PID, gravity compensation |
| `include/Reaper-Template/lift_core.hpp` | All settings and their defaults (`struct Config`). Read it, don't edit it. |

## Calibrating (do this first)

The lift ships **without** an upper limit, because the right height depends on your
robot. Until you set one, it refuses automatic moves above 0.

1. **Start low.** Every time the program starts, the lift's current position becomes
   zero. Always power on with the lift at its lowest point.
2. **Set your ports** in `robot_hardware.hpp`. The two motors face each other, so
   one is normally negative.
3. **Measure the travel.** `lift_settings.hpp` ships with
   `c.allow_unbounded_jog = true` so you can jog up with no limit. With the robot
   on a stand, hold **R2** to raise the lift to its safe top, and read the
   number on the controller's `L:` line. Until `max_height` is set, R2/L2 move the
   lift at the slow `up_rate`/`down_rate`, even with `full_power_manual_jog` on.
4. **Set the limit:**

   ```cpp
   c.max_height = 1150;           // your measured number
   c.allow_unbounded_jog = false; // turn calibration mode off
   ```

5. **Check the direction.** R2 must raise the lift and make `L:` count up. If it
   doesn't, flip the signs of both lift ports.

> Leave `allow_unbounded_jog = false` for normal use. With it on, nothing stops the
> lift at the top.

## Presets

Name the heights you use:

```cpp
inline std::vector<Preset> presets() {
  return {{"Start", 0}, {"Low", 320}, {"Score", 780}};   // measured values only
}
```

Every preset has to sit between 0 and `max_height`, or the lift refuses to start
(`L: preset`).

Using them in code:

```cpp
lift::move_to("Score");                // go to a preset
lift::move_to(500.0);                  // or a raw height
lift::wait_until_settled(4500);        // true = arrived; false = timed out or faulted
lift::next();  lift::previous();       // step to the next preset above/below
lift::set_preset("Mid", 550);          // add or change a preset after startup
lift::reached_preset();                // name of the preset it settled at, or ""
```

`move_to` returns `false` if the target is outside the travel limits. Nothing is
silently clamped.

## Driver control

`opcontrol()` calls:

```cpp
lift::driver_update(R2, L2, Left_new_press);
lift::claw_driver_update(L1 ? -70 : R1 ? 127 : 0);
```

- **R2 / L2:** full power while held (`c.full_power_manual_jog = true`), stopping at
  the limits. Before `max_height` is set they always use the slow jog. Releasing holds the current height. Set it to `false` for a smoother
  jog where the target creeps at `up_rate` / `down_rate` instead.
- **R2 and L2 together:** hold.
- **Left:** return to 0. If the lift failed its startup check, pressing **Left** (with
  R2/L2 released) retries the check and re-zeros. Make sure the lift is at the bottom
  first.
- **Claw:** always send power through `claw_driver_update()`. If it isn't called for
  100 ms, the claw stops on its own.

## Gravity compensation

If the lift sags or drifts down while holding, add a constant push upward:

```cpp
c.gravity = GravityModel::Constant;   // or Cosine for an arm that swings through an angle
c.kg = 12;                            // motor power units (-127..127), measured
```

- **Constant:** adds `kg` all the time. Good for a lift that carries the same load
  at every height (such as a DR4B or cascade).
- **Cosine:** `kg × cos(angle from horizontal)`. Good for a single arm. Set
  `c.horizontal` (the motor degrees where the arm is level) and
  `c.degrees_per_motor_degree` (arm degrees per motor degree, from your gearing).
- Gravity compensation is capped at `feedforward_fraction` (80% by default) of the
  maximum power.
- With gravity compensation on, the controller keeps running while holding instead
  of relying on motor brake mode alone.

## Gain scheduling

If one set of PID numbers is too soft at the bottom and too sharp at the top, give
it a table:

```cpp
c.gains = {{0, 0.30, 0, 0.02}, {900, 0.45, 0, 0.03}};   // {position, kp, ki, kd}
```

Gains blend smoothly between the points and hold the end values outside them.
Positions must be in increasing order.

## Main settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `kp`, `ki`, `kd` | 0.30, 0, 0.02 | PID gains |
| `max_height` | 0 | Upper travel limit. **You must set this.** |
| `max_up_power`, `max_down_power` | 100, 80 | Power caps |
| `return_power` | 60 | Down power cap when going to 0 |
| `tolerance` | 5 | How close counts as arrived, in degrees |
| `settle_ms` | 150 | How long it must stay there |
| `move_timeout_ms` | 4000 | A move gives up after this long |
| `slew_per_second` | 400 | How fast power can change |
| `encoder_disagreement` | 90 | Max difference between the two motors before a fault. `0` turns the check off. |
| `full_power_manual_jog` | true (in `lift_settings.hpp`) | R2/L2 at full power, once `max_height` is set |
| `allow_unbounded_jog` | true (in `lift_settings.hpp`) | Calibration mode. Set to false. |

Every setting is checked at startup. One bad value stops the lift from starting and
shows `L: settings`.

## Optional rotation sensor

To measure the lift with a rotation sensor instead of the motor encoders:

1. Set `LIFT_ROTATION_PORT` to an unused port.
2. Set `LIFT_MOTOR_DEGREES_PER_SENSOR_DEGREE` so one sensor degree converts to the
   matching motor degrees, with a negative sign if the sensor counts the other way.
3. Keep all your heights in motor degrees. Nothing else changes.

If the sensor stops answering, the lift faults instead of guessing.

## Faults

The lift stops itself (`L: FAULT`, motors braked) when:

- a lift motor or sensor stops answering,
- the two motors disagree by more than `encoder_disagreement` for 150 ms (for example
  a slipped gear or a stripped screw joint), or
- the settings turn out invalid.

To recover, fix the cause, then disable and re-enable the robot (`lift::enable()`
runs again when the next mode starts). The disagreement check can't catch both motors
stalling together, so keep an eye on motor temperatures.

## Status

```cpp
auto s = lift::get_status();
// s.mode (Disabled, Holding, Moving, Jogging, Fault), s.position, s.target,
// s.power, s.settled, s.timed_out, s.fault, s.velocity, s.encoder_difference
```

## API summary

| Function | Does |
| --- | --- |
| `initialize()` | Checks the hardware and starts the task. Called in `initialize()`. |
| `enable()` / `disable()` | Called by `autonomous()`, `opcontrol()` and `disabled()` |
| `move_to(height)` / `move_to("name")` | Go to a height or preset |
| `hold_current()` / `stop()` | Hold where it is now |
| `return_to_start()` | Go to 0 |
| `wait_until_settled(ms)` | Block until arrived; `false` on timeout or fault |
| `next()` / `previous()` | Next preset up or down |
| `set_preset(name, height)` | Add or replace a preset |
| `reached_preset()` | Preset it settled at, or empty |
| `get_status()` | Current status |
| `startup_message()` | `nullptr` if the startup check passed, otherwise the reason it failed |
| `configure(config, source)` | Advanced: replace the settings or position source. Call before `initialize()`. |

For lift + claw + piston combos, see [sequences](mechanisms.md#sequences).
