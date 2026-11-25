#pragma once

// [Reaper] Controller screen: live X/Y, heading, lift position and VMP warnings.
namespace controller_ui {
// Call once after initialization. Displays pose (inches/degrees) and the
// measured lift PID position (motor degrees) in every competition mode.
bool start();
}
