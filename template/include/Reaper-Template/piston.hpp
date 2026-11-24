/*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "api.h"
#include <atomic>
#include <cstdint>
#include "pros/misc.h"

namespace ez {
class Piston {
public:
  /**
   * Piston used throughout.
   */
  pros::adi::DigitalOut piston;

  /**
   * Piston constructor.
   *
   * The starting position of your piston defaults to false.
   *
   * \param input_port
   *        the ADI port of your piston
   * \param default_state
   *        level the solenoid's pin starts at.  get() and set() are relative to it: get() is false at startup,
   *        and set(true) moves the piston to the opposite of default_state
   */
  Piston(int input_port, bool default_state = false);

  /**
   * Piston constructor in 3 wire expander.
   *
   * The starting position of your piston defaults to false.
   *
   * \param input_port
   *        the ADI port of your piston on the expander
   * \param expander_smart_port
   *        the smart port the 3 wire expander is plugged into
   * \param default_state
   *        level the solenoid's pin starts at.  get() and set() are relative to it: get() is false at startup,
   *        and set(true) moves the piston to the opposite of default_state
   */
  Piston(int input_port, int expander_smart_port, bool default_state = false);

  /**
   * Sets the piston to the input.
   *
   * \param input
   *        true sets to the opposite of the starting position
   */
  void set(bool input);

  /**
   * Returns current piston state.
   */
  bool get();
  std::uint32_t revision() const { return revision_.load(); }
  bool command_succeeded() const { return command_ok_.load(); }

  /**
   * One button toggle for the piston.
   *
   * \param toggle
   *        an input button
   */
  void button_toggle(int toggle);

  /**
   * One button toggle for the piston.
   *
   * \param toggle
   *        an input button
   */
  void button_toggle(pros::controller_digital_e_t toggle);

  /**
   * Two buttons trigger the piston.  Active is enabled, deactive is disabled.
   *
   * \param active
   *        sets piston to true
   * \param deactive
   *        sets piston to false
   */
  void buttons(int active, int deactive);

  /**
   * Two buttons trigger the piston. Active is enabled, deactive is disabled.
   *
   * \param active
   *        sets piston to true
   * \param deactive
   *        sets piston to false
   */
  void buttons(pros::controller_digital_e_t active, pros::controller_digital_e_t deactive);

private:
  bool reversed = false;
  std::atomic<bool> current{false};
  std::atomic<std::uint32_t> revision_{0};
  std::atomic<bool> command_ok_{true};
  int last_press = 0;
};
};  // namespace ez
