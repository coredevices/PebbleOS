/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/memc/sf32lb52_mpi.h>
#include <pbl/drivers/spi_mem.h>

/**
 * @defgroup drivers_spi_mem_sf32lb52_mpi SF32LB52 MPI
 * @ingroup drivers_spi_mem
 * @brief @ref drivers_spi_mem controller driver for the SF32LB52 MPI, for a SPI NOR flash on it.
 *
 * Operations go through the command registers and the 64-byte FIFO: read data comes back
 * through it, and write data is pushed to it byte by byte once the command is issued, so that it
 * only ever holds the bytes the command takes. The memory-mapped window is the read window,
 * programmed with the read operation the memory driver picks.
 *
 * The controller is @ref PBL_SPI_MEM_XIP when the firmware runs from its window, at
 * @c CONFIG_FLASH_BASE: the boot code's clock, prescaler and sampling setup are kept, the clock is
 * never gated, and everything that runs while the memory is unreadable (status polling, window
 * changes, power down) runs from RAM with interrupts disabled.
 *
 * @code{.c}
 * PBL_SF32LB52_MPI_DEFINE(s_mpi2, "mpi2", 0x50042000, 0x12000000, 0xe000000,
 *                         PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MPI2), nullptr);
 * PBL_SPI_MEM_SF32LB52_MPI_DEFINE(s_mpi2_spi_mem, "mpi2", &s_mpi2, 0x12000000, 0xe000000);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_mem_sf32lb52_mpi_data {
  bool started;
};
/** @endcond */

/** @brief SF32LB52 MPI SPI memory controller. */
struct pbl_spi_mem_sf32lb52_mpi {
  /** Controller. */
  struct pbl_spi_mem mem;
  /** Core of the MPI. */
  const struct pbl_sf32lb52_mpi *mpi;
  /** Runtime data. */
  struct pbl_spi_mem_sf32lb52_mpi_data *data;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_sf32lb52_mpi_ops;

#define PBL_SPI_MEM_SF32LB52_MPI_XIP(_mem_base, _mem_size)                                 \
  (((CONFIG_FLASH_BASE >= (_mem_base)) && (CONFIG_FLASH_BASE - (_mem_base) < (_mem_size))) \
       ? PBL_SPI_MEM_XIP                                                                   \
       : 0U)
/** @endcond */

/**
 * @brief Define an SF32LB52 MPI SPI memory controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _mpi Core of the MPI, from PBL_SF32LB52_MPI_DEFINE().
 * @param _mem_base Memory-mapped window of the MPI.
 * @param _mem_size Memory-mapped window size, in bytes.
 */
#define PBL_SPI_MEM_SF32LB52_MPI_DEFINE(sym, _name, _mpi, _mem_base, _mem_size)                    \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                                   \
  static struct pbl_spi_mem_sf32lb52_mpi_data sym##_data;                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_mem_sf32lb52_mpi, struct pbl_spi_mem, mem) {             \
    .mem =                                                                                         \
        PBL_SPI_MEM_INIT(sym, _name, PBL_DEVICE_DEPS(&(_mpi)->dev), &pbl_spi_mem_sf32lb52_mpi_ops, \
                         PBL_SPI_MEM_SF32LB52_MPI_XIP(_mem_base, _mem_size)),                      \
    .mpi = (_mpi), .data = &sym##_data,                                                            \
  }

/** @} */
