/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/uart.h>

/**
 * @defgroup drivers_uart_nrf5 nRF5
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the nRF5 UARTE peripheral.
 *
 * Bytes are sent one at a time through EasyDMA. Reception runs through EasyDMA into the two
 * halves of a receive buffer, while a TIMER in counter mode counts the received bytes through a
 * PPI channel. Odd parity and flow control are not supported.
 *
 * TX is an output, idle high. RX is an input with its wiring flags, e.g. a pull, connected when
 * reception first starts. The UARTE interrupt is connected by the board:
 *
 * @code{.c}
 * PBL_UART_NRF5_DEFINE(s_uart0, "uart0", 0, 1000000, PBL_GPIO(NRF5_GPIO_P0, 27, 0),
 *                      PBL_GPIO(NRF5_GPIO_P0, 5, 0), 2, nullptr);
 * PBL_IRQ_CONNECT(UART0_UARTE0, 7, pbl_uart_nrf5_irq_handler, &s_uart0, 0);
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
  /** UARTE instance index. */
  uint8_t index;
  /** Index of the TIMER instance counting received bytes. */
  uint8_t timer;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_nrf5_ops;
/** @endcond */

/**
 * @brief Define a serial port on an nRF5 UARTE instance.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _idx UARTE instance index.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _tx TX pin, from PBL_GPIO().
 * @param _rx RX pin, from PBL_GPIO().
 * @param _timer Index of a TIMER instance for the driver's use.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_UART_NRF5_DEFINE(sym, _name, _idx, _baudrate, _tx, _rx, _timer, _deps) \
  PBL_UART_STATE_DEFINE(sym);                                                      \
  static struct pbl_uart_nrf5_state sym##_nrf5_state;                              \
  const struct pbl_uart_nrf5 sym = {                                               \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_nrf5_ops, _baudrate, _deps),       \
    .state = &sym##_nrf5_state,                                                    \
    .tx = _tx,                                                                     \
    .rx = _rx,                                                                     \
    .index = (_idx),                                                               \
    .timer = (_timer),                                                             \
  };                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.uart.dev)

/**
 * @brief UARTE interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param uart Port.
 */
void pbl_uart_nrf5_irq_handler(const struct pbl_uart_nrf5 *uart);

/** @} */
