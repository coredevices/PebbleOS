/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/spi_mem.h>

/**
 * @defgroup drivers_spi_mem_sf32lb52_mpi SF32LB52 MPI
 * @ingroup drivers_spi_mem
 * @brief @ref drivers_spi_mem controller driver for the SF32LB52 MPI in NOR mode.
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
 * PBL_SPI_MEM_SF32LB52_MPI_DEFINE(s_mpi2, "mpi2", SF32LB52_MPI2_BASE, SF32LB52_MPI2_MEM_BASE,
 *                                 0xe000000, PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MPI2));
 * @endcode
 * @{
 */

/** @brief SF32LB52 MPI controller. */
struct pbl_spi_mem_sf32lb52_mpi {
  /** Controller. */
  struct pbl_spi_mem mem;
  /** Registers. */
  uintptr_t base;
  /** Memory-mapped window. */
  uintptr_t mem_base;
  /** Memory-mapped window size, in bytes. */
  uint32_t mem_size;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_sf32lb52_mpi_ops;

#define PBL_SPI_MEM_SF32LB52_MPI_XIP(_mem_base, _mem_size)                                 \
  (((CONFIG_FLASH_BASE >= (_mem_base)) && (CONFIG_FLASH_BASE - (_mem_base) < (_mem_size))) \
       ? PBL_SPI_MEM_XIP                                                                   \
       : 0U)
/** @endcond */

/**
 * @brief Define an SF32LB52 MPI controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _mem_base Memory-mapped window.
 * @param _mem_size Memory-mapped window size, in bytes.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_SPI_MEM_SF32LB52_MPI_DEFINE(sym, _name, _base, _mem_base, _mem_size, _clk) \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_mem_sf32lb52_mpi, struct pbl_spi_mem, mem) { \
    .mem = PBL_SPI_MEM_INIT(sym, _name, nullptr, &pbl_spi_mem_sf32lb52_mpi_ops,        \
                            PBL_SPI_MEM_SF32LB52_MPI_XIP(_mem_base, _mem_size)),       \
    .base = (_base), .mem_base = (_mem_base), .mem_size = (_mem_size), .clk = _clk,    \
  }

/** @} */
