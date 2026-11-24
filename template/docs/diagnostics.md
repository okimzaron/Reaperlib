# Diagnostics

## Terminal output

Open the PROS terminal (`pros terminal`, or the PROS sidebar) with the brain or
controller plugged in by USB. Reaper's messages start with a tag:

| Message | Meaning |
| --- | --- |
| `[Reaper] Reaper 1.0 beta (EZ-Template 4.0.0-beta.4) initializing...` | Normal startup. Confirms the version you uploaded. |
| `[Lift] <reason> (configured motor ports a, b)` | The lift failed a startup check. See [Troubleshooting](troubleshooting.md#lift-startup-messages). |
| `[VMP] Invalid/duplicate ports or monitoring already started.` | Check the IMU ports. |
| `[Reaper] Localization configuration rejected.` | A distance sensor setting is out of range, or two sensors share a port. |
| `[Controller UI] Could not start display task.` | The controller screen didn't start. Rare. |
| `[Reaper] Test auton started / finished / cancelled.` | Down + B was used. |
| `[Roller] Sees RED (hue 8, sat 0.71, prox 150)` | The roller's optical sensor settled on a new color. Use it to tune thresholds. |
| `[Roller] Optical sensor on port N is not responding.` | Check the optical sensor cable / port. |
| `[Roller] Invalid configuration ...` | A roller port or power in `robot_hardware.hpp` is out of range. |
| `[Reaper splash] ...` | Splash animation progress. Safe to ignore. |

## Motor monitor

A background task checks every configured motor's connection and temperature and
feeds the brain's **Motors** page. It covers the drive, lift, claw, toggle and roller
motors, and also flags a motor port that a sensor is using too. See
[Brain screen](brain-screen.md#motors-page) for what each status means.

From code:

```cpp
for (const auto& m : motor_monitor::snapshot())
  printf("%s port %d %.0f C\n", m.name.c_str(), m.port, m.temperature);
```

## VMP (IMU motion monitor)

VMP watches the drive IMUs and an optional lift IMU for stillness, hits, tipping and
lift sway. Its readings are on the brain's **Sensors** page. See [VMP](vmp.md).

## IMU checks

- **Calibration:** one controller rumble means the IMU is fine. Three long rumbles
  mean it isn't. Check the port, and keep the robot still during the splash.
- **Two IMUs:** EZ-Template compares them. If they disagree by more than 15°
  (`chassis.imu_drift_threshold_set()` changes this), it flags the drift.
