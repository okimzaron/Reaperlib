# Hardware configuration

All wiring and sensor positions live in
[`include/robot_hardware.hpp`](../include/robot_hardware.hpp), between the
`SETTINGS` and `END SETTINGS` markers. Edit that section, rebuild and upload.

## Rules that apply to every port

- **`0` means disabled.** The device isn't created and nothing tries to talk to it.
- **Smart ports are 1 to 21.**
- **A negative motor port reverses that motor.** Fix direction here, not by negating
  power in your code. Doing both cancels out.
- **Never use one port for two devices.** The Motors page marks a port used twice
  as `USED`, and the optional lift rotation sensor refuses a port that's taken.
- Other code reaches these devices through the aliases in
  `include/Reaper-Template/subsystems.hpp` (`chassis`, `LiftL`, `LiftR`,
  `claw`). Don't declare a second `pros::Motor` on the same port.

## Drivetrain

| Setting | Meaning |
| --- | --- |
| `DRIVE_LEFT_PORTS`, `DRIVE_RIGHT_PORTS` | Signed ports for each side. Use as many motors as your drive has. |
| `DRIVE_WHEEL_DIAMETER` | Use a measured size from `ez::wheel` (below), not the number on the box. |
| `DRIVE_WHEEL_DIAMETER` | Wheel diameter in inches (3.25, 4, 2.75…). |
| `DRIVE_WHEEL_RPM` | RPM at the **wheel**, after gearing. A 600 RPM cartridge geared 36:48 is 450. |

## Mechanisms

| Setting | Meaning |
| --- | --- |
| `LIFT_LEFT_PORT`, `LIFT_RIGHT_PORT` | The two lift motors. They normally have opposite signs, because they face each other. |
| `CLAW_PORT` | Claw motor. |
| `LIFT_ROTATION_PORT` | Optional rotation sensor on the lift. `0` uses the motor encoders instead. |
| `LIFT_MOTOR_DEGREES_PER_SENSOR_DEGREE` | Converts rotation sensor degrees into motor degrees. It's signed: make it negative if the sensor counts the other way. Work it out from your actual gearing. |
| `ROLLER_MOTOR_PORT`, `ROLLER_OPTICAL_PORT`, `ROLLER_POWER` | Optional color roller. See [Mechanisms](mechanisms.md#roller). |

Lift tuning (travel, presets, PID) is in a separate file. See [Lift](lift.md).

## IMUs

| Setting | Meaning |
| --- | --- |
| `DRIVE_IMU_1_PORT` | Main drivetrain IMU. Required for turns and odometry. |
| `DRIVE_IMU_2_PORT` | Optional second IMU. EZ-Template averages them and can catch one drifting. |
| `LIFT_IMU_PORT` | Optional IMU on the lift, watched by VMP only. |
| `VMP_CONSOLE_CSV` | `true` prints VMP data to the terminal as CSV. |

## Distance sensors (localization)

Each `localization::Mount` is `{port, x, y, heading, scale}`:

- `x`, `y`: inches from the robot's **turning center** to the sensor's face. +X is
  right, +Y is forward.
- `heading`: the direction the sensor faces, clockwise from forward: front `0`,
  right `90`, back `180`, left `270`.
- `scale`: calibration multiplier. Leave it at `1.0` unless you measured that a
  sensor reads consistently long or short.

| Setting | Meaning |
| --- | --- |
| `FIELD_HALF_SIZE` | Half the field width in inches, from the center to the wall. |
| `DISTANCE_SENSOR_DELAY_MS` | How old a distance reading is by the time you get it. Used to line readings up with where the robot was. |
| `DISTANCE_RESET_MAX_JUMP` | Biggest position correction, in inches, a manual wall reset may make. |
| `DISTANCE_MIN_CONFIDENCE` | Ignore distance readings the sensor rates below this (0-63, past 200 mm only). `0` = off. |

More detail in [Localization](localization.md).

## Tracking wheels

```cpp
//                         enabled, type,                         smart_port, ADI pair, diameter, offset, ratio
LEFT_TRACKER{  true, hardware::TrackerType::Rotation, 13,         {0, 0},   2.75,     4.0,    1.0};
```

| Field | Meaning |
| --- | --- |
| `enabled` | Leave `false` until the rest is filled in correctly. |
| `type` | `Rotation` (V5 rotation sensor), `ADI` (encoder on the brain's 3-wire ports) or `ExpanderADI` (encoder on a 3-wire expander). |
| `smart_port` | The rotation sensor's port, or the expander's port. Ignored for `ADI`. |
| `adi_ports` | Encoder channels, for example `{'A', 'B'}`. `{0, 0}` for `Rotation`. |
| `diameter` | Tracking wheel diameter in inches. |
| `offset` | Distance from the tracking wheel to the turning center, in inches. |
| `ratio` | Gear ratio between the wheel and the sensor. |

A Rotation sensor needs `smart_port` set and `adi_ports = {0, 0}`. A value like
`{13, 0}` is **not** a valid ADI encoder pair.

## After you change anything

1. Rebuild and upload.
2. Check the brain's **Motors** page: every configured motor should be green.
3. Read the terminal output (`pros terminal`) for `[Lift]`, `[VMP]` or
   `[Reaper]` messages about rejected settings.

## Wheel sizes

The size printed on a wheel isn't its real rolling diameter, and odometry is only as
accurate as this number. Use the measured values in
`include/Reaper-Template/wheel_sizes.hpp` (from LemLib) for the drive and for
tracking wheels:

```cpp
static constexpr double DRIVE_WHEEL_DIAMETER = ez::wheel::NEW_325;
```

| Name | Diameter (in) | | Name | Diameter (in) |
| --- | --- | --- | --- | --- |
| `NEW_2` | 2.125 | | `NEW_325` | 3.25 |
| `NEW_275` | 2.75 | | `NEW_325_HALF` | 3.246 |
| `NEW_275_HALF` | 2.744 | | `OLD_325`, `OLD_325_HALF` | 3.25, 3.246 |
| `OLD_275` | 2.75 | | `NEW_4` | 4.0 |
| `OLD_275_HALF` | 2.74 | | `NEW_4_HALF` | 3.995 |
| | | | `OLD_4` | 4.18 |
| | | | `OLD_4_HALF` | 4.175 |

`NEW` is the current VEX omni wheel, `OLD` the previous generation, `HALF` the
half-cut versions. The old 4" wheel really is 4.18", so the wrong one is off by 4.5%:
about 3 inches over a 72-inch drive.
