/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_nrf5 nRF5
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the nRF5 UARTE peripheral.
 *
 * Bytes are sent one at a time through EasyDMA. Reception runs through EasyDMA into the two
 * halves of a receive buffer, while TIMER @c CONFIG_UART_NRF5_RX_TIMER in counter mode counts the
 * received bytes through a PPI channel. Odd parity and flow control are not supported.
 *
 * TX is an output, idle high. RX is an input with its wiring flags, e.g. a pull, connected when
 * reception first starts. An instance connects its interrupt itself, with priority
 * @c CONFIG_UART_NRF5_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_UART_NRF5_DEFINE(s_uart0, "uart0", NRF52_UARTE0_BASE, PBL_SOC_IRQN_UART0_UARTE0, 1000000,
 *                      PBL_GPIO(&dt_gpio0, 27, 0), PBL_GPIO(&dt_gpio0, 5, 0));
 * @endcode
 * @{
 */

/** @brief Size of the receive buffer, in bytes. */
#define PBL_UART_NRF5_RX_BUF_SIZE 256U

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_nrf5_state {
  uint8_t rx_buf[PBL_UART_NRF5_RX_BUF_SIZE];
  uint32_t rx_consumed;
  uint8_t rx_next;
  uint8_t ppi_ch;
};
/** @endcond */

/** @brief nRF5 serial port. */
struct pbl_uart_nrf5 {
  /** Serial port. */
  struct pbl_uart uart;
  /** Driver runtime state. */
  struct pbl_uart_nrf5_state *state;
  /** TX pin. */
  struct pbl_gpio tx;
  /** RX pin. */
  struct pbl_gpio rx;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_nrf5_ops;
/** @endcond */

/**
 * @brief Define a serial port on an nRF5 UARTE instance.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _tx TX pin, from PBL_GPIO().
 * @param _rx RX pin, from PBL_GPIO().
 */
#define PBL_UART_NRF5_DEFINE(sym, _name, _base, _irq, _baudrate, _tx, _rx)     \
  PBL_UART_STATE_DEFINE(sym);                                                  \
  static struct pbl_uart_nrf5_state sym##_nrf5_state;                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_nrf5, struct pbl_uart, uart){       \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_nrf5_ops, _baudrate, nullptr), \
    .state = &sym##_nrf5_state,                                                \
    .base = (_base),                                                           \
    .irq = (_irq),                                                             \
    .tx = _tx,                                                                 \
    .rx = _rx,                                                                 \
  };                                                                           \
  /* A second port would share the TIMER: it fails to link */                  \
  const uint8_t pbl_uart_nrf5_one_port_per_rx_timer = 0;                       \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_NRF5_IRQ_PRIO, pbl_uart_nrf5_irq_handler, &sym##_inst, 0)

/**
 * @brief UARTE interrupt handler, connected by PBL_UART_NRF5_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_nrf5_irq_handler(const struct pbl_uart_nrf5 *uart);

/** @} */
