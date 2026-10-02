/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_temperature Temperature sensor
 * @ingroup drivers
 * @brief On-chip temperature sensor.
 * @{
 */

/** @brief Initialize the temperature sensor. */
void temperature_init(void);

/**
 * @brief Read the temperature.
 *
 * @warning The sensor may not be calibrated: do not rely on the reading as an accurate absolute
 * temperature.
 *
 * @return Temperature in millidegrees Celsius, or INT32_MIN if the conversion timed out.
 */
int32_t temperature_read(void);

/** @} */
