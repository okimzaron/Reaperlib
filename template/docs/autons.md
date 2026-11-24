# Writing autons

## Where routes live

| File | Contains |
| --- | --- |
| [`src/routes/match_routes.cpp`](../src/routes/match_routes.cpp) | 15 match route slots and `register_routes()` |
| [`src/routes/skills_routes.cpp`](../src/routes/skills_routes.cpp) | 3 skills route slots |
| [`include/autons.hpp`](../include/autons.hpp) | Declarations for every route |
| [`src/autons.cpp`](../src/autons.cpp) | `default_constants()`: PID, exit conditions, slew, odometry settings |

## Anatomy of a route

```cpp
ez::Auton match_02() {
  return {"Left Side", "Grab the near goal, score, touch the bar.", [] {
    chassis.pid_drive_set(24_in, 110);
    chassis.pid_wait();
  }};
}
```

- **Name**: the title shown on the brain's Routes page. Keep it short.
- **Description**: one line explaining what the route does.
- **Body**: the code that runs.

Rename the function too if you like, but then update
`include/autons.hpp` and the list in `register_routes()` to match.

## Registering routes

At the bottom of `match_routes.cpp`:

```cpp
void register_routes() {
  ez::as::auton_selector.autons_add({ match_01(), match_02(), /* ... */ match_15() });
  ez::as::auton_selector.skills_add({skills_01(), skills_02(), skills_03()});
}
```

- Up to **15** match routes and **3** skills routes.
- The order here is the order on the brain screen.
- `register_routes()` runs first thing in `initialize()`.
- The selected route is saved to the SD card (`auto.txt`) if one is inserted, so it
  survives a power cycle.

## What runs before your route

Every time `autonomous()` starts (on a field or with Down + B), Reaper:

1. Resets PID targets, the IMU and the drive sensors.
2. Sets the position to `(0, 0)` facing `0°`.
3. Sets the drive brake to HOLD.
4. Enables the lift.
5. Stops the roller.

Then it runs your route, and stops the roller again afterwards. To start somewhere
other than `(0, 0, 0°)`, put this at the top of the route:

```cpp
chassis.odom_xyt_set(-48_in, -60_in, 90_deg);
```

## Motion basics (EZ-Template)

These are EZ-Template functions. Its docs cover all of them in detail:
<https://ez-robotics.github.io/EZ-Template/>

```cpp
chassis.pid_drive_set(24_in, 110);           // drive straight 24 in at speed 110
chassis.pid_wait();                          // wait until it's done

chassis.pid_turn_set(90_deg, 90);            // turn to face 90° (absolute)
chassis.pid_wait();

chassis.pid_swing_set(ez::LEFT_SWING, 45_deg, 90);  // swing on one side
chassis.pid_wait();

chassis.pid_turn_set({24_in, 24_in}, ez::fwd, 90);  // turn to face a point
chassis.pid_wait();

chassis.pid_swing_set(ez::RIGHT_SWING, {24_in, 24_in}, ez::fwd, 90);  // swing to face a point
chassis.pid_wait();

chassis.pid_drive_set(36_in, 110);
chassis.pid_wait_until(24_in);               // do something partway through
lift::move_to("Score");
chassis.pid_wait();

chassis.pid_odom_set({{24_in, 24_in}, ez::fwd, 110});   // drive to a point
chassis.pid_wait();
```

Use `pid_wait_quick_chain()` instead of `pid_wait()` to flow straight into the next
motion without fully stopping.

**Swinging to a point** works out its heading once, when the swing starts, from the
wheel the robot pivots on (`LEFT_SWING` pivots on the right wheel, `RIGHT_SWING` on
the left). That way the robot's *center* ends up lined up with the point, even though
it moves sideways while swinging. It needs the drive width (`chassis.drive_width_set(...)`);
without one it aims from the robot's center. Use `ez::rev` to point the back at it.

### Robot speed

```cpp
pose v = chassis.odom_velocity_get();        // field x/y in in/s, theta in deg/s
pose local = chassis.odom_velocity_local_get();  // y = forward speed, x = sideways
pose ahead = chassis.odom_pose_estimate(150);    // where it'll be in 150 ms at this speed
```

Speeds are smoothed over about 50 ms, and resetting the position never counts as
moving. `odom_pose_estimate` is a straight-line guess, good for short look-aheads
such as allowing for sensor delay.

## Using mechanisms in a route

Move the lift and claw through the lift module, never with `LiftL.move()` or
`claw.move()` directly, so the background controller stays in charge:

```cpp
lift::move_to("Score");             // a preset you measured (see lift.md)
chassis.pid_drive_set(12_in, 80);
chassis.pid_wait();
if (!lift::wait_until_settled(3000)) return;  // stop the route if the lift never arrived
```

For a timed lift + claw + piston combo, use a sequence. See
[Mechanisms](mechanisms.md#sequences).

## Tuning (`default_constants()`)

`src/autons.cpp` holds the numbers that shape every motion:

| Call | Controls |
| --- | --- |
| `pid_drive_constants_set(p, i, d)` | Driving straight |
| `pid_heading_constants_set(p, i, d)` | Staying straight while driving |
| `pid_turn_constants_set(p, i, d, start_i)` | Turning in place |
| `pid_swing_constants_set(p, i, d)` | Swing turns |
| `pid_odom_angular_constants_set`, `pid_odom_boomerang_constants_set` | Odometry motions |
| `pid_*_exit_condition_set(...)` | When a motion counts as done |
| `pid_*_chain_constant_set(...)` | How close counts as done for `pid_wait_quick_chain()` |
| `slew_*_constants_set(distance, min_speed)` | Gentle starts |

The shipped values are a starting point, not a tune for your robot. EZ-Template's
PID tuner and tuning guide work in Reaper unchanged.

### Stuck detection

Reaper reworks EZ-Template's exit conditions to detect a robot that's stuck, for
example pushed against a wall or blocked by another robot. When driving, a `pid_wait()`
no longer ends just because the robot is moving slowly. It ends when the robot stops
making progress for long enough. The shortest window it checks is 350 ms, so shrinking
`velocity_exit_time` won't make it trigger on a slow but moving robot. Details are
in the comments at the top of `src/Reaper-Template/drive/exit_conditions.cpp`.

## Testing

Press **Down + B** on the controller (no field connected) to run the selected route,
and press it again to cancel. See [Driver controls](controls.md#down--b-testing-autons).
