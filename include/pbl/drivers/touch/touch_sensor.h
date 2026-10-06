/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup drivers_touch Touch sensor
 * @ingroup drivers
 * @brief Touchscreen controller driver interface.
 *
 * Touch events are reported to the touch service.
 * @{
 */

/**
 * @brief Initialize the touch sensor.
 *
 * Called once at startup.
 */
void touch_sensor_init(void);

/**
 * @brief Enable or disable the touch sensor.
 *
 * While disabled, no touch events are reported.
 *
 * @param enabled true to enable.
 */
void touch_sensor_set_enabled(bool enabled);

/** @} */
