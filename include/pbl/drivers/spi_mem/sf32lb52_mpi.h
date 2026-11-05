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
 * With @ref PBL_SPI_MEM_XIP, the CPU executes from the memory: the boot code's clock, prescaler
 * and sampling setup are kept, the clock is never gated, and everything that runs while the
 * memory is unreadable (status polling, window changes, power down) runs from RAM with
 * interrupts disabled.
 *
 * @code{.c}
 * PBL_SPI_MEM_SF32LB52_MPI_DEFINE(s_mpi2, "mpi2", 2, 32U * 1024U * 1024U,
 *                                 PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MPI2),
 *                                 PBL_SPI_MEM_XIP, nullptr);
 * @endcode
 * @{
 */

/** @brief SF32LB52 MPI controller. */
struct pbl_spi_mem_sf32lb52_mpi {
  /** Controller. */
  struct pbl_spi_mem mem;
  /** Instance, 1 or 2. */
  uint8_t idx;
  /** Memory-mapped window size, in bytes. */
  uint32_t mem_size;
  /** Clock. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_sf32lb52_mpi_ops;
/** @endcond */

/**
 * @brief Define an SF32LB52 MPI controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _idx Instance, 1 or 2.
 * @param _mem_size Memory-mapped window size, in bytes.
 * @param _clk Clock, from PBL_CLK().
 * @param _flags Flags, @c PBL_SPI_MEM_*.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_SPI_MEM_SF32LB52_MPI_DEFINE(sym, _name, _idx, _mem_size, _clk, _flags, _deps) \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                          \
  const struct pbl_spi_mem_sf32lb52_mpi sym = {                                           \
    .mem = PBL_SPI_MEM_INIT(sym, _name, _deps, &pbl_spi_mem_sf32lb52_mpi_ops, _flags),    \
    .idx = (_idx),                                                                        \
    .mem_size = (_mem_size),                                                              \
    .clk = _clk,                                                                          \
  };                                                                                      \
  PBL_DEVICE_REGISTER(sym, &sym.mem.dev)

/** @} */
