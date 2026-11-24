/*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <functional>
#include <string>

namespace ez {
class Auton {
 public:
  Auton();

  // Backwards-compatible constructor. If the display string contains newlines,
  // the first line becomes the title and the remaining text becomes the
  // description used by the touch selector.
  Auton(std::string display, std::function<void()> callback);

  // Preferred constructor for the Reaper touch selector.
  Auton(std::string name, std::string description, std::function<void()> callback);

  std::string Name;
  std::string Description;
  std::function<void()> auton_call;
};
}  // namespace ez
