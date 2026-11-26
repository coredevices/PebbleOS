/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/util/units.h>

/**
 * @defgroup drivers_flash_nrf_rramc nRF RRAMC
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for the internal RRAM of the nRF54L, written through its
 * controller and read at its CPU address.
 *
 * The RRAM writes any data over any data, see @ref PBL_FLASH_NO_ERASE. The MPU keeps the image
 * read-only, so data goes in partitions outside @c code. With the MPSL, each chunk is programmed in
 * a timeslot of the radio scheduler: the CPU cannot read the RRAM while it programs, and the radio
 * interrupts run from it.
 * @{
 */

/** @brief Internal RRAM. */
struct pbl_flash_nrf_rramc {
  /** Flash. */
  struct pbl_flash flash;
  /** Controller register base address. */
  uintptr_t base;
  /** CPU address of the RRAM. */
  uintptr_t mem;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash_ops pbl_flash_nrf_rramc_ops;
/** @endcond */

/**
 * @brief Define the internal RRAM.
 *
 * @param sym Symbol of the flash.
 * @param _name Name.
 * @param _base Controller register base address.
 * @param _mem CPU address of the RRAM.
 * @param _size Size of the RRAM, in bytes.
 */
#define PBL_FLASH_NRF_RRAMC_DEFINE(sym, _name, _base, _mem, _size)                          \
  PBL_FLASH_STATE_DEFINE(sym);                                                              \
  static const struct pbl_flash_geometry sym##_geometry = {                                 \
    .size = (_size),                                                                        \
    .page_size = 512U,                                                                      \
    .write_size = 16U,                                                                      \
    .flags = PBL_FLASH_NO_ERASE,                                                            \
    .sector_size = PBL_KIB(64),                                                             \
    .subsector_size = PBL_KIB(4),                                                           \
  };                                                                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_flash_nrf_rramc, struct pbl_flash, flash) {           \
    .flash = PBL_FLASH_INIT(sym, _name, nullptr, &pbl_flash_nrf_rramc_ops, &sym##_geometry, \
                            nullptr, nullptr),                                              \
    .base = (_base), .mem = (_mem),                                                         \
  }

/** @} */
