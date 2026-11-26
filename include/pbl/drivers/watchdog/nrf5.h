/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/device.h>

/**
 * @defgroup drivers_watchdog_nrf5 Nordic nRF5 watchdog
 * @ingroup drivers_watchdog
 * @brief WDT of the nRF52, behind the @ref drivers_watchdog functions.
 *
 * The device starts the watchdog when it comes up, with a timeout of @c CONFIG_WATCHDOG_TIMEOUT_MS,
 * and enables the interrupt it raises just before the reset, which reports to the
 * watchdog_set_callback() subscriber. Nothing stops it once started but a reset, and its
 * configuration is fixed from then on: when the bootloader started it, its timeout stays.
 * @{
 */

/** @brief Watchdog device. */
struct pbl_watchdog_nrf5 {
  /** Device. */
  struct pbl_device dev;
};

/** @cond INTERNAL_HIDDEN */
int pbl_watchdog_nrf5_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the watchdog device.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 */
#define PBL_WATCHDOG_NRF5_DEFINE(sym, _name)                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                          \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_watchdog_nrf5) {               \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_watchdog_nrf5_init, nullptr), \
  }

/** @} */
