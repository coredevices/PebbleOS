/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/temp.h>

/**
 * @defgroup drivers_temp_nrf5 nRF5
 * @ingroup drivers_temp
 * @brief Nordic nRF5 die temperature sensor (TEMP).
 * @{
 */

/** @brief nRF5 die temperature sensor. */
struct pbl_temp_nrf5 {
  /** Temperature sensor. */
  struct pbl_temp temp;
  /** Registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp_ops pbl_temp_nrf5_ops;
/** @endcond */

/**
 * @brief Define the temperature sensor.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 */
#define PBL_TEMP_NRF5_DEFINE(sym, _name, _base)                                      \
  PBL_TEMP_STATE_DEFINE(sym);                                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_nrf5, struct pbl_temp, temp) {            \
    .temp = PBL_TEMP_INIT(sym, _name, nullptr, &pbl_temp_nrf5_ops), .base = (_base), \
  }

/** @} */
