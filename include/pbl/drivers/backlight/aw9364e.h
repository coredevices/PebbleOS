/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_backlight_aw9364e AW9364E
 * @ingroup drivers_backlight
 * @brief @ref drivers_backlight implementation for the AW9364E LED driver.
 *
 * Brightness is set by pulse counting on a single enable line.
 * @{
 */

/** @brief AW9364E configuration. */
typedef struct LedControllerAW9364E {
  /** Enable line, also used for the dimming pulses. */
  OutputConfig gpio;
} LedControllerAW9364E;

/** @} */