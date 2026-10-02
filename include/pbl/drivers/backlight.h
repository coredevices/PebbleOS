/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_backlight Backlight
 * @ingroup drivers
 * @brief Backlight driver interface.
 *
 * @code{.c}
 * backlight_init();
 * backlight_set_brightness(60);
 * ...
 * backlight_set_brightness(0);
 * @endcode
 * @{
 */

/**
 * @name Backlight colors
 * Colors for backlight_set_color(), as 0xRRGGBB. Not gamma-corrected.
 * @{
 */
/** @brief Red. */
#define BACKLIGHT_COLOR_RED 0xFF0000
/** @brief Green. */
#define BACKLIGHT_COLOR_GREEN 0x00FF00
/** @brief Blue. */
#define BACKLIGHT_COLOR_BLUE 0x0000FF
/** @brief Black (off). */
#define BACKLIGHT_COLOR_BLACK 0x000000
/** @brief White. */
#define BACKLIGHT_COLOR_WHITE 0xFFFFFF
/** @brief Warm white. */
#define BACKLIGHT_COLOR_WARM_WHITE 0xFFBFA2
/** @} */

/** @brief Initialize the backlight. */
void backlight_init(void);

/**
 * @brief Set the backlight brightness.
 *
 * @param brightness Brightness from 0 (off) to 100.
 */
void backlight_set_brightness(uint8_t brightness);

/**
 * @brief Map a brightness to an opaque hardware level identifier.
 *
 * Two brightness values with the same identifier produce identical output, which lets callers
 * collapse steps the hardware cannot distinguish. Drivers with continuous control return the
 * input unchanged.
 *
 * @param brightness Brightness from 0 to 100.
 * @return Hardware level identifier.
 */
uint8_t backlight_get_level(uint8_t brightness);

/**
 * @brief Re-apply the cached driver state to the hardware.
 *
 * A no-op if the driver believes the backlight is off.
 */
void backlight_refresh(void);

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
/**
 * @brief Set the backlight color.
 *
 * Only on boards with @c CONFIG_BACKLIGHT_HAS_COLOR.
 *
 * @param rgb_color Color as 0xRRGGBB, e.g. one of the @c BACKLIGHT_COLOR_* values.
 */
void backlight_set_color(uint32_t rgb_color);

/**
 * @brief Get the backlight color.
 *
 * Only on boards with @c CONFIG_BACKLIGHT_HAS_COLOR.
 *
 * @return Color as 0xRRGGBB.
 */
uint32_t backlight_get_color(void);
#endif

/** @} */