# Credits

Reaper is created and maintained by **Za'Ron Lee** (4588G Gambit).

Reaper would not exist without these open-source projects. Each one keeps its own
license. The full texts are in [`licenses/`](licenses/).

| Project | Author | License | What Reaper uses |
| --- | --- | --- | --- |
| [EZ-Template](https://github.com/EZ-Robotics/EZ-Template) | EZ-Robotics | MPL-2.0 | The base of the template (version 4.0.0-beta.4): drive, PID, odometry, pure pursuit, exit conditions, auton selector, pistons, SD card and utilities under `Reaper-Template/`. Reaper's changes are applied on top. → [`licenses/EZ-Template.txt`](licenses/EZ-Template.txt) |
| EZ-Units | EZ-Robotics, from Benjamin Jurke's 2015 work and OkapiLib | MPL-2.0 | Unit types (`_in`, `_deg`, `_ms`) in `include/EZ-Units/`. → [`licenses/EZ-Units.txt`](licenses/EZ-Units.txt) |
| RW-Template | Richard Wang | MIT | The particle filter (Monte Carlo localization) and distance reset math in `Reaper-Template/rw/`. → [`licenses/RW-Template.txt`](licenses/RW-Template.txt) |
| [PROS](https://github.com/purduesigbots/pros) | Purdue SIGBots | MPL-2.0 | The kernel 4.2.2: `include/pros/`, `firmware/`, `common.mk`. Includes FreeRTOS (MIT). → [`licenses/PROS.txt`](licenses/PROS.txt) |
| [OkapiLib](https://github.com/purduesigbots/OkapiLib) | Purdue SIGBots | MPL-2.0 | The units design EZ-Units is derived from. |
| [LVGL](https://github.com/lvgl/lvgl) | LVGL Kft | MIT | Graphics library behind the brain screen splash and dashboard. → [`licenses/LVGL.txt`](licenses/LVGL.txt) |
| [LemLib](https://github.com/LemLib/LemLib) | Liam Teale and LemLib contributors | MIT | The `pros-build` GitHub Action and the PR comment workflow in `.github/workflows/`. → [`licenses/LemLib.txt`](licenses/LemLib.txt) |
| [nightly.link](https://github.com/oprypin/nightly.link) | Oleh Prypin | Public domain snippet | Part of `.github/workflows/pr-edit.yml`. |

## Reaper's own work

Reaper's additions on top of EZ-Template include: the hardware configuration
system, the lift controller and sequences, piston groups, localization integration,
VMP, the motor monitor, the controller screen, the scythe splash animation, the
touch dashboard, the manual auton runner, the kill-safe locking rules and the
reworked stuck detection.

The scythe splash art and the 4588G Gambit logo belong to 4588G Gambit. If you use
Reaper for your own team, swap in your own logo (see
[Brain screen](docs/brain-screen.md)).

## How licensing works here

- **MPL-2.0 is per file.** A file that came from EZ-Template or PROS keeps its
  MPL-2.0 header, stays under MPL-2.0 if you change it, and its source has to stay
  available to anyone you share your code with. Your own new files can use any
  license, but Reaper ships everything under MPL-2.0 for simplicity.
- **MIT code** (RW-Template, LVGL, LemLib) can be used freely, as long as its
  copyright notice and license text stay with it.
- **If you fork Reaper,** keep `LICENSE.md`, `CREDITS.md`, the `licenses/` folder
  and the license header at the top of each file.

This is a plain-language summary, not legal advice. The license texts are the final word.
