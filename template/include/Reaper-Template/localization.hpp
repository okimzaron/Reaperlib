#pragma once
#include "Reaper-Template/localization_core.hpp"
#include "Reaper-Template/drive/drive.hpp"
namespace localization {
// Call once, after chassis.initialize(). Port 0 disables a distance sensor.
bool start(ez::Drive& chassis, const Config& config);
// Reset the actual EZ pose. Filter automatically recenters on the next sample.
void set_pose(Pose pose);
Pose getPose();
Diagnostics diagnostics();

// Snap one axis of the position to a wall seen by `sensor`. The robot must be stopped (vmp::wait_quiet() is
// the easy way). False means rejected; last_reset_result() says why.
//
// average_ms = 0 uses a single reading. Above 0, readings are collected for that long (at the sensor's update
// rate, ~35 ms each) and the median is used, which is steadier; 150 is a good value. The reset is refused if
// the readings disagree by more than an inch (something moved in front of the sensor) or the robot moved.
bool distance_reset(Sensor sensor, std::uint32_t average_ms = 0);

enum class ResetResult {
  None,           // no reset attempted yet
  Applied,        // position corrected
  NotRunning,     // localization not started, or no distance sensors configured
  NoReading,      // sensor unplugged, out of range, or every reading failed the confidence check
  TooFewReadings, // averaging got fewer than 3 good readings
  Unsteady,       // averaged readings spread over 1 inch: something moved in front of the sensor
  RobotMoved,     // the robot moved while readings were being collected
  Blocked,        // a field element in include/field_map.hpp is between the sensor and the wall
  Refused,        // reading not square to a wall, robot not settled, or jump over DISTANCE_RESET_MAX_JUMP
};
ResetResult last_reset_result();
const char* reset_result_name(ResetResult result);
}  // namespace localization
