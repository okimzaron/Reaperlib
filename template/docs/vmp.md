# VMP (IMU motion monitor)

VMP watches up to three IMUs 100 times a second and answers four questions:

| Question | Where you see it | From code |
| --- | --- | --- |
| Is the robot still? | Sensors page `quiet` | `vmp::is_quiet()`, `vmp::wait_quiet(ms)` |
| Did it just get hit? | Sensors page `hits` / `peak` | `vmp::impact_since(t)`, `vmp::last_impact()` |
| Is it tipping? | Sensors page `tilt`, controller `!TIPPING` | `vmp::warning()` |
| Is the lift swaying? | Sensors page (lift card), controller `!LIFT SWAY` | `vmp::warning()` |

It **only watches**: it never calibrates or resets an IMU and never moves a motor.
The only things that act on it are the optional driver warnings (rumble and a drive
speed cap) and localization pausing its corrections right after a hit, all
configurable below.

## Setup

In `robot_hardware.hpp`:

| Setting | Slot |
| --- | --- |
| `DRIVE_IMU_1_PORT`, `DRIVE_IMU_2_PORT` | Drive IMUs (the same ones EZ-Template uses for heading). Used for still / hits / tipping. |
| `LIFT_IMU_PORT` | Optional IMU mounted high on the lift. Used for sway. `0` = none. |

Tuning is in `include/Reaper-Template/vmp_settings.hpp`. **Start the program with the
robot flat on the field**: the first time it sits still after startup becomes "flat"
for the tilt reading.

## The Sensors page

Tap **S** on the brain dashboard. One card per IMU:

```
Drive IMU 1 P5
quiet               <- warming / active / quiet / invalid / stale
shake 0.004 g       <- how much it's vibrating
rot 0.3 dps         <- rotation jitter
peak 0.12 g         <- biggest jolt in the last 2 s
hits 0              <- jolts above IMPACT_G since startup
tilt 0.8 deg        <- lean from "flat"
```

The card border is green when quiet, amber when active, red when the IMU isn't
reading. The bottom line shows any warning. **Zero tilt** makes the robot's current
orientation (once it's still) the new "flat".

## Tuning, using the Sensors page

1. **Still:** park the robot. Cards should go `quiet` within a second. If they never
   do (a motor humming, a fan), raise `QUIET_ACCEL_G` / `QUIET_GYRO_DPS` a little.
2. **Hits:** drive around normally and watch `peak`. Then drive gently into a wall and
   watch it again. Set `IMPACT_G` between the two. `hits` should then count wall bumps
   but not normal driving.
3. **Tipping:** lean the robot by hand and watch `tilt`. `TIP_WARN_DEG` is where the
   warning starts, `TIP_CLEAR_DEG` where it stops.
4. **Lift sway:** raise the lift, drive and stop hard, and watch the lift card's `rot`.
   Set `SWAY_GYRO_DPS` just under what you see when it's swaying badly.

## Using it in autons

Wait until the robot has actually stopped (not just until the drive PID says it's
done) before reading your start position or doing a wall reset:

```cpp
chassis.pid_wait();
vmp::wait_quiet(600);   // true once still; gives up after 600 ms
localization::distance_reset(localization::Sensor::left, 150);  // averaged over 150 ms
```

`wait_quiet` returns `false` at once if no drive IMU is configured, so a route never
sits waiting for nothing.

React to a collision:

```cpp
const auto t = pros::millis();
chassis.pid_drive_set(24_in, 110);
chassis.pid_wait();
if (vmp::impact_since(t)) {
  // something hit us during that move: re-check position, back off, etc.
}
```

## Driver warnings

When a tip or sway warning starts, in driver control:

- The controller rumbles once (`---` tipping, `-` sway). `WARN_RUMBLE = false` turns it off.
- The controller's heading line shows `!TIPPING 18deg` or `!LIFT SWAY` while it lasts.
- The drive is capped at `TIP_DRIVE_SPEED_LIMIT` / `SWAY_DRIVE_SPEED_LIMIT` (out of 127)
  until the warning clears. Set them to `127` for no cap.

## Localization after a hit

For `LOCALIZATION_HOLDOFF_MS` (300 ms) after a hit, localization keeps running but
doesn't apply distance-sensor corrections. The robot may have been shoved, and the
sensors may be seeing another robot. Set it to `0` to turn this off.

## CSV logging

`VMP_CONSOLE_CSV = true` in `robot_hardware.hpp` prints one row per IMU ten times a
second to the terminal:

```
VMP,ms,slot,state,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,accel_rms_g,gyro_rms_dps,peak_g,impacts,tilt_deg,dt_ms
```

## States

| State | Meaning |
| --- | --- |
| `disabled` | Port is `0` |
| `invalid` | The port doesn't have a working IMU |
| `warming` | First second after start |
| `active` | Moving or vibrating |
| `quiet` | Still for at least `QUIET_HOLD_MS` |
| `stale` | No new reading for over 100 ms |
