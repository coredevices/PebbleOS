/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/util/units.h>

/**
 * @defgroup drivers_flash_qemu QEMU
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for the QEMU external flash, read through its memory window.
 * @{
 */

/** @brief QEMU external flash. */
struct pbl_flash_qemu {
  /** Flash. */
  struct pbl_flash flash;
  /** Register base address. */
  uintptr_t base;
  /** Memory window base address. */
  uintptr_t mem;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash_ops pbl_flash_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU external flash.
 *
 * @param sym Symbol of the flash.
 * @param _name Name.
 * @param _base Register base address.
 * @param _mem Memory window base address.
 * @param _size Size, that of the memory window, in bytes.
 */
#define PBL_FLASH_QEMU_DEFINE(sym, _name, _base, _mem, _size)                                   \
  PBL_FLASH_STATE_DEFINE(sym);                                                                  \
  static const struct pbl_flash_geometry sym##_geometry = {                                     \
    .size = (_size),                                                                            \
    .page_size = 256U,                                                                          \
    .sector_size = PBL_KIB(64),                                                                 \
    .subsector_size = PBL_KIB(4),                                                               \
    .sector_erase_ms = 1U,                                                                      \
    .subsector_erase_ms = 1U,                                                                   \
  };                                                                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_flash_qemu, struct pbl_flash, flash) {                    \
    .flash = PBL_FLASH_INIT(sym, _name, nullptr, nullptr, &pbl_flash_qemu_ops, &sym##_geometry, \
                            nullptr, nullptr),                                                  \
    .base = (_base), .mem = (_mem),                                                             \
  }

/** @} */
