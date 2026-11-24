#pragma once

// Measured VEX omni wheel diameters, in inches. The nominal size on the box is not the real rolling diameter,
// and odometry distance is only as accurate as this number. Use these for DRIVE_WHEEL_DIAMETER and
// tracking-wheel diameters in robot_hardware.hpp, e.g. `DRIVE_WHEEL_DIAMETER = ez::wheel::NEW_325;`.
//
// NEW = current (2022+) VEX omni wheels, OLD = the previous generation. HALF = the "half-cut" anti-static
// variants. Values from LemLib (MIT licence, see licenses/LemLib.txt).
namespace ez::wheel {
inline constexpr double NEW_2 = 2.125;
inline constexpr double NEW_275 = 2.75;
inline constexpr double OLD_275 = 2.75;
inline constexpr double NEW_275_HALF = 2.744;
inline constexpr double OLD_275_HALF = 2.74;
inline constexpr double NEW_325 = 3.25;
inline constexpr double OLD_325 = 3.25;
inline constexpr double NEW_325_HALF = 3.246;
inline constexpr double OLD_325_HALF = 3.246;
inline constexpr double NEW_4 = 4.0;
inline constexpr double OLD_4 = 4.18;
inline constexpr double NEW_4_HALF = 3.995;
inline constexpr double OLD_4_HALF = 4.175;
}  // namespace ez::wheel
