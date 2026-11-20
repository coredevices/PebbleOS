/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/ram.h>

/**
 * @defgroup drivers_ram_qemu QEMU PSRAM
 * @ingroup drivers_ram
 * @brief @ref drivers_ram driver for the PSRAM of the QEMU Pebble machines.
 *
 * The memory is always mapped and powered: it takes no setting up and has no power states.
 * @{
 */

/** @brief QEMU PSRAM. */
struct pbl_ram_qemu {
  /** RAM device. */
  struct pbl_ram ram;
  /** CPU address of the memory. */
  uintptr_t base;
  /** Size of the memory, in bytes. */
  size_t size;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ram_ops pbl_ram_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU PSRAM.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base CPU address of the memory.
 * @param _size Size of the memory, in bytes.
 */
#define PBL_RAM_QEMU_DEFINE(sym, _name, _base, _size)                                              \
  PBL_RAM_STATE_DEFINE(sym);                                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ram_qemu, struct pbl_ram, ram) {                             \
    .ram = PBL_RAM_INIT(sym, _name, nullptr, &pbl_ram_qemu_ops), .base = (_base), .size = (_size), \
  }

/** @} */
