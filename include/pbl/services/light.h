/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "shell/prefs.h"

/**
 * @defgroup services_light Backlight
 * @ingroup services
 * @brief Backlight state machine.
 *
 * Buttons and touches keep the backlight on while held and start a timed fade-out once released.
 * Whether it turns on at all depends on the user's backlight settings and, with the ambient light
 * sensor enabled, on the ambient light level. Apps can force the light on or off, and set its
 * color on platforms with a color backlight. Entities shared with the app SDK are documented in
 * the SDK's Light group.
 */

/**
 * @addtogroup UI
 * @{
 */

/**
 * @addtogroup Light Light
 * @brief Controlling Pebble's backlight
 *
 * The Light API provides you with functions to turn on Pebble’s backlight or
 * put it back into automatic control. You can trigger the backlight and schedule a timer
 * to automatically disable the backlight after a short delay, which is the preferred
 * method of interacting with the backlight.
 * @{
 */

/**
 * @ingroup services_light
 * @brief Initialize the backlight service at startup.
 */
void light_init(void);

/**
 * @ingroup services_light
 * @brief Handle a button press.
 *
 * Turns the backlight on, if allowed, until all buttons are released. Called by the launcher on
 * button down.
 */
void light_button_pressed(void);

/**
 * @ingroup services_light
 * @brief Handle a button release.
 *
 * Starts the fade-out timer once no button is held, unless the light is forced on. Called by the
 * launcher on button up.
 */
void light_button_released(void);

/**
 * @ingroup services_light
 * @brief Handle a touch finger-down.
 *
 * Acts like a button press; repeated calls before light_touch_up() count once.
 */
void light_touch_down(void);

/**
 * @ingroup services_light
 * @brief Release the hold taken by light_touch_down().
 *
 * Called on liftoff and on app teardown. No-op if no touch is holding the backlight.
 */
void light_touch_up(void);

/**
 * @brief Turn the watch's backlight on or put it back into automatic control.
 *
 * Developers should take care when calling this function, keeping Pebble's backlight on for long
 * periods of time will rapidly deplete the battery. Ignores the user's backlight settings; see
 * light_enable_respect_settings().
 *
 * @param enable Turn the backlight on if `true`, otherwise `false` to turn it off and put it back
 *               into automatic control. While buttons are held, the light stays on until they
 *               are released.
 */
void light_enable(bool enable);

/**
 * @ingroup services_light
 * @brief light_enable() that respects the user's backlight settings.
 *
 * When enabling, the light only turns on if the settings and ambient light allow it.
 *
 * @param enable true to force the light on, false to turn it off.
 */
void light_enable_respect_settings(bool enable);

/**
 * @brief Trigger the backlight and schedule a timer to automatically disable the backlight
 * after a short delay.
 *
 * This is the preferred method of interacting with the backlight. Does nothing while buttons are
 * held or the backlight was forced on with light_enable(), and the light only turns on if the
 * user's settings and the ambient light allow it.
 */
void light_enable_interaction(void);

/**
 * @ingroup services_light
 * @brief Return to the normal state machine after an app forced the light with light_enable().
 *
 * Also releases a touch hold that never saw its liftoff.
 */
void light_reset_user_controlled(void);

/**
 * @brief Tint the backlight LED to a packed 24-bit RGB value.
 *
 * The override lasts until light_set_system_color() is called; the app manager calls it on app
 * exit. While light_system_color_request() is held, the user's default color is shown instead.
 * No-op on platforms without a color backlight.
 *
 * @param rgb Packed 0x00RRGGBB value; 8 bits per channel. High byte ignored.
 */
void light_set_color_rgb888(uint32_t rgb);

/**
 * @brief Restore the backlight to the user's default color.
 *
 * Clears the override set with light_set_color_rgb888(). No-op on platforms without a color
 * backlight.
 */
void light_set_system_color(void);

/**
 * @ingroup services_light
 * @brief Make the system color take precedence over any app color.
 *
 * Reference counted: while held, the LED shows the user's default color even if an app set an
 * override. Used by notifications and other modals to show neutral white without clearing the
 * app's color. No-op on platforms without a color backlight.
 */
void light_system_color_request(void);

/**
 * @ingroup services_light
 * @brief Release a light_system_color_request().
 *
 * When the count drops to zero, any app color is applied again.
 */
void light_system_color_release(void);

/**
 * @ingroup services_light
 * @brief Toggle the user's backlight enabled setting.
 *
 * Also clears a forced-on state and briefly shows the result.
 */
void light_toggle_enabled(void);

/**
 * @ingroup services_light
 * @brief Toggle the user's ambient light sensor setting.
 */
void light_toggle_ambient_sensor_enabled(void);

#ifdef CONFIG_DYNAMIC_BACKLIGHT
/**
 * @ingroup services_light
 * @brief Set the dynamic backlight mode.
 *
 * Briefly turns the light on so the user sees the effect.
 *
 * @param mode New mode.
 */
void light_set_dynamic_mode(BacklightDynamicMode mode);
#endif

/**
 * @ingroup services_light
 * @brief Allow or disallow the backlight, e.g. in low power mode.
 *
 * Disallowing turns the light off.
 *
 * @param allowed true to allow the backlight.
 */
void light_allow(bool allowed);

/**
 * @ingroup services_light
 * @brief Get the current backlight brightness.
 *
 * May differ from the configured brightness when dynamic backlight is enabled.
 *
 * @return Brightness in percent, 0-100.
 */
uint8_t light_get_current_brightness_percent(void);

/**
 * @brief Check whether the backlight is on.
 *
 * @return true if the backlight is currently on in any form (on, timed, or
 *         fading out). Returns false only when the backlight is fully off.
 */
bool light_is_on(void);

/**
 * @ingroup services_light
 * @brief Check whether the backlight was lit by touch alone.
 *
 * @return true if the backlight is on and its current lit period was last started or refreshed
 *         by touch contact alone.
 */
bool light_is_lit_by_touch(void);

/**
 * @ingroup services_light
 * @brief Get the ambient light level.
 *
 * Screen-compensated and converted with the board's calibration; raw counts pass through on
 * boards without lux coefficients. Served from a short-lived cache; while the backlight is on,
 * the last value read before it turned on is returned.
 *
 * @return Ambient light level in lux.
 */
uint32_t light_get_ambient_lux(void);

/** @} */

/** @} */
