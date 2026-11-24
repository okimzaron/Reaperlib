# Changelog

## Unreleased

**Fixes**
- Lift: motor direction is now set once, by the port signs in `robot_hardware.hpp`
  (as the docs always said). The code no longer reverses the right motor a second
  time, which made the two motors fight when the ports had opposite signs.
- Lift: R2/L2 jog slowly until `max_height` is set, even with
  `full_power_manual_jog` on, so calibration never runs at full power with no limit.
- Down + B test auton: the auton task no longer deletes itself, so its handle is
  never read after being freed (that could leave driver control locked out). A
  manual run is also ended when the robot is disabled.
- Boot: EZ-Template's LLEMU screen no longer flashes before the Reaper splash; the
  legacy EZ brain-page task is removed. The PID tuner hands the screen back to the
  dashboard when it closes.

**Roller**
- Rebuilt as an independent subsystem with its own task. Any task can call
  `spin`, `stop`, `seek`, `wait`, `roll_to_target`, `maintain`, `maintain_for`.
- Recognises red, yellow and blue; rolls to the alliance color, never stops on yellow.
- Hold **X** in driver control. Live optical readout on the Motors page and
  `[Roller] Sees ...` in the terminal. Thresholds in `roller_settings.hpp`.

**Removed**
- The two "toggle" motors (`TOGGLE_LEFT_PORT`, `TOGGLE_RIGHT_PORT`, `ToggleL`,
  `ToggleR`). They were a stand-in for the roller, which now does that job on **A**.

**Team branding**
- New `src/team/` folder holding everything team-specific: `team_theme.hpp` (team
  name, logo, every dashboard color) and the logo image. Ships filled in for 4588G
  Gambit as an example. The Reaper splash is unchanged. The logo is centered
  whatever its size, and the two-tone Gambit logo follows the theme colors.
  Guide in `docs/brain-screen.md`.
- The first match route is a plain `match_01` placeholder again.

**Controls**
- New `include/robot_controls.hpp`: every driver button, claw/toggle power, drive
  style, joystick-curve and PID-tuner button in one place. Two driver actions on the
  same button fail the build; curve-button overlaps are printed at startup.

**VMP**
- Brain **Sensors** page (S): per-IMU state, shake, rotation jitter, windowed peak,
  hit count and tilt, plus a Zero tilt button.
- `vmp::wait_quiet()` / `is_quiet()` for waiting until the robot is really still.
- Hit detection: `vmp::impact_since()`, `vmp::last_impact()`. The peak reading is now
  "biggest jolt in the last 2 s" instead of growing forever.
- Tipping (drive IMUs, mount-independent tilt) and lift-sway (lift IMU) warnings:
  controller rumble, `!TIPPING` / `!LIFT SWAY` on the controller, optional drive
  speed cap. Localization pauses corrections briefly after a hit.
- All thresholds in the new `vmp_settings.hpp`.

**From LemLib**
- Swing to face a point: `pid_swing_set(ez::LEFT_SWING, {x, y}, ez::fwd, speed)`. It
  aims from the pivot wheel, so the robot's center lines up with the point.
- Robot speed and prediction: `odom_velocity_get()`, `odom_velocity_local_get()`,
  `odom_pose_estimate(ms)`.
- Joystick minimum output (`DRIVE_MIN_OUTPUT`) and deadband (`DRIVE_DEADBAND`) in
  `robot_controls.hpp`.
- Measured omni wheel diameters, `ez::wheel::NEW_325` etc. (`wheel_sizes.hpp`). The
  drive and tracking-wheel defaults now use them.

**Localization (ideas from RCL-Tracking)**
- Distance readings the sensor rates as low-confidence are ignored
  (`DISTANCE_MIN_CONFIDENCE`, default 60), in the filter and in wall resets.
- Averaged wall resets: `localization::distance_reset(sensor, 150)` takes the median
  of the readings, and refuses if they're unsteady or the robot moved.
- `localization::last_reset_result()` says why a reset was refused.
- Field map (`include/field_map.hpp`): circles and segments for goals, loaders and
  barriers. Sensors whose beam would pass one before the wall are skipped by the
  filter and by wall resets (`Blocked`). Ships empty; fill it in for your game.

**Code**
- Reformatted the lift, localization, VMP and piston-group code for readability
  (new `.clang-format`, 110 columns).

## 1.0 beta

First public beta of Reaper, built on EZ-Template 4.0.0-beta.4 and PROS 4.2.2.

**Features**
- One-file hardware configuration (`robot_hardware.hpp`) with port checks
- Lift controller: travel limits, presets, optional rotation sensor, gravity
  compensation, gain scheduling, encoder disagreement faults
- Non-blocking lift / claw / pneumatic / wait sequences
- `ez::PistonGroup`
- Distance-sensor localization using RW-Template's particle filter, plus wall resets
- VMP IMU motion monitor with optional CSV logging
- Motor monitor and Motors dashboard page
- Scythe splash animation during IMU calibration
- Touch dashboard: Home, Routes (15 match + 3 skills, red/blue alliance), Motors
- Controller screen: pose, heading, lift position, lift startup errors
- Down + B manual auton runner
- Kill-safe locking so mode changes can't freeze the drive task
- Reworked stuck detection in drive exit conditions
- Optional color roller

**Package cleanup**
- All mechanism ports reset to `0` (disabled), including the lift
- Removed build caches, the compile database and machine-specific editor settings
- Removed the old GitHub Pages docs workflow and the old upgrade notes, replaced by
  the `docs/` guides
- Version numbers unified to 1.0 beta; added `reaper_version.hpp`
- Added `CREDITS.md` and license files for every project Reaper builds on
