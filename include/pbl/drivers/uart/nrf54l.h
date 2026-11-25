/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_nrf54l nRF54L
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the nRF54L UARTE peripheral.
 *
 * Bytes are sent one at a time through EasyDMA. Reception runs through EasyDMA into the two
 * halves of a receive buffer in turn, each ending when its half is full or the line idles: only
 * then does EasyDMA write out the bytes it holds. Odd parity and flow control are not supported.
 *
 * TX is an output, idle high. RX is an input with its wiring flags, e.g. a pull, connected when
 * reception first starts. An instance connects its interrupt itself, with priority
 * @c CONFIG_UART_NRF54L_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_UART_NRF54L_DEFINE(s_uart20, "uart20", 0x500c6000, PBL_SOC_IRQN_SERIAL20, 115200,
 *                        PBL_GPIO(&dt_gpio1, 4, 0), PBL_GPIO(&dt_gpio1, 5, 0));
 * @endcode
 * @{
 */

/** @brief Size of the receive buffer, in bytes. */
#define PBL_UART_NRF54L_RX_BUF_SIZE 256U

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_nrf54l_state {
  uint8_t rx_buf[PBL_UART_NRF54L_RX_BUF_SIZE];
  uint8_t rx_half;
};
/** @endcond */

/** @brief nRF54L serial port. */
struct pbl_uart_nrf54l {
  /** Serial port. */
  struct pbl_uart uart;
  /** Driver runtime state. */
  struct pbl_uart_nrf54l_state *state;
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
extern const struct pbl_uart_ops pbl_uart_nrf54l_ops;
/** @endcond */

/**
 * @brief Define a serial port on an nRF54L UARTE instance.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _tx TX pin, from PBL_GPIO().
 * @param _rx RX pin, from PBL_GPIO().
 */
#define PBL_UART_NRF54L_DEFINE(sym, _name, _base, _irq, _baudrate, _tx, _rx)                       \
  PBL_UART_STATE_DEFINE(sym);                                                                      \
  static struct pbl_uart_nrf54l_state sym##_nrf54l_state;                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_nrf54l, struct pbl_uart, uart){                         \
    .uart = PBL_UART_INIT(sym, _name, nullptr, &pbl_uart_nrf54l_ops, _baudrate),                   \
    .state = &sym##_nrf54l_state,                                                                  \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .tx = _tx,                                                                                     \
    .rx = _rx,                                                                                     \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_NRF54L_IRQ_PRIO, pbl_uart_nrf54l_irq_handler, &sym##_inst, \
                      0)

/**
 * @brief UARTE interrupt handler, connected by PBL_UART_NRF54L_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_nrf54l_irq_handler(const struct pbl_uart_nrf54l *uart);

/** @} */
