/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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

//! @copydoc app_light_set_color_rgb888
//! rgb is a packed 0x00RRGGBB value (8 bits per channel). No-op on
//! platforms without a color backlight.
void light_set_color_rgb888(uint32_t rgb);

//! @copydoc app_light_set_system_color
//! No-op on platforms without a color backlight.
void light_set_system_color(void);

//! Request that the system color take precedence over any app override.
//! While the refcount is non-zero, the LED is forced to the user default
//! color even if an app has set an override. Used by notifications and
//! other modals so they display in neutral white without permanently
//! clearing the underlying app's color.
void light_system_color_request(void);

//! Release a system color request. When the refcount returns to zero, any
//! app override is re-applied.
void light_system_color_release(void);

//! @internal
void light_toggle_enabled(void);

//! @internal
void light_toggle_ambient_sensor_enabled(void);

//! @internal
void light_toggle_dynamic_intensity_enabled(void);

//! Switches for temporary disabling backlight (ie: low power mode)
void light_allow(bool allowed);

//! Get the current active backlight brightness as a percentage (0-100)
//! This returns the actual current brightness, which may differ from the
//! configured brightness when dynamic backlight is enabled.
uint8_t light_get_current_brightness_percent(void);

//! @return true if the backlight is currently on in any form (on, timed, or
//! fading out). Returns false only when the backlight is fully off.
bool light_is_on(void);

//!   @} // group Light
//! @} // group UI
