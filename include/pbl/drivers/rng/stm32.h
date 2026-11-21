/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_rng_stm32 ST STM32 RNG
 * @ingroup drivers_rng
 * @brief True random number generator of the STM32, behind rng_rand().
 * @{
 */

/** @brief RNG. */
struct pbl_rng_stm32 {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
int pbl_rng_stm32_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the RNG.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_RNG_STM32_DEFINE(sym, _name, _base, _clk)                                              \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_rng_stm32) {                                             \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_rng_stm32_init, nullptr), .base = (_base), .clk = _clk, \
  }

/** @} */
