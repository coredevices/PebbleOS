/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_watchdog_sf32lb SiFli SF32LB watchdog
 * @ingroup drivers_watchdog
 * @brief WDT1 of the SF32LB52, behind the @ref drivers_watchdog functions.
 *
 * The device starts the watchdog when it comes up, with a timeout of
 * @c CONFIG_WATCHDOG_TIMEOUT_MS at the rate of its clock. Until then, the watchdog the
 * bootloader started keeps running.
 * @{
 */

/** @brief Watchdog device. */
struct pbl_watchdog_sf32lb {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
int pbl_watchdog_sf32lb_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the watchdog device.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_WATCHDOG_SF32LB_DEFINE(sym, _name, _base, _clk)                              \
  PBL_DEVICE_STATE_DEFINE(sym);                                                          \
  extern const struct pbl_watchdog_sf32lb sym##_inst;                                    \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.dev);                                      \
  PBL_DEVICE_ALIAS(sym, sym##_inst, struct pbl_watchdog_sf32lb, struct pbl_device, dev); \
  const struct pbl_watchdog_sf32lb sym##_inst = {                                        \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_watchdog_sf32lb_init, nullptr, nullptr),      \
    .base = (_base),                                                                     \
    .clk = _clk,                                                                         \
  }

/** @} */
