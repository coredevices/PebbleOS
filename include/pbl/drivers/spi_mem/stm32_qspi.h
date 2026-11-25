/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/spi_mem.h>

/**
 * @defgroup drivers_spi_mem_stm32_qspi STM32 QUADSPI
 * @ingroup drivers_spi_mem
 * @brief @ref drivers_spi_mem driver for the STM32 QUADSPI controller.
 *
 * Operations run in indirect mode, polled, on the memory of bank 1, with the controller clock
 * divided down to the frequency of the memory and sampling half a cycle late. The controller does
 * not map the memory.
 *
 * @code{.c}
 * PBL_SPI_MEM_STM32_QSPI_DEFINE(s_qspi, "qspi", 0x52005000,
 *                               PBL_CLK(STM32H7_RCC, STM32H7_CLK_QUADSPI_CK), &s_qspi_default,
 *                               50000000);
 * @endcode
 * @{
 */

/** @brief STM32 QUADSPI controller. */
struct pbl_spi_mem_stm32_qspi {
  /** SPI memory controller. */
  struct pbl_spi_mem mem;
  /** Registers. */
  uintptr_t base;
  /** Kernel clock. */
  struct pbl_clk clk;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** Highest frequency of the memory, in Hz. */
  uint32_t max_freq_hz;
  /** Whether two memories, one on each bank, take every other byte. */
  bool dual_flash;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_stm32_qspi_ops;
/** @endcond */

/**
 * @brief Define an STM32 QUADSPI controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _pinctrl Default pin control state.
 * @param _max_freq_hz Highest frequency of the memory, in Hz.
 * @param _dual_flash Whether two memories, one on each bank, take every other byte.
 */
#define PBL_SPI_MEM_STM32_QSPI_DEFINE(sym, _name, _base, _clk, _pinctrl, _max_freq_hz, \
                                      _dual_flash)                                     \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_mem_stm32_qspi, struct pbl_spi_mem, mem) {   \
    .mem = PBL_SPI_MEM_INIT(sym, _name, nullptr, &pbl_spi_mem_stm32_qspi_ops, 0U),     \
    .base = (_base), .clk = _clk, .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),        \
    .max_freq_hz = (_max_freq_hz), .dual_flash = (_dual_flash),                        \
  }

/** @} */
