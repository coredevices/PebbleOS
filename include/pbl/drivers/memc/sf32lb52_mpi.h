/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>

/**
 * @defgroup drivers_memc_sf32lb52_mpi SF32LB52 MPI
 * @ingroup drivers
 * @brief Core of the SF32LB52 MPI memory controller.
 *
 * The controller drives one memory: a SPI one, with the @ref drivers_spi_mem driver of the
 * controller, or a HyperBus one, with its @ref drivers_hyperbus driver. The core is what both
 * have of the controller: its registers, its memory-mapped window, its clock and its pins. The
 * devicetree generator gives a controller the driver for the memory on it.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_sf32lb52_mpi_state {
  bool prepared;
};
/** @endcond */

/** @brief SF32LB52 MPI controller. */
struct pbl_sf32lb52_mpi {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Memory-mapped window. */
  uintptr_t mem_base;
  /** Memory-mapped window size, in bytes. */
  uint32_t mem_size;
  /** Clock. */
  struct pbl_clk clk;
  /** Pins, when the boot code did not set them up. */
  struct pbl_pinctrl pinctrl;
  /** Clock parents and rates to assign. */
  const struct pbl_clk_assigned *assigned;
  /** Runtime state. */
  struct pbl_sf32lb52_mpi_state *state;
};

/**
 * @brief Define an SF32LB52 MPI controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _mem_base Memory-mapped window.
 * @param _mem_size Memory-mapped window size, in bytes.
 * @param _clk Clock, from PBL_CLK().
 * @param _pinctrl Default state of its pins, or NULL when the boot code set them up.
 * @param ... PBL_CLK_ASSIGNED() initializers, applied by pbl_sf32lb52_mpi_prepare().
 */
#define PBL_SF32LB52_MPI_DEFINE(sym, _name, _base, _mem_base, _mem_size, _clk, _pinctrl, ...) \
  PBL_DEVICE_STATE_DEFINE(sym);                                                               \
  static struct pbl_sf32lb52_mpi_state sym##_mpi_state;                                       \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                       \
  const struct pbl_sf32lb52_mpi sym = {                                                       \
    .dev = PBL_DEVICE_INIT(sym, _name, nullptr, nullptr, nullptr),                            \
    .base = (_base),                                                                          \
    .mem_base = (_mem_base),                                                                  \
    .mem_size = (_mem_size),                                                                  \
    .clk = _clk,                                                                              \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                             \
    .assigned = sym##_assigned,                                                               \
    .state = &sym##_mpi_state,                                                                \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/**
 * @brief Set up the pins and the clock parents of a controller, once.
 *
 * Called by the driver of the memory on it before its first use, so that a controller nothing
 * uses is left as it is.
 *
 * @param mpi Controller.
 * @return 0 or a negative errno.
 */
int pbl_sf32lb52_mpi_prepare(const struct pbl_sf32lb52_mpi *mpi);

/** @} */
