/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_sf32lb52_hxt SF32LB52 HXT48 trim
 * @ingroup drivers_sf32lb52
 * @brief Crystal load capacitance trim from the factory configuration in the flash OTP area.
 *
 * The SoC factory configuration is a list of records (one byte id, one byte length, data) in OTP
 * cells the board names. The first crystal record found is applied to the 48 MHz crystal
 * oscillator once the flash is up, before the radio core starts:
 *
 * @code{.c}
 * PBL_SF32LB52_HXT_DEFINE(s_hxt, "hxt", &s_flash.flash, "soc-config-user", "soc-config-cust");
 * @endcode
 * @{
 */

/** @brief Crystal trim device. */
struct pbl_sf32lb52_hxt {
  /** Device, a child of the flash. */
  struct pbl_device dev;
  /** Flash holding the factory configuration. */
  const struct pbl_flash *flash;
  /** Names of the OTP cells holding it, in search order. */
  const char *const *cells;
  /** Number of cells. */
  uint8_t num_cells;
};

/** @cond INTERNAL_HIDDEN */
int pbl_sf32lb52_hxt_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the crystal trim device.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _flash Flash, a struct pbl_flash.
 * @param ... Names of the OTP cells holding the factory configuration, in search order.
 */
#define PBL_SF32LB52_HXT_DEFINE(sym, _name, _flash, ...)                                \
  PBL_DEVICE_STATE_DEFINE(sym);                                                         \
  static const char *const sym##_cells[] = {__VA_ARGS__};                               \
  const struct pbl_sf32lb52_hxt sym = {                                                 \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_sf32lb52_hxt_init, &(_flash)->dev, nullptr), \
    .flash = (_flash),                                                                  \
    .cells = sym##_cells,                                                               \
    .num_cells = sizeof(sym##_cells) / sizeof(sym##_cells[0]),                          \
  };                                                                                    \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/** @} */
