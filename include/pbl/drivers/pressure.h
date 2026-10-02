/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_pressure Pressure sensor
 * @ingroup drivers
 * @brief Barometric pressure sensor driver interface.
 * @{
 */

/**
 * @brief Initialize the pressure sensor.
 *
 * Called once at startup. Probes the sensor and leaves it in its low-power state.
 */
void pressure_init(void);

/** @} */
