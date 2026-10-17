/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/temperature.h>

/**
 * @defgroup drivers_temperature_sf32lb SF32LB
 * @ingroup drivers_temperature
 * @brief SiFli SF32LB die temperature sensor (TSEN).
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp pbl_temp_sf32lb;
/** @endcond */

/** @brief The SF32LB die temperature sensor. */
#define SF32LB_TEMP (&pbl_temp_sf32lb)

/** @} */
