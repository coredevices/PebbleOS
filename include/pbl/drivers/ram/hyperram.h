/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/hyperbus.h>
#include <pbl/drivers/ram.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_ram_hyperram HyperRAM
 * @ingroup drivers_ram
 * @brief @ref drivers_ram driver for HyperRAM, PSRAM with a HyperBus interface.
 *
 * It knows the memory, not the controller it is on: its identification and configuration
 * registers, the latency its clock needs, and how it sleeps. It reaches it through its
 * @ref drivers_hyperbus controller, and takes its size from its identification register.
 *
 * Off, the memory is in deep power-down and its controller is gated; powering it on wakes it up
 * and sets it up again. In deep sleep it is in hybrid sleep, which keeps its contents.
 * @{
 */

/** @brief HyperRAM. */
struct pbl_ram_hyperram {
  /** RAM device. */
  struct pbl_ram ram;
  /** Controller the memory is on. */
  const struct pbl_hyperbus *bus;
  /** Supply, or NULL. */
  const struct pbl_regulator *vcc;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ram_ops pbl_ram_hyperram_ops;
/** @endcond */

/**
 * @brief Define a HyperRAM.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _bus Controller, a @ref drivers_hyperbus device.
 * @param _vcc Supply, or NULL.
 */
#define PBL_RAM_HYPERRAM_DEFINE(sym, _name, _bus, _vcc)                                           \
  PBL_RAM_STATE_DEFINE(sym);                                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ram_hyperram, struct pbl_ram, ram) {                        \
    .ram = PBL_RAM_INIT(sym, _name, &(_bus)->dev, nullptr, &pbl_ram_hyperram_ops), .bus = (_bus), \
    .vcc = (_vcc),                                                                                \
  }

/** @} */
