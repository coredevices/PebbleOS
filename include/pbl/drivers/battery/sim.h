/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_battery_sim Simulator
 * @ingroup drivers_battery
 * @brief Simulated battery, full and unplugged until pbl_battery_sim_set() changes it.
 * @{
 */

/**
 * @brief Set the battery state.
 *
 * The voltage follows the battery curve. A charger change is reported to the connection handler.
 *
 * @param percent Charge percentage, capped at 100.
 * @param charger_connected Whether the charger is connected.
 */
void pbl_battery_sim_set(uint8_t percent, bool charger_connected);

/**
 * @brief Get the charge percentage last set.
 *
 * Lets the battery service skip the lossy voltage curve round trip.
 *
 * @return Charge percentage, 0 to 100.
 */
uint8_t pbl_battery_sim_get_percent(void);

/** @} */
