# Localization

Odometry drifts: wheels slip, and every turn adds a little error. Reaper uses up to
four distance sensors that see the field walls to pull the robot's X/Y position back
to the truth while it drives.

The math is RW-Template's Monte Carlo localization: a particle filter of 200
guesses at where the robot is. Each guess is scored by how well it explains the
distance readings, and good guesses survive. Heading still comes from the IMU. The
walls only correct X and Y.

## Coordinates

- The field center is `(0, 0)`.
- +X is right, +Y is forward (as you face the field from your starting side).
- Headings are clockwise from +Y, in degrees.
- `FIELD_HALF_SIZE` (70.25 in by default) is the distance from the center to a wall.

These are the same coordinates EZ-Template's odometry uses, so
`chassis.odom_xyt_set()` and `chassis.pid_odom_set()` line up with the corrections.

## Setup

In `robot_hardware.hpp`, give each sensor its port and **where its face is**,
measured from the robot's turning center:

```cpp
//                                         port, x,    y,    heading, scale
static constexpr localization::Mount DISTANCE_FRONT{ 7,  0.0,  6.5,    0.0, 1.0};
static constexpr localization::Mount DISTANCE_RIGHT{ 8,  5.5,  0.0,   90.0, 1.0};
static constexpr localization::Mount DISTANCE_BACK { 0,  0.0,  0.0,  180.0, 1.0};  // not fitted
static constexpr localization::Mount DISTANCE_LEFT { 9, -5.5,  0.0,  270.0, 1.0};
```

Measure carefully. An inch of mounting error becomes an inch of position error.
With all four ports at `0`, localization doesn't run at all.

## How it behaves

- It runs in its own task every 10 ms and corrects the pose about every 30 ms.
- Readings over 60 in, or with the sensor more than 30° off square to the wall, are
  ignored.
- A sensor reading the exact same value twice in a row is skipped, so a stuck
  sensor can't drag the position.
- Readings are lined up with where the robot was `DISTANCE_SENSOR_DELAY_MS` ago, to
  allow for sensor lag.
- Setting the pose (`chassis.odom_xyt_set(...)`, or the reset at the start of
  `autonomous()`) re-centers the filter on the new position.
- Game objects or robots in front of a sensor make it read short. The filter rejects
  readings that don't fit, but a clear view of the wall works best.

## Wall resets

When the robot is stopped against or facing a wall, you can snap one axis to it:

```cpp
chassis.pid_wait();
vmp::wait_quiet(600);                                       // until the robot is really still (or 600 ms)
if (!localization::distance_reset(localization::Sensor::left, 150)) {   // average for 150 ms
  printf("reset refused: %s\n",
         localization::reset_result_name(localization::last_reset_result()));
}
```

The second number is how long to average for. With `150`, about four readings are
taken and the **median** is used, which is steadier than one reading. Leave it out
(or pass `0`) for a single, instant reading.

If a reset is refused, `last_reset_result()` says why:

| Result | Meaning | Try |
| --- | --- | --- |
| `NoReading` | Sensor unplugged, out of range, or not confident in what it sees | Check the port; aim squarely at a plain wall |
| `TooFewReadings` | Fewer than 3 good readings while averaging | Average longer, or check the sensor |
| `Unsteady` | Readings spread over 1 inch: something moved in front of the sensor | Make sure the view to the wall is clear |
| `RobotMoved` | The robot moved while averaging | Wait for `vmp::wait_quiet()` first |
| `Blocked` | A field element in `field_map.hpp` is between the sensor and the wall | Reset against a different wall, or with a different sensor |
| `Refused` | Not square enough to the wall, robot not settled, or the jump was bigger than `DISTANCE_RESET_MAX_JUMP` | Square up to the wall, wait longer |
| `NotRunning` | No distance sensors configured | Set the ports in `robot_hardware.hpp` |

Without a drive IMU, `wait_quiet` returns at once, so use `pros::delay(80)` instead
(at least the sensor delay + 30 ms).

For a short time after a hit (see [VMP](vmp.md#localization-after-a-hit)), the filter
keeps running but its corrections aren't applied.

A reset is refused if it would move the robot more than `DISTANCE_RESET_MAX_JUMP`
inches (12 by default). That protects you from a sensor seeing a robot instead of a
wall.

## Field map

Goals, loaders and barriers sit in front of the walls. A sensor aimed past one reads
the element instead of the wall, and that pulls the position off. List those elements
in **`include/field_map.hpp`** and localization ignores any sensor whose beam would
pass near one before reaching the wall, both in the background filter and in wall
resets (which then return `Blocked`).

```cpp
map.margin = 2.0;                 // clearance around every obstacle, inches

map.circles = {                   // round things: {x, y, radius}
    {0.0, 0.0, 5.0},
    {21.0, 47.5, 4.0},
};
map.segments = {                  // straight things: {x1, y1, x2, y2}
    {-24.0, 0.0, 24.0, 0.0},
};
```

It ships **empty** (no effect) because the elements depend on the game. To fill it in:

1. **Use the same coordinates as your routes:** field center `(0, 0)`, +X right, +Y
   forward from your starting side.
2. **Take positions from the game manual's field drawings.** Each element's center
   and size, converted to inches from the field center.
3. **Round elements** (posts, round goals, goal legs) are circles. Give the radius of
   the part a sensor could actually hit.
4. **Long, straight elements** (barriers, the long side of a goal) are segments from
   one end to the other.
5. **Check on a real field** with a tape measure. An element that's a few inches off
   still works, because `margin` covers it.

Include every element that's **taller than your sensors are mounted**. Leave out
anything lower than them: the beam passes over it.

`localization::diagnostics().blocked` tells you how many sensors were skipped on the
last update, which is handy to check the map is doing something.

RCL-Tracking (a LemLib add-on) has a map for the game it was written for: loaders,
goal legs and a center goal. If that's your game, its numbers are a starting point,
but check them against your manual and field.

## Sensor confidence

Past 200 mm (about 8 in), a distance sensor also reports how sure it is of each
reading, from 0 to 63. Readings below `DISTANCE_MIN_CONFIDENCE` (60 by default, in
`robot_hardware.hpp`) are ignored, both by the background filter and by wall resets.
Low confidence usually means the beam hits the wall at an angle, hits something shiny,
or only half-hits an object. If good walls get ignored a lot, lower it a little; `0`
turns the check off.

## API

| Function | Does |
| --- | --- |
| `localization::start(chassis, config)` | Called in `initialize()` from `robot_hardware.hpp`'s settings |
| `localization::set_pose({x, y, theta})` | Set the robot's pose |
| `localization::getPose()` | Current pose |
| `localization::distance_reset(Sensor, average_ms)` | Snap to a wall with one sensor (`front`, `right`, `back`, `left`), optionally averaged |
| `localization::last_reset_result()` | Why the last reset was applied or refused (`reset_result_name()` turns it into text) |
| `localization::diagnostics()` | Estimate, particle spread, speed, sensors accepted, sensors blocked by the field map, ready flag |

## Tracking wheels

Tracking wheels improve odometry itself, so localization has less to fix. Set them
up in `robot_hardware.hpp` (see [Hardware configuration](hardware-config.md#tracking-wheels)).
Reaper creates them before `chassis.initialize()` and hands them to EZ-Template's
odometry. With tracking wheels, you can raise `chassis.odom_turn_bias_set()` in
`default_constants()`.
