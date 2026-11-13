/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/hyperbus.h>
#include <pbl/drivers/memc/sf32lb52_mpi.h>

/**
 * @defgroup drivers_hyperbus_sf32lb52_mpi SF32LB52 MPI
 * @ingroup drivers_hyperbus
 * @brief @ref drivers_hyperbus controller driver for the SF32LB52 MPI.
 *
 * It calibrates the delay lines of the controller, turns the times the memory asks for into
 * clock cycles, and maps the memory through the system bus alias of the controller's window.
 * The bus runs at half the controller clock.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_hyperbus_sf32lb52_mpi_data {
  uint8_t latency;
};
/** @endcond */

/** @brief SF32LB52 MPI HyperBus controller. */
struct pbl_hyperbus_sf32lb52_mpi {
  /** Controller. */
  struct pbl_hyperbus bus;
  /** Core of the MPI. */
  const struct pbl_sf32lb52_mpi *mpi;
  /** Runtime data. */
  struct pbl_hyperbus_sf32lb52_mpi_data *data;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_hyperbus_ops pbl_hyperbus_sf32lb52_mpi_ops;
/** @endcond */

/**
 * @brief Define an SF32LB52 MPI HyperBus controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _mpi Core of the MPI, from PBL_SF32LB52_MPI_DEFINE().
 */
#define PBL_HYPERBUS_SF32LB52_MPI_DEFINE(sym, _name, _mpi)                               \
  PBL_DEVICE_STATE_DEFINE(sym);                                                          \
  static struct pbl_hyperbus_sf32lb52_mpi_data sym##_data;                               \
  PBL_DEVICE_INSTANCE(sym, struct pbl_hyperbus_sf32lb52_mpi, struct pbl_hyperbus, bus) { \
    .bus = PBL_HYPERBUS_INIT(sym, _name, &(_mpi)->dev, &pbl_hyperbus_sf32lb52_mpi_ops),  \
    .mpi = (_mpi), .data = &sym##_data,                                                  \
  }

/** @} */
