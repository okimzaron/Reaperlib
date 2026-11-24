#pragma once
#include "robot_hardware.hpp"

// Compatibility aliases: existing routes and controls use the same owned objects.
// Change ports/configuration only in robot_hardware.hpp, not here.
inline ez::Drive& chassis = robot.chassis;
inline pros::Motor& LiftL = robot.lift_left;
inline pros::Motor& LiftR = robot.lift_right;
inline pros::Motor& claw = robot.claw;
