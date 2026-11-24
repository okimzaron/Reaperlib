# Reaper 1.0 beta

Reaper is a VEX V5 code template for PROS. It's built on
[EZ-Template](https://ez-robotics.github.io/EZ-Template/) and adds the things a
competition robot needs past the drivetrain: a lift controller, mechanism sequences,
distance-sensor localization, a touch-screen dashboard, a controller readout and
startup safety checks.

Created by Za'Ron Lee, 4588G Gambit.

> **Beta:** Reaper 1.0 is still in beta. Test every setting on your own robot before
> you compete with it. Every port ships set to `0` (disabled), so nothing moves until
> you configure it.

## Features

| Area | What you get |
| --- | --- |
| Drivetrain | Everything EZ-Template 4.0 does: PID drive/turn/swing, odometry, boomerang, pure pursuit, a PID tuner and joystick curves. Exit conditions are reworked to catch a robot that's stuck or pushed. |
| Setup | All ports and sensor positions in one file, [`include/robot_hardware.hpp`](include/robot_hardware.hpp). All driver buttons in another, [`include/robot_controls.hpp`](include/robot_controls.hpp). Duplicate buttons fail the build. |
| Lift | One background controller with travel limits, named presets, optional rotation sensor, gravity compensation, gain scheduling and fault detection. |
| Sequences | Lift, claw, pneumatic and wait steps that run in order without blocking your code. |
| Pneumatics | `ez::Piston` plus `ez::PistonGroup` for pistons that fire together. |
| Localization | Up to four distance sensors correct the robot's X/Y position against the field walls, using RW-Template's particle filter. |
| Brain screen | An animated scythe splash during calibration, then a touch dashboard with Home, Routes (15 match + 3 skills slots, red/blue alliance) and Motors (connection and temperature for every configured motor). |
| Controller | Live X/Y, heading and lift position on the controller screen. |
| Testing | **Down + B** runs the selected auton when no field is connected. Press it again to cancel. |
| Diagnostics | Motor monitor, IMU motion monitor (VMP: still / hit / tip / lift-sway detection) and clear startup messages. |

## Quick start

1. Install [PROS](https://pros.cs.purdue.edu/) (the VS Code extension is easiest).
2. Open this folder in VS Code.
3. Set your ports in `include/robot_hardware.hpp`.
4. Write your autons in `src/routes/match_routes.cpp` and `src/routes/skills_routes.cpp`.
5. Build and upload: `pros mu`.

The full walkthrough is in [Getting started](docs/getting-started.md).

## Documentation

| Guide | Covers |
| --- | --- |
| [Getting started](docs/getting-started.md) | Install, configure, upload, and a first-boot checklist |
| [Hardware configuration](docs/hardware-config.md) | Every setting in `robot_hardware.hpp` |
| [Driver controls](docs/controls.md) | Button map, the controller screen, Down + B testing |
| [Writing autons](docs/autons.md) | Routes, the selector, PID constants, motion basics |
| [Brain screen](docs/brain-screen.md) | Splash, dashboard pages, your team's name, logo and colors (`src/team/`) |
| [Lift](docs/lift.md) | Calibration, presets, gravity compensation, gain scheduling |
| [Mechanisms](docs/mechanisms.md) | Claw, pistons, piston groups, sequences, roller |
| [Localization](docs/localization.md) | Distance sensors, wall resets, tracking wheels |
| [VMP](docs/vmp.md) | IMU stillness, hit detection, tipping and lift-sway warnings, the Sensors page |
| [Diagnostics](docs/diagnostics.md) | Motor monitor, console messages |
| [Troubleshooting](docs/troubleshooting.md) | Common problems and fixes |
| [Architecture](docs/architecture.md) | How Reaper works inside, for anyone changing the template |

For drive motions and tuning, EZ-Template's own docs apply directly:
<https://ez-robotics.github.io/EZ-Template/>

## Project layout

```
include/robot_hardware.hpp      <- your ports and sensor positions
include/robot_controls.hpp      <- your controller buttons and drive style
include/field_map.hpp           <- field elements that block distance sensors (per game)
include/Reaper-Template/        <- template headers (lift_settings.hpp is yours to edit)
src/main.cpp                    <- initialize, autonomous, opcontrol
src/autons.cpp                  <- PID constants and motion defaults
src/routes/                     <- your match and skills routes
src/team/                       <- your team name, logo and dashboard colors (4588G example)
src/Reaper-Template/            <- template source
sd-card/                        <- optional SD card files (selected auton, joystick curves)
docs/                           <- these guides
licenses/                       <- licenses for everything Reaper builds on
```

## Versions

| | |
| --- | --- |
| Reaper | 1.0 beta |
| EZ-Template base | 4.0.0-beta.4 |
| PROS kernel | 4.2.2 |

## Credits and license

Reaper is released under the Mozilla Public License 2.0 ([LICENSE.md](LICENSE.md)).
It's built on work by EZ-Robotics (EZ-Template), Richard Wang (RW-Template), Purdue
SIGBots (PROS, OkapiLib), LVGL and LemLib. See [CREDITS.md](CREDITS.md) for exactly
what came from where, and [`licenses/`](licenses/) for the license texts.
