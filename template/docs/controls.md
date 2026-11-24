# Driver controls

Every button is set in one file,
[`include/robot_controls.hpp`](../include/robot_controls.hpp), the same way ports
live in `robot_hardware.hpp`. Change a binding there, rebuild, upload. Nothing else
needs editing.

```cpp
static constexpr Button LIFT_UP = btn::R2;      // change to btn::UP, btn::X, ...
static constexpr Button ROLLER_TO_COLOR = btn::X;
```

- Button names: `btn::L1 L2 R1 R2 UP DOWN LEFT RIGHT X B Y A`, or `btn::NONE` to leave
  an action unbound.
- **The build fails if two driver actions share a button**, with the message
  `two driver actions are mapped to the same button`.
- Claw powers are set there too.

## Button map

| Input | Action |
| --- | --- |
| Left stick (up/down) | Drive forward / back (split arcade) |
| Right stick (left/right) | Turn |
| **R2** (hold) | Lift up at full power (slow until `max_height` is set). Releasing holds the height. |
| **L2** (hold) | Lift down at full power (slow until `max_height` is set). Releasing holds the height. |
| **Left** (press) | Lift returns to its start (bottom) position |
| **R1** (hold) | Claw at full power (127) |
| **L1** (hold) | Claw reverse at 70 power |
| **A** (hold) | Roller turns to the alliance color and holds it; release stops it |
| **Down + B** | Run / cancel the selected auton (only with no field controller connected) |

> **Known overlap:** EZ-Template's joystick curve buttons are on by default
> (`CURVE_BUTTONS_ENABLED` in `robot_controls.hpp`) and use **Left/Right** for the left
> stick curve and **Y/A** for the right. So **Left** (lift return) and **A** (roller) also change a curve, and the change is saved to the SD card. Curve buttons
> are allowed to overlap, so this doesn't fail the build; instead startup prints each
> overlap to the terminal, e.g. `[Controls] CURVE_LEFT_DECREASE shares button Left
> with LIFT_RETURN.` Set `CURVE_BUTTONS_ENABLED = false` or move them to fix it.

> **Warnings:** if [VMP](vmp.md) sees the robot tipping or the lift swaying, the
> controller rumbles once, the heading line shows `!TIPPING 18deg` or `!LIFT SWAY`,
> and the drive speed is capped until it clears (limits in `vmp_settings.hpp`).

## Stick feel

Two settings at the top of `robot_controls.hpp`:

| Setting | Default | What it does |
| --- | --- | --- |
| `DRIVE_DEADBAND` | `3` | Stick values below this count as centered, so a resting stick never creeps. |
| `DRIVE_MIN_OUTPUT` | `0` (off) | The smallest power a stick sends once it's past the deadband. A light push then goes straight to a power that actually moves the robot, instead of a few volts that only hum the motors. Full stick is still 127. |

To set `DRIVE_MIN_OUTPUT`: put the robot on the floor, start at `10`, and raise it a
little at a time until the lightest push just starts the robot rolling.

## Drive style

Set `DRIVE_STYLE` in `robot_controls.hpp`:

| `DriveStyle::` | Sticks |
| --- | --- |
| `SplitArcade` (default) | Left stick forward/back, right stick turn |
| `SingleArcade` | Left stick does both |
| `FlippedSplitArcade` | Right stick forward/back, left stick turn |
| `FlippedSingleArcade` | Right stick does both |
| `Tank` | Left stick left side, right stick right side |

The PID tuner's buttons are in the same file (`PID_TUNER_*`). They only matter while
the tuner is switched on.

## Controller screen

The controller shows three lines that update about three times a second:

```
X  12.0 Y -36.5      <- position on the field in inches (center is 0, 0)
H:90.0 deg           <- heading, clockwise
L:420.0 mdeg         <- lift position in motor degrees
```

What other readings mean:

| Shows | Meaning |
| --- | --- |
| `X:--- Y:---` / `H:--- deg` | Odometry isn't ready yet (still calibrating). |
| `XY: RANGE` / `H: RANGE` | The value is too big to fit. Something is wrong with the position. |
| `L: ports off` | Both lift ports are `0`. This is normal if you have no lift. |
| `L: check ports` | Only one lift port is set, both are the same, or one is above 21. |
| `L: left missing` / `L: right absent` | The port is set, but no motor answers on it. |
| `L: FAULT` | The lift stopped itself, for example from a sensor problem or the two lift motors disagreeing. See [Lift](lift.md#faults). |

Any other `L:` message is a lift startup check that failed. The full list is in
[Troubleshooting](troubleshooting.md#lift-startup-messages).

## Down + B: testing autons

With **no** field controller or competition switch connected:

1. Pick a route on the brain's **Routes** page.
2. Hold **Down** and press **B**. The auton runs, and joystick driving pauses.
3. Press **Down + B** again at any time to cancel. The drive stops and the lift
   holds where it is.

When the auton finishes, the drive brake mode goes back to what it was. If a field
controller gets connected while a test is running, the test is cancelled. Selecting
a route is locked while one is running.
