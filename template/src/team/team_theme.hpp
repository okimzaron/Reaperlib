#pragma once
#include <cstdint>
#include "liblvgl/lvgl.h"

// TEAM BRANDING FOR THE BRAIN DASHBOARD -- everything team-specific lives in this folder (src/team/).
//
// This folder ships filled in for 4588G Gambit, as a worked example: this file plus gambit_logo.cpp.
// To make it yours, replace the name, colors and logo below. Edit the SETTINGS section, then rebuild/upload.
// Nothing outside src/team/ needs changing.
// The startup splash is Reaper's and stays as it is; everything after it (Home, Routes, Motors, Sensors)
// uses the name, logo and colors below. Step-by-step guide: https://reaperlib.online/docs/#brain-screen.
//
// Colors are 0xRRGGBB hex, the same codes any online color picker gives you (drop the '#').

// The logo image. It is declared extern "C" because the LVGL image converter produces a C (.c) file.
// To use your own logo, change gambit_logo here and in LOGO below to your image's name.
extern "C" {
LV_IMAGE_DECLARE(gambit_logo);  // src/team/gambit_logo.cpp
}

namespace team_theme {

// ========================== SETTINGS ==========================
// Team name. Shown on the Home page when there is no logo.
inline constexpr const char* TEAM_NAME = "4588G GAMBIT";

// Team logo on the Home page, centered at the top. Up to about 400 x 200 pixels. nullptr = show TEAM_NAME.
// To use your own: convert it with the LVGL image converter (see https://reaperlib.online/docs/#brain-screen), put the file in
// src/team/, and change "gambit_logo" to its name here and in the extern "C" block above.
inline const lv_image_dsc_t* const LOGO = &gambit_logo;

// Page colors
inline constexpr std::uint32_t PAGE = 0x522D73;            // page backgrounds
inline constexpr std::uint32_t CARD = 0x3A1F52;            // route buttons, motor/sensor cards
inline constexpr std::uint32_t CARD_PRESSED = 0x6D4794;    // a card or button while pressed
inline constexpr std::uint32_t TEXT = 0xFEFAE0;            // text, selected route, and the left nav bar
inline constexpr std::uint32_t TEXT_PRESSED = 0xDDD3A6;    // selected route while pressed
inline constexpr std::uint32_t NAV_BUTTON = 0xEFE7C4;      // nav buttons that aren't selected
inline constexpr std::uint32_t BORDER = 0xA58BC2;          // card and button outlines

// Status colors (motor icons, sensor card borders, warnings)
inline constexpr std::uint32_t STATUS_OK = 0x5ABC03;       // motor OK, IMU still
inline constexpr std::uint32_t STATUS_BAD = 0xF05D5D;      // unplugged, wrong device, port used twice
inline constexpr std::uint32_t STATUS_CAUTION = 0xF0B84A;  // read error, IMU moving, warm motor
inline constexpr std::uint32_t STATUS_WAITING = 0x888888;  // not read yet / warming up
inline constexpr std::uint32_t STATUS_HOT = 0xFF8A80;      // hot motor, active warning text

// Alliance button on the Routes page
inline constexpr std::uint32_t ALLIANCE_RED = 0xB83232;
inline constexpr std::uint32_t ALLIANCE_RED_PRESSED = 0x8F2525;
inline constexpr std::uint32_t ALLIANCE_BLUE = 0x245DAD;
inline constexpr std::uint32_t ALLIANCE_BLUE_PRESSED = 0x1B4780;
// ======================== END SETTINGS ========================

}  // namespace team_theme
