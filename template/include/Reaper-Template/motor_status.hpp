#pragma once

// [Reaper] Motor monitor data and the checks behind the Motors page colors (port conflicts, wrong device).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ez {
enum class MotorConnection { Pending, Connected, Disconnected, WrongDevice, ReadError, InvalidPort, Conflict, Stale };
struct MotorStatus {
  std::string name;
  int port = 0;
  MotorConnection state = MotorConnection::Pending;
  double temperature = NAN;
};
inline int physical_motor_port(int port) { return port >= -21 && port < 0 ? -port : port; }
inline std::vector<MotorStatus> configured_motors(std::vector<MotorStatus> entries,
                                               const std::vector<int>& sensor_ports = {}) {
  std::vector<MotorStatus> result;
  for (auto entry : entries) {
    if (entry.port == 0) continue;
    entry.port = physical_motor_port(entry.port);
    if (entry.port < 1 || entry.port > 21) entry.state = MotorConnection::InvalidPort;
    bool duplicate = false;
    for (auto& existing : result) {
      if (existing.port == entry.port) {
        existing.name += "/" + entry.name;
        existing.state = MotorConnection::Conflict;
        duplicate = true;
        break;
      }
    }
    if (!duplicate) result.push_back(entry);
  }
  for (auto& entry : result)
    for (int port : sensor_ports)
      if (port != 0 && physical_motor_port(port) == entry.port)
        entry.state = MotorConnection::Conflict;
  return result;
}
inline bool motor_configuration_error(MotorConnection state) {
  return state == MotorConnection::InvalidPort || state == MotorConnection::Conflict;
}
inline MotorConnection motor_reading_state(bool is_motor, bool empty_port, double temperature) {
  if (!is_motor) return empty_port ? MotorConnection::Disconnected : MotorConnection::WrongDevice;
  return std::isfinite(temperature) && temperature >= 0 && temperature <= 150
      ? MotorConnection::Connected : MotorConnection::ReadError;
}
inline void mark_motor_snapshot_stale(std::vector<MotorStatus>& entries) {
  for (auto& entry : entries) if (!motor_configuration_error(entry.state)) {
    entry.state = MotorConnection::Stale;
    entry.temperature = NAN;
  }
}
}
