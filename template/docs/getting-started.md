# Getting started

This guide takes you from a fresh download to a robot that drives.

## 1. Install the tools

1. Install [VS Code](https://code.visualstudio.com/).
2. In VS Code, open Extensions, search for **PROS** and install it. The extension
   installs the PROS command line tools and the ARM compiler for you.
3. Restart VS Code once the install finishes.

## 2. Open Reaper

1. Unzip Reaper somewhere easy to find, for example `Documents/Reaper`.
   Avoid OneDrive or other synced folders: syncing can lock files mid-build.
2. In VS Code, use **File → Open Folder** and pick the folder that contains
   `project.pros`.
3. Open the PROS sidebar and click **Build**, or run this in the PROS terminal:

   ```
   pros make
   ```

   A clean build shows no errors. The first build takes a minute or two.

## 3. Set your ports

Everything about your robot's wiring lives in **one file**:
[`include/robot_hardware.hpp`](../include/robot_hardware.hpp). Don't change ports
anywhere else.

At minimum, set:

```cpp
static inline const std::vector<int> DRIVE_LEFT_PORTS{-1, -2, -3};   // negative = reversed
static inline const std::vector<int> DRIVE_RIGHT_PORTS{4, 5, 6};
static constexpr double DRIVE_WHEEL_DIAMETER = 3.25;  // inches
static constexpr double DRIVE_WHEEL_RPM = 450.0;      // speed at the wheel, after gearing
static constexpr int DRIVE_IMU_1_PORT = 10;
```

Every port ships set to `0`, which means "not plugged in". Anything left at `0` is
switched off, so you can enable the robot one part at a time.

[Hardware configuration](hardware-config.md) explains every setting.

## 4. Upload

Plug the brain or controller into your computer by USB, then:

```
pros mu
```

`mu` means make and upload. Pick a program slot on the brain if asked.

## 5. First boot checklist

Run through this with the robot **on a stand, wheels off the ground**.

- [ ] The scythe splash plays while the IMU calibrates. **Don't touch the robot
      until it finishes.**
- [ ] The controller rumbles once (`.`). Three long rumbles (`---`) mean the IMU did
      not calibrate. Check its port and cable.
- [ ] The **Motors** page (the `M` button on the brain) shows every motor green and
      `OK`. Red means unplugged, wrong device or a port used twice.
- [ ] Push both joysticks forward. Every drive wheel spins forward. If one side spins
      backward, flip the sign of that side's ports.
- [ ] Turn in place. The heading on the controller screen (`H:`) goes up when you
      turn right (clockwise).
- [ ] Only then put the robot on the ground.

If you have a lift, follow the calibration steps in [Lift](lift.md) before driving
it. Its upward travel stays limited until you set its height.

## 6. Write an auton

Open `src/routes/match_routes.cpp` and fill in the first route:

```cpp
ez::Auton match_01() {
  return {"Match 01", "Drive forward, then turn right.", [] {
    chassis.pid_drive_set(24_in, 110);
    chassis.pid_wait();
    chassis.pid_turn_set(90_deg, 90);
    chassis.pid_wait();
  }};
}
```

Upload, pick the route on the brain's **Routes** page, and press **Down + B** on the
controller to test it without a field controller. Press **Down + B** again to stop.
[Writing autons](autons.md) covers more.

## Next steps

- [Driver controls](controls.md): what each button does
- [Writing autons](autons.md): routes, tuning and motions
- [Troubleshooting](troubleshooting.md): if something doesn't work
