#include "main.h"
#include "Reaper-Template/controller_ui.hpp"
#include "Reaper-Template/vmp_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace controller_ui {
namespace {
pros::task_t worker = nullptr;


// Exactly 15 visible columns, padded to erase old longer readings. Never
// silently truncate a number: values outside the display range show RANGE.
void line_format(char (&out)[16], int row, const ez::Drive::LocalizationSample& pose,
                 const lift::Status& lift, const vmp::Warning& warning) {
  char text[80] = {};
  if (row == 1 && warning.alert == vmp::Alert::tipping) {
    std::snprintf(text, sizeof(text), "!TIPPING %.0fdeg", warning.tilt_deg);  // replaces the heading line
  } else if (row == 1 && warning.alert == vmp::Alert::sway) {
    std::snprintf(text, sizeof(text), "!LIFT SWAY");
  } else if (row == 0) {
    if (!pose.ready || !std::isfinite(pose.position.x) || !std::isfinite(pose.position.y))
      std::snprintf(text, sizeof(text), "X:--- Y:---");
    else if (pose.position.x < -999.9 || pose.position.x > 999.9 ||
             pose.position.y < -999.9 || pose.position.y > 999.9)
      std::snprintf(text, sizeof(text), "XY: RANGE");
    else std::snprintf(text, sizeof(text), "X%6.1f Y%6.1f", pose.position.x, pose.position.y);
  } else if (row == 1) {
    if (!pose.ready || !std::isfinite(pose.position.theta))
      std::snprintf(text, sizeof(text), "H:--- deg");
    else if (std::abs(pose.position.theta) > 99999.9)
      std::snprintf(text, sizeof(text), "H: RANGE");
    else std::snprintf(text, sizeof(text), "H:%.1f deg", pose.position.theta);
  } else {
    if (const char* error = lift::startup_message()) std::snprintf(text, sizeof(text), "%s", error);
    else if (lift.mode == lift::Mode::Fault || !std::isfinite(lift.position))
      std::snprintf(text, sizeof(text), "L: FAULT");
    else if (std::abs(lift.position) > 99999.9)
      std::snprintf(text, sizeof(text), "L: RANGE");
    else std::snprintf(text, sizeof(text), "L:%.1f mdeg", lift.position);
  }
  std::snprintf(out, sizeof(out), "%-15.15s", text);
}

void run(void*) {
  // One line per 100 ms, so the whole display refreshes about three times/s.
  // Controller I/O stays out of the 10 ms driver and lift control loops.
  char last[3][16] = {};
  uint32_t last_sent[3] = {};
  int row = 0;
  auto pose = chassis.localization_sample();
  auto lift_state = lift::get_status();
  auto warning = vmp::warning();
  vmp::Alert last_alert = vmp::Alert::none;
  while (true) {
    if (master.is_connected() == 1) {
      if (row == 0) {
        pose = chassis.localization_sample(); // coherent locked pose snapshot
        lift_state = lift::get_status();
      }
      warning = vmp::warning();  // every pass, so a tip warning shows within ~100 ms
      // A new tip / sway warning rumbles once. The rumble uses this pass's controller message slot.
      if (warning.alert != vmp::Alert::none && warning.alert != last_alert && vmp::settings::WARN_RUMBLE) {
        last_alert = warning.alert;
        master.rumble(warning.alert == vmp::Alert::tipping ? "---" : "-");
        pros::delay(100);
        continue;
      }
      last_alert = warning.alert;
      char text[16];
      line_format(text, row, pose, lift_state, warning);
      const auto now = pros::millis();
      if (std::strcmp(text, last[row]) != 0 || now - last_sent[row] >= 1000) {
        if (master.set_text(row, 0, text) == 1) {
          std::memcpy(last[row], text, sizeof(text));
          last_sent[row] = now;
        } // failed writes are retried on the next pass
      }
      row = (row + 1) % 3;
    } else {
      std::memset(last, 0, sizeof(last)); // repaint all lines after reconnect
      row = 0;
    }
    pros::delay(100);
  }
}
}

bool start() {
  if (worker) return true;
  chassis.opcontrol_curve_display_toggle(false);
  worker = pros::c::task_create(run, nullptr, TASK_PRIORITY_DEFAULT,
                               TASK_STACK_DEPTH_DEFAULT, "Controller UI");
  if (!worker) chassis.opcontrol_curve_display_toggle(true);
  return worker != nullptr;
}
}
