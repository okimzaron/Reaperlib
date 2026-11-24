# Troubleshooting

## Building and uploading

| Problem | Fix |
| --- | --- |
| `pros` command not found | Use the PROS terminal from the VS Code sidebar, or reinstall the PROS extension. |
| Build errors right after unzipping | Make sure the folder you opened contains `project.pros`. Run `pros make clean`, then `pros make`. |
| Strange build errors in a OneDrive folder | Move the project out of OneDrive (or any synced folder). |
| `static_assert` about IMU ports | An IMU port is outside 0–21, or both IMUs use the same port. |
| Upload can't find the brain | Try another cable or USB port, and close anything else using the brain (such as VEXcode or another terminal). |

## Robot

| Problem | Fix |
| --- | --- |
| Nothing moves | The ports are probably still `0`. Set them in `robot_hardware.hpp`. |
| Robot spins when you push forward | One side is reversed. Flip the signs of that side's ports. |
| One motor fights the others | That single motor's sign is wrong. |
| Turns overshoot or wobble | Tune `pid_turn_constants_set` in `src/autons.cpp`. Follow EZ-Template's tuning guide. |
| Heading goes the wrong way | The IMU is mounted upside down or sideways. Mount it flat. |
| Controller rumbles `---` at startup | The IMU didn't calibrate. Check its port and cable, and keep the robot still during the splash. |
| A motor shows red on the Motors page | `OFF`: unplugged. `TYPE`: wrong device on that port. `USED`: two devices set to the same port. `PORT`: invalid number. |
| Can't select a route | The robot is enabled by a field controller, or a route is running. Disable it first. |
| Down + B does nothing | It only works with **no** field controller or competition switch connected. |
| Joystick feels different after pressing Left, Right, Y or A | Those buttons also change EZ-Template's joystick curve. See [Driver controls](controls.md#button-map). |

## Lift startup messages

Shown on the controller's third line and in the terminal as `[Lift] ...`.

| Message | Cause | Fix |
| --- | --- | --- |
| `L: ports off` | Both lift ports are `0` | Normal if you have no lift |
| `L: check ports` | Only one port set, both the same, or one above 21 | Fix `LIFT_LEFT_PORT` / `LIFT_RIGHT_PORT` |
| `L: left missing` / `L: right absent` | No motor answers on that port | Check the cable and port number |
| `L: left units`, `L: left brake`, `L: left encoder` (or the right-side versions) | The motor answered, but a setup command failed | Reseat the cable and try again |
| `L: settings` | A value in `lift_settings.hpp` is invalid (negative, power above 127, gains out of order...) | Check against the table in [Lift](lift.md#main-settings) |
| `L: preset` | A preset is outside 0…`max_height` | Fix the preset or set `max_height` |
| `L: rot config` | Rotation sensor port is taken or invalid, or the conversion factor is 0 | Use a free port and a non-zero factor |
| `L: rot sensor` | The rotation sensor didn't give a valid reading | Check its cable |
| `L: task failed` | The lift task couldn't start | Rare: restart the brain |
| `L: FAULT` (while running) | Sensor lost, or the two motors disagree | See [Lift faults](lift.md#faults) |

Once you've fixed the cause, put the lift at the bottom and press **Left** (with R2/L2
released) to retry, or restart the program.

## Lift behavior

| Problem | Fix |
| --- | --- |
| Lift won't go up in a `move_to` | `max_height` is still 0, or the target is above it. |
| Lift goes up forever with R2 | `allow_unbounded_jog` is still `true` (calibration mode). Set `max_height` and turn it off. |
| Lift sags when holding | Add gravity compensation (`c.gravity`, `c.kg`). |
| Heights change from one run to the next | The lift wasn't at the bottom when the program started. Zero is set at startup. |
| Lift faults while lifting heavy loads | The motors are flexing apart under load. Raise `encoder_disagreement`, or tighten the lift. |

## Getting help

When you report a problem, include:

- The Reaper version (`version` file, or the first `[Reaper]` line in the terminal)
- The PROS kernel version (`pros conduct info-project`)
- What you expected and what actually happened
- The terminal output
