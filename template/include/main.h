/**
 * \file main.h
 *
 * Contains common definitions and header files used throughout your PROS
 * project.
 *
 * Copyright (c) 2017-2021, Purdue University ACM SIGBots.
 * All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _PROS_MAIN_H_
#define _PROS_MAIN_H_

#define PROS_USE_SIMPLE_NAMES

#define PROS_USE_LITERALS

#include <atomic>
#include <cmath>
#include <cstdint>

#include "api.h"

#include "Reaper-Template/api.hpp"
#include "Reaper-Template/controller_ui.hpp"
#include "Reaper-Template/reaper_splash.hpp"
#include "Reaper-Template/localization.hpp"
#include "Reaper-Template/roller.hpp"
#include "Reaper-Template/vmp.hpp"
#include "Reaper-Template/vmp_settings.hpp"

#include "Reaper-Template/subsystems.hpp"
#include "robot_controls.hpp"
#include "Reaper-Template/lift.hpp"
#include "Reaper-Template/motor_monitor.hpp"
#include "team_theme.hpp"
#include "autons.hpp"

using namespace ez::literals;

#ifdef __cplusplus
extern "C" {
#endif
void autonomous(void);
void initialize(void);
void disabled(void);
void competition_initialize(void);
void opcontrol(void);
#ifdef __cplusplus
}
#endif

#ifdef _USE_EZ_NAMESPACE_
#warning "[Reaper-Template] Automatic 'using namespace ez;' from headers is DEPRECATED. Use 'ez::' or opt in manually in your source files."
using namespace ez;
#endif

#endif  // _PROS_MAIN_H_
