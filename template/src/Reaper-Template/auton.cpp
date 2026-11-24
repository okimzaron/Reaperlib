/*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "Reaper-Template/api.hpp"

#include <cstddef>

namespace {
std::string trim_leading_newlines(std::string text) {
  while (!text.empty() && (text.front() == '\n' || text.front() == '\r')) {
    text.erase(text.begin());
  }
  return text;
}
}  // namespace

ez::Auton::Auton() : Name(""), Description(""), auton_call(nullptr) {}

ez::Auton::Auton(std::string display, std::function<void()> callback)
    : Name(""), Description(""), auton_call(callback) {
  const std::size_t split = display.find('\n');
  if (split == std::string::npos) {
    Name = std::move(display);
    return;
  }

  Name = display.substr(0, split);
  Description = trim_leading_newlines(display.substr(split + 1));
}

ez::Auton::Auton(std::string name, std::string description,
                 std::function<void()> callback)
    : Name(std::move(name)),
      Description(std::move(description)),
      auton_call(std::move(callback)) {}
