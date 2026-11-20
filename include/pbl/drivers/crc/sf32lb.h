/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_crc_sf32lb SiFli SF32LB CRC unit
 * @ingroup drivers_crc
 * @brief CRC unit of the SF32LB52, behind the hardware CRC functions.
 * @{
 */

/** @brief CRC unit. */
struct pbl_crc_sf32lb {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
int pbl_crc_sf32lb_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the CRC unit.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_CRC_SF32LB_DEFINE(sym, _name, _base, _clk)                                 \
  PBL_DEVICE_STATE_DEFINE(sym);                                                        \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_crc_sf32lb) {                                \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_crc_sf32lb_init, nullptr), .base = (_base), \
    .clk = _clk,                                                                       \
  }

/** @} */
