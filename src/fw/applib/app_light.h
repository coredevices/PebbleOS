/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @file light.h
//! @addtogroup UI
//! @{
//!   @addtogroup Light Light
//! \brief Controlling Pebble's backlight
//!
//! The Light API provides you with functions to turn on Pebble’s backlight or
//! put it back into automatic control. You can trigger the backlight and schedule a timer
//! to automatically disable the backlight after a short delay, which is the preferred
//! method of interacting with the backlight.
//!   @{

//! Trigger the backlight and schedule a timer to automatically disable the backlight
//! after a short delay. This is the preferred method of interacting with the backlight.
void app_light_enable_interaction(void);

//! Turn the watch's backlight on or put it back into automatic control.
//! Developers should take care when calling this function, keeping Pebble's backlight on for long periods of time
//! will rapidly deplete the battery.
//! @param enable Turn the backlight on if `true`, otherwise `false` to put it back into automatic control.
void app_light_enable(bool enable);

//!   @} // group Light
//! @} // group UI
