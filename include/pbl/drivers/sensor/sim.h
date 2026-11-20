/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/sensor.h>

/**
 * @defgroup drivers_sensor_sim Simulators
 * @ingroup drivers_sensor
 * @brief Simulated sensors, measuring what their simulator API sets.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_temp_sim;
/** @endcond */

/** @brief The simulated temperature sensor, at 25 degrees Celsius until set. */
#define SIM_TEMP (&pbl_temp_sim)

/**
 * @brief Set the temperature the simulated temperature sensor reads.
 *
 * @param sensor Sensor.
 * @param millicelsius Temperature in millidegrees Celsius.
 */
void pbl_temp_sim_set(const struct pbl_sensor *sensor, int32_t millicelsius);

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_mag_sim;
/** @endcond */

/**
 * @brief The simulated magnetometer, measuring no field until set.
 *
 * While sampling, the field is reported once per sampling interval.
 */
#define SIM_MAG (&pbl_mag_sim)

/**
 * @brief Set the magnetic field the simulated magnetometer measures.
 *
 * @param sensor Sensor.
 * @param x Field along the watch X axis, in nanotesla.
 * @param y Field along the watch Y axis, in nanotesla.
 * @param z Field along the watch Z axis, in nanotesla.
 */
void pbl_mag_sim_set(const struct pbl_sensor *sensor, int32_t x, int32_t y, int32_t z);

/** @} */
