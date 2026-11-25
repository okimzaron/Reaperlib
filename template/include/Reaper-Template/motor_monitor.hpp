#pragma once

// [Reaper] Motor monitor: checks every motor in robot_hardware.hpp twice a second (plugged in, right
// device, temperature) for the brain's Motors page. Only configured motors are listed.
#include "Reaper-Template/motor_status.hpp"
namespace motor_monitor {
bool start();
std::vector<ez::MotorStatus> snapshot();
}
