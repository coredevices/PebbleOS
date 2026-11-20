/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/charger.h>

/**
 * @defgroup drivers_charger_sim Simulator
 * @ingroup drivers_charger
 * @brief Simulated charger, with a full battery and no external power until
 * pbl_charger_sim_set() changes it.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_charger pbl_charger_sim;
/** @endcond */

/** @brief The simulated charger. */
#define SIM_CHARGER (&pbl_charger_sim)

/**
 * @brief Set the battery state.
 *
 * An external power change is reported to the listener.
 *
 * @param percent Charge percentage, capped at 100.
 * @param voltage_mv Battery voltage in mV.
 * @param online Whether external power is present.
 */
void pbl_charger_sim_set(uint8_t percent, int32_t voltage_mv, bool online);

/**
 * @brief Get the charge percentage last set.
 *
 * Lets the battery service skip the lossy voltage curve round trip.
 *
 * @return Charge percentage, 0 to 100.
 */
uint8_t pbl_charger_sim_get_percent(void);

/** @} */
