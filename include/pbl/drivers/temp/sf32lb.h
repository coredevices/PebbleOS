/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/temp.h>

/**
 * @defgroup drivers_temp_sf32lb SF32LB
 * @ingroup drivers_temp
 * @brief SiFli SF32LB die temperature sensor (TSEN).
 * @{
 */

/** @brief SF32LB die temperature sensor. */
struct pbl_temp_sf32lb {
  /** Temperature sensor. */
  struct pbl_temp temp;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** HPSYS configuration registers, holding the analog bandgap enable. */
  uintptr_t cfg;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp_ops pbl_temp_sf32lb_ops;
/** @endcond */

/**
 * @brief Define the temperature sensor.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _cfg HPSYS configuration registers.
 */
#define PBL_TEMP_SF32LB_DEFINE(sym, _name, _base, _clk, _cfg)                                   \
  PBL_TEMP_STATE_DEFINE(sym);                                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_sf32lb, struct pbl_temp, temp) {                     \
    .temp = PBL_TEMP_INIT(sym, _name, nullptr, nullptr, &pbl_temp_sf32lb_ops), .base = (_base), \
    .clk = _clk, .cfg = (_cfg),                                                                 \
  }

/** @} */
