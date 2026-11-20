/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/flash.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_clk_sf32lb52_hxt SF32LB52 crystal trim
 * @ingroup drivers_clk
 * @brief Load capacitance trim of the 48 MHz crystal, from the SoC factory configuration in the
 * flash OTP area.
 *
 * The configuration is a list of records (one byte id, one byte length, data) in the OTP cells
 * given, searched in order. The first crystal record found is applied once the flash is up.
 *
 * @code{.c}
 * PBL_CLK_SF32LB52_HXT_DEFINE(s_hxt48, "hxt48", 0x500ca000, &s_flash, "soc-config-user");
 * @endcode
 * @{
 */

/** @brief Crystal trim, a child of the flash holding the configuration. */
struct pbl_clk_sf32lb52_hxt {
  /** Device. */
  struct pbl_device dev;
  /** PMUC registers. */
  uintptr_t base;
  /** Flash holding the configuration. */
  const struct pbl_flash *flash;
  /** Names of the OTP cells holding it, in search order. */
  const char *const *cells;
  /** Number of cells. */
  uint8_t num_cells;
};

/** @cond INTERNAL_HIDDEN */
int pbl_clk_sf32lb52_hxt_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the crystal trim.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base PMUC registers.
 * @param _flash Flash, a struct pbl_flash.
 * @param ... Names of the OTP cells holding the configuration, in search order.
 */
#define PBL_CLK_SF32LB52_HXT_DEFINE(sym, _name, _base, _flash, ...)                       \
  PBL_DEVICE_STATE_DEFINE(sym);                                                           \
  static const char *const sym##_cells[] = {__VA_ARGS__};                                 \
  extern const struct pbl_clk_sf32lb52_hxt sym##_inst;                                    \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.dev);                                       \
  PBL_DEVICE_ALIAS(sym, sym##_inst, struct pbl_clk_sf32lb52_hxt, struct pbl_device, dev); \
  const struct pbl_clk_sf32lb52_hxt sym##_inst = {                                        \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_clk_sf32lb52_hxt_init, &(_flash)->dev),        \
    .base = (_base),                                                                      \
    .flash = (_flash),                                                                    \
    .cells = sym##_cells,                                                                 \
    .num_cells = PBL_ARRAY_SIZE(sym##_cells),                                             \
  }

/** @} */
