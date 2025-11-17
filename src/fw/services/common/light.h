/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "shell/prefs.h"

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
//!
//! @internal
//! to be called when starting up to initialize variables correctly
void light_init(void);

//! @internal
//! to be called by the launcher on a button down event
void light_button_pressed(void);

//! @internal
//! to be called by the launcher on a button up event
void light_button_released(void);

//! @copydoc app_light_enable
void light_enable(bool enable);

//! @internal
//! light_enable that adheres to user's backlight setting.
void light_enable_respect_settings(bool enable);

//! @copydoc app_light_enable_interaction
//! if light_enable was called (backlight was forced on),
//! then do nothing
void light_enable_interaction(void);

//! Reset the state if an app overrode the usual state machine using light_enable()
void light_reset_user_controlled(void);

//! @internal
void light_toggle_enabled(void);

//! @internal
void light_toggle_ambient_sensor_enabled(void);

//! Switches for temporary disabling backlight (ie: low power mode)
void light_allow(bool allowed);

//! Get the current active backlight brightness as a percentage (0-100)
//! This returns the actual current brightness, which may differ from the
//! configured brightness when dynamic backlight is enabled.
uint8_t light_get_current_brightness_percent(void);

//!   @} // group Light
//! @} // group UI
