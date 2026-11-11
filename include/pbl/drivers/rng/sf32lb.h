/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_rng_sf32lb SiFli SF32LB TRNG
 * @ingroup drivers_rng
 * @brief True random number generator of the SF32LB52, behind rng_rand().
 * @{
 */

/** @brief TRNG. */
struct pbl_rng_sf32lb {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
};

/**
 * @brief Define the TRNG.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_RNG_SF32LB_DEFINE(sym, _name, _base, _clk)                              \
  PBL_DEVICE_STATE_DEFINE(sym);                                                     \
  extern const struct pbl_rng_sf32lb sym##_inst;                                    \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.dev);                                 \
  PBL_DEVICE_ALIAS(sym, sym##_inst, struct pbl_rng_sf32lb, struct pbl_device, dev); \
  const struct pbl_rng_sf32lb sym##_inst = {                                        \
    .dev = PBL_DEVICE_INIT(sym, _name, nullptr, nullptr, nullptr),                  \
    .base = (_base),                                                                \
    .clk = _clk,                                                                    \
  }

/** @} */
