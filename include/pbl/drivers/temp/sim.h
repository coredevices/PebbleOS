/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/temp.h>

/**
 * @defgroup drivers_temp_sim Simulator
 * @ingroup drivers_temp
 * @brief Simulated temperature sensor, reading what pbl_temp_sim_set() sets, 25 degrees Celsius
 * by default.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp pbl_temp_sim;
/** @endcond */

/** @brief The simulated temperature sensor. */
#define SIM_TEMP (&pbl_temp_sim)

/**
 * @brief Set the temperature the sensor reads.
 *
 * @param temp Device.
 * @param millicelsius Temperature in millidegrees Celsius.
 */
void pbl_temp_sim_set(const struct pbl_temp *temp, int32_t millicelsius);

/** @} */
