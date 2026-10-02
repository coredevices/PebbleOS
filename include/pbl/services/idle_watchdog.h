/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_idle_watchdog PRF idle watchdog
 * @ingroup services
 * @brief Shuts down an idle watch running the recovery firmware.
 *
 * Puts the watch into standby after 10 minutes without Bluetooth connection changes, charger
 * changes or button presses, unless a BLE connection is up or the charger is plugged in. This
 * improves the chances of watches being shipped with some battery charge left.
 * @{
 */

/**
 * @brief Subscribe to the events that feed the watchdog.
 *
 * Called once at boot from KernelMain. Does not start the watchdog.
 */
void prf_idle_watchdog_init(void);

/** @brief Start or restart the watchdog. Can be called from any task. */
void prf_idle_watchdog_start(void);

/** @brief Stop the watchdog. Can be called from any task. */
void prf_idle_watchdog_stop(void);

/** @} */
