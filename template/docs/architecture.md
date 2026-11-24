# Architecture

This guide is for anyone changing Reaper itself rather than just using it.

## Startup order (`initialize()`)

1. `register_routes()`: fills the selector.
2. Saved route restored from the SD card (no LLEMU: the dashboard owns the screen), then the motor monitor starts.
3. Dashboard configured and built (`dashboard_configure`, `ui_initialize`).
4. `reaper_splash::start()`: the animation runs on its own 3-second timeline.
5. `robot.configure_devices()`: creates tracking wheels and starts the roller task, before the chassis starts.
6. `lift::initialize()`: lift hardware checks and task.
7. Joystick curve settings, then `default_constants()`.
8. `chassis.initialize(false)`: IMU calibration, with no EZ loading bar.
9. `vmp::start(...)`, then `localization::start(...)`.
10. Controller rumble (`.` = IMU OK, `---` = failed), then `controller_ui::start()`.

## Background tasks

| Task | Rate | Owns |
| --- | --- | --- |
| EZ-Template drive task | 10 ms | Drive motors, PID, odometry |
| Reaper Mechanisms (`lift.cpp`) | 10 ms | Lift motors, claw motor, sequences |
| Localization | 10 ms | Distance sensor reads; corrects the chassis pose |
| VMP | 10 ms (+ 100 ms CSV logger) | IMU reads, still/hit/tilt/sway decisions |
| Motor monitor | periodic | Motor connection and temperature reads |
| Controller UI | 100 ms per line | Controller screen |
| Roller | 10 ms | Optical sensor read, color decision, roller motor |
| LVGL / display | PROS | Splash and dashboard. UI changes go through `lv_async_call`. |

**One owner per actuator.** Each motor has exactly one task or function that writes
to it. Other code sends requests (lift commands, `roller::seek`/`spin`,
`claw_driver_update`) instead of calling `move()` directly. Keep it that way when
you add mechanisms: give the new mechanism one owner and a command interface.

## Hardware ownership

`RobotHardware` (`include/robot_hardware.hpp`, `src/Reaper-Template/robot_hardware.cpp`)
holds the single instance of every motor and the chassis, as the global `robot`.
`subsystems.hpp` gives short aliases (`chassis`, `LiftL`...) that point at those same
objects. It never makes copies. Copying `RobotHardware` is deleted on purpose.

## Locking rules (`lock.hpp`)

PROS deletes the competition task (autonomous, opcontrol or disabled) wherever it is
when the mode changes. A deleted task never releases a mutex it was holding, which
can freeze the drive task for good.

Reaper's `ez::KillSafeGuard` raises the task above the system daemon's priority
while it holds a lock, so the task can't be deleted mid-lock. The rules for
library code:

1. Take every lock with `ez::KillSafeGuard`, or with `ez::PlainGuard` in tasks PROS
   never deletes. Never use a bare `pros::Mutex`, `std::lock_guard` or `take()`/`give()`.
2. Never block while holding a guard: no `printf`, no `pros::delay`, no screen calls,
   no second lock. Use `print_after_unlock(...)` to print.

The header comment in `include/Reaper-Template/lock.hpp` explains this in full.

## Module map

| Module | Header | Source | Notes |
| --- | --- | --- | --- |
| Hardware config | `robot_hardware.hpp` | `robot_hardware.cpp` | Single source of ports |
| Controls config | `robot_controls.hpp` | `robot_controls.cpp` | Single source of driver buttons and drive style |
| Lift + sequences | `lift.hpp`, `lift_core.hpp`, `lift_settings.hpp`, `sequence_core.hpp` | `lift.cpp` | `lift_core.hpp` is pure logic with no PROS calls, so it can be tested on a computer |
| Piston groups | `piston_group.hpp` | header-only | |
| Localization | `localization.hpp`, `localization_core.hpp`, `rw/mcl.hpp`, `rw/utils.hpp` | `localization.cpp`, `rw/*.cpp` | `localization_core.hpp` and `rw/` are pure math |
| VMP | `vmp.hpp`, `vmp_core.hpp`, `vmp_settings.hpp` | `vmp.cpp` | `vmp_core.hpp` is pure logic |
| Motor monitor | `motor_monitor.hpp`, `motor_status.hpp` | `motor_monitor.cpp` | |
| Roller | `roller.hpp`, `roller_core.hpp`, `roller_settings.hpp` | `roller.cpp` | |
| Controller UI | `controller_ui.hpp` | `controller_ui.cpp` | |
| Dashboard / selector | `auton_selector.hpp`, `touch_gate.hpp`, `display.hpp` | `auton_selector.cpp`, `auton_pages.hpp`, `display.cpp` | |
| Splash | `reaper_splash.hpp`, `reaper_splash_assets.hpp` | `reaper_splash.cpp`, `reaper_splash_assets.cpp` | Asset file is generated geometry |
| Field map | `field_map.hpp`, `field_map_core.hpp` | - | Obstacles that block distance sensors; `field_map_core.hpp` is pure geometry |
| Team branding | `src/team/team_theme.hpp` | `src/team/gambit_logo.cpp` | Name, logo, dashboard colors; `src/team/` is on the include path (`EXTRA_INCDIR` in the Makefile) |
| Drive (EZ-Template) | `drive/drive.hpp` | `drive/*.cpp` | Exit conditions reworked for stuck detection |
| Versions | `reaper_version.hpp`, `version.hpp` | | `version.hpp` is EZ-Template's base version, not Reaper's |

Everything is pulled in through `include/main.h` → `Reaper-Template/api.hpp`.

## Versioning

When you release a new version, change it in all three places:

1. The `version` file (`1.0 beta`)
2. `VERSION` in the `Makefile`. This must be semver for PROS (`1.0.0-beta`).
3. `include/Reaper-Template/reaper_version.hpp`

Don't edit `version.hpp`: it describes the EZ-Template base.

## Building as a PROS template

Set `IS_LIBRARY:=1` in the `Makefile` and run `pros make template` to produce a
`Reaper-Template@1.0.0-beta.zip` that other projects can apply with `pros c fetch` /
`pros c apply`. On GitHub, the `Build` workflow in `.github/workflows/main.yml` builds the template on
every push using LemLib's `pros-build` action, and `pr-edit.yml` posts a download
link on pull requests.

## Tests

The pure-logic headers (`lift_core.hpp`, `sequence_core.hpp`, `localization_core.hpp`,
`vmp_core.hpp`, `motor_status.hpp`, `rw/`) have no PROS dependencies, so they can be
compiled and tested with a normal desktop `g++`. The host test folder (`tests/`) is
not included in 1.0 beta. Restoring it is planned.
