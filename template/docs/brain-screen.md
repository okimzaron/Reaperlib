# Brain screen

## Startup splash

When the program starts, a 3-second scythe animation plays over the screen: the
swing, the strike, glass cracking, and the shards falling away to show the dashboard.
It covers the IMU calibration that happens at the same time.

- The splash never cuts calibration short. It only covers it.
- It replaces EZ-Template's IMU loading bar (`chassis.initialize(false)`).
- To change the total length, call `reaper_splash::min_duration_set(ms)` **before**
  `reaper_splash::start()` in `initialize()`. The minimum is 1620 ms.
- To turn the splash off, remove `reaper_splash::start();` from `initialize()`.

**Keep the robot still until the splash is done.** The controller rumbles once when
the IMU is good, and three long rumbles if it failed.

## Dashboard

After the splash, the brain shows a dashboard with four buttons on the left:

| Button | Page | What's on it |
| --- | --- | --- |
| ⌂ | **Home** | Team logo and the selected route |
| **A** | **Routes** | Pick a route: **Main Match** (up to 15) or **Skills Match** (up to 3), plus the **RED / BLUE** alliance button |
| **M** | **Motors** | Every configured motor's connection and temperature |
| **S** | **Sensors** | IMU stillness, shake, hits, tilt and lift sway (see [VMP](vmp.md)) |

### Routes page

- Tap a route to select it. The status line shows `Selected: Match / <name>`.
- While a route is running it shows `Running: ...`, and selecting is locked.
- Selecting is also locked while a field controller has the robot enabled. Pick your
  route while the robot is disabled.
- The **RED / BLUE** button sets the alliance color the roller looks for (see
  [Mechanisms](mechanisms.md#roller)).
- Taps closer than 120 ms apart are ignored, so a bouncy touch can't select twice.

### Motors page

Each card shows a motor's name, port, status and temperature. When a roller optical
sensor is configured, the line under the title becomes a live color readout (see
[Mechanisms](mechanisms.md#roller)).

Status meanings:

| Status | Color | Meaning |
| --- | --- | --- |
| `OK` | Green | Motor detected and reading normally |
| `OFF` | Red | Nothing plugged into that port |
| `TYPE` | Red | Something other than a motor is plugged in there |
| `PORT` | Red | The port number in `robot_hardware.hpp` isn't valid |
| `USED` | Red | Two devices are set to the same port |
| `READ` | Amber | Motor found, but the reading looks wrong |
| `STALE` | | The readings stopped updating |
| `WAIT` | | Not checked yet |

Motors with port `0` aren't shown. Use the arrows to page through them if there
are more than fit on screen.

## Making the dashboard yours

The startup splash is Reaper's and stays the same for every team. Everything after
it (Home, Routes, Motors, Sensors) takes its team name, logo and colors from **one
folder, `src/team/`**:

```
src/team/
  team_theme.hpp     <- team name, which logo, every dashboard color
  gambit_logo.cpp    <- 4588G Gambit's logo (the example; replace with yours)
```

It ships filled in for 4588G Gambit, so you can see a finished setup. Replace those
values with yours, rebuild, and upload. Nothing outside `src/team/` needs changing.

### Team name

```cpp
inline constexpr const char* TEAM_NAME = "1234A ROBOTS";
```

The name shows on the Home page when there's no logo (see below).

### Colors

Every color is a hex code `0xRRGGBB`, the six-digit code any online color picker
gives you, written with `0x` in front instead of `#`. For example, `#1E90FF` becomes
`0x1E90FF`.

| Setting | Where it shows |
| --- | --- |
| `PAGE` | Background of every page |
| `CARD` | Route buttons, motor and sensor cards |
| `CARD_PRESSED` | A card or button while your finger is on it |
| `TEXT` | All text, the selected route, and the bar of nav buttons on the left |
| `TEXT_PRESSED` | The selected route while pressed |
| `NAV_BUTTON` | Nav buttons (⌂ A M S) that aren't selected |
| `BORDER` | Card and button outlines |
| `STATUS_OK` | Green: motor OK, IMU still |
| `STATUS_BAD` | Red: unplugged, wrong device, port used twice |
| `STATUS_CAUTION` | Amber: read error, IMU moving, motor warm |
| `STATUS_WAITING` | Grey: not read yet, IMU warming up |
| `STATUS_HOT` | Hot motor temperature and active warning text |
| `ALLIANCE_RED`, `ALLIANCE_BLUE` (+ `_PRESSED`) | The RED / BLUE button on the Routes page |

Tips:
- **Keep `TEXT` and `PAGE` far apart** (light on dark or dark on light), or the text
  becomes hard to read on a brain screen in a bright venue.
- **`TEXT` is also the nav bar color**, so the selected nav button uses `PAGE` and the
  others use `NAV_BUTTON`. Pick a `NAV_BUTTON` close to `TEXT`.
- **Leave the status colors as green / red / amber** unless you have a reason not to.
  People read those colors without thinking.

### Logo

1. **Make the image.** A PNG up to about **400 × 200** pixels; 200 × 200 fits best.
   The page background shows through transparent areas.
2. **Convert it.** Open the [LVGL image converter](https://lvgl.io/tools/imageconverter),
   choose **LVGL v9**, color format **RGB565** (or **ARGB8888** to keep
   transparency), and give it a name with no spaces, such as `my_team_logo`. Download
   the `.c` file.
3. **Add it to the project.** Put the `.c` file in `src/team/`. Near the top, the
   converter includes LVGL with a few `#if` / `#include "lvgl.h"` lines. Replace those
   lines with the one line PROS needs:

   ```c
   #include "liblvgl/lvgl.h"
   ```
4. **Point the theme at it.** In `src/team/team_theme.hpp`, change `gambit_logo` to
   your image's name in both places:

   ```cpp
   extern "C" {
   LV_IMAGE_DECLARE(my_team_logo);
   }
   ...
   inline const lv_image_dsc_t* const LOGO = &my_team_logo;
   ```

   (`extern "C"` is there because the converter makes a C file. Leave it in.)

   The logo is centered at the top of the Home page automatically.

**No logo:** set `LOGO = nullptr;` and the Home page shows `TEAM_NAME` instead.

Once your own logo works, delete `src/team/gambit_logo.cpp`. That also makes every
upload a little smaller.

The Gambit logo is two-tone, so it's recolored to your `TEXT` and `PAGE` colors
automatically. A logo you convert yourself is shown in its own colors.
