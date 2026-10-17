/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/temperature.h>

/**
 * @defgroup drivers_temperature_nrf5 nRF5
 * @ingroup drivers_temperature
 * @brief Nordic nRF5 die temperature sensor (TEMP).
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp pbl_temp_nrf5;
/** @endcond */

/** @brief The nRF5 die temperature sensor. */
#define NRF5_TEMP (&pbl_temp_nrf5)

/** @} */
