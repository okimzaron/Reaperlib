#pragma once
#include "Reaper-Template/motor_status.hpp"
namespace motor_monitor {
bool start();
std::vector<ez::MotorStatus> snapshot();
}
