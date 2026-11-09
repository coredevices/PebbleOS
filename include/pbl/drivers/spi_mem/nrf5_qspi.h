/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/spi_mem.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>

/**
 * @defgroup drivers_spi_mem_nrf5_qspi nRF5 QSPI
 * @ingroup drivers_spi_mem
 * @brief @ref drivers_spi_mem controller driver for the nRF5 QSPI peripheral.
 *
 * Runs operations with up to 8 bytes after a single line opcode as custom instructions, and reads,
 * programs and erases with the opcodes the peripheral issues itself. Its read window is the READ
 * task, covering the whole memory. An instance connects its interrupt itself, with priority
 * @c CONFIG_SPI_MEM_NRF5_QSPI_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_SPI_MEM_NRF5_QSPI_DEFINE(s_qspi, "qspi", NRF52_QSPI_BASE, PBL_SOC_IRQN_QSPI, 8000000,
 *                              PBL_GPIO(&dt_gpio0, 19, 0), PBL_GPIO(&dt_gpio0, 17, 0),
 *                              PBL_GPIO(&dt_gpio0, 20, 0), PBL_GPIO(&dt_gpio0, 21, 0),
 *                              PBL_GPIO(&dt_gpio0, 22, 0), PBL_GPIO(&dt_gpio0, 23, 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_mem_nrf5_qspi_data {
  struct pbl_sem done;
  uint32_t psel[6];
  uint8_t readoc;
  uint8_t writeoc;
  bool addr32;
  uint8_t dirmap_readoc;
};
/** @endcond */

/** @brief nRF5 QSPI controller. */
struct pbl_spi_mem_nrf5_qspi {
  /** Controller. */
  struct pbl_spi_mem mem;
  /** Driver state. */
  struct pbl_spi_mem_nrf5_qspi_data *data;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Clock frequency, in Hz. */
  uint32_t freq_hz;
  /** SCK pin. */
  struct pbl_gpio sck;
  /** CSN pin. */
  struct pbl_gpio csn;
  /** IO0 to IO3 pins. */
  struct pbl_gpio io[4];
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_nrf5_qspi_ops;
/** @endcond */

/**
 * @brief Define the nRF5 QSPI controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _freq_hz Clock frequency, in Hz: 2 to 32 MHz.
 * @param _sck SCK pin, from PBL_GPIO().
 * @param _csn CSN pin, from PBL_GPIO().
 * @param _io0 IO0 pin, from PBL_GPIO().
 * @param _io1 IO1 pin, from PBL_GPIO().
 * @param _io2 IO2 pin, from PBL_GPIO().
 * @param _io3 IO3 pin, from PBL_GPIO().
 */
#define PBL_SPI_MEM_NRF5_QSPI_DEFINE(sym, _name, _base, _irq, _freq_hz, _sck, _csn, _io0, _io1,   \
                                     _io2, _io3)                                                  \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                                  \
  static struct pbl_spi_mem_nrf5_qspi_data sym##_data = {.done = PBL_SEM_INITIALIZER(0, 1)};      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_mem_nrf5_qspi, struct pbl_spi_mem, mem){                \
    .mem = PBL_SPI_MEM_INIT(sym, _name, nullptr, &pbl_spi_mem_nrf5_qspi_ops, 0U),                 \
    .data = &sym##_data,                                                                          \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .freq_hz = (_freq_hz),                                                                        \
    .sck = _sck,                                                                                  \
    .csn = _csn,                                                                                  \
    .io = {_io0, _io1, _io2, _io3},                                                               \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_SPI_MEM_NRF5_QSPI_IRQ_PRIO, pbl_spi_mem_nrf5_qspi_irq_handler, \
                      &sym##_inst, 0)

/**
 * @brief QSPI interrupt handler, connected by PBL_SPI_MEM_NRF5_QSPI_DEFINE().
 *
 * @param qspi Controller.
 */
void pbl_spi_mem_nrf5_qspi_irq_handler(const struct pbl_spi_mem_nrf5_qspi *qspi);

/** @} */
