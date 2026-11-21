/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_watchdog_stm32_iwdg ST STM32 IWDG
 * @ingroup drivers_watchdog
 * @brief Independent watchdog of the STM32H7, behind the @ref drivers_watchdog functions.
 *
 * The device starts the watchdog when it comes up, with a timeout of
 * @c CONFIG_WATCHDOG_TIMEOUT_MS at the rate of its clock. Nothing stops it once started but a
 * reset; a debugger freezes it while the CPU is halted. The reset flags come from the RCC that
 * clocks it.
 * @{
 */

/** @brief Watchdog device. */
struct pbl_watchdog_stm32_iwdg {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** LSI clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
int pbl_watchdog_stm32_iwdg_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the watchdog device.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk LSI clock, from PBL_CLK().
 */
#define PBL_WATCHDOG_STM32_IWDG_DEFINE(sym, _name, _base, _clk)                                 \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                 \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_watchdog_stm32_iwdg) {                                \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_watchdog_stm32_iwdg_init, nullptr), .base = (_base), \
    .clk = _clk,                                                                                \
  }

/** @} */
