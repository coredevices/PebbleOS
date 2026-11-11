/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/als.h>

/**
 * @defgroup drivers_als_sim Simulator
 * @ingroup drivers_als
 * @brief Simulated ambient light sensor, reading what pbl_als_sim_set() sets, complete darkness
 * by default.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_als pbl_als_sim;
/** @endcond */

/** @brief The simulated ambient light sensor. */
#define SIM_ALS (&pbl_als_sim)

/**
 * @brief Set the light level the sensor reads.
 *
 * @param als Device.
 * @param level Light level in raw counts, below @ref PBL_ALS_LEVEL_MAX.
 */
void pbl_als_sim_set(const struct pbl_als *als, uint32_t level);

/** @} */
