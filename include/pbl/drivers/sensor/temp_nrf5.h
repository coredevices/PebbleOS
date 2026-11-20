/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/sensor.h>

/**
 * @defgroup drivers_sensor_temp_nrf5 nRF5 TEMP
 * @ingroup drivers_sensor
 * @brief Nordic nRF5 die temperature sensor (TEMP).
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_temp_nrf5;
/** @endcond */

/** @brief The nRF5 die temperature sensor. */
#define NRF5_TEMP (&pbl_temp_nrf5)

/** @} */
