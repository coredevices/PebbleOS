/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/compiler.h>
#include <pbl/mcu/cache.h>

/**
 * @defgroup drivers_uart_sf32lb SF32LB
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the SF32LB USART.
 *
 * Bytes are sent polled. Received bytes come from the receive interrupt or, with a receive DMA
 * channel, from a cyclic transfer into a receive buffer, drained when the line goes idle and at
 * each half of the buffer. Deep sleep is blocked while reception is enabled. The USART interrupt,
 * and the DMA channel's, are connected by the board with the same priority:
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_uart1_rx_dma =
 *     PBL_DMA_CHAN(SF32LB52_DMAC1, 0, SF32LB52_DMA_REQ_USART1_RX, SF32LB52_DMA_PL_MEDIUM);
 *
 * PBL_UART_SF32LB_DMA_DEFINE(s_uart1, "uart1", 1, 1000000, &s_uart1_default, &s_uart1_rx_dma,
 *                            nullptr);
 * PBL_IRQ_CONNECT(USART1, 5, pbl_uart_sf32lb_irq_handler, &s_uart1, 0);
 * PBL_IRQ_CONNECT(DMAC1_CH1, 5, pbl_dma_sf32lb52_irq_handler, &s_uart1_rx_dma, 0);
 * @endcode
 * @{
 */

/** @brief Size of the receive buffer of a port with a receive DMA channel, in bytes. */
#define PBL_UART_SF32LB_RX_BUF_SIZE 256U

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_sf32lb_state {
  uint32_t rx_pos;
  bool tx_pending;
};
/** @endcond */

/** @brief SF32LB serial port. */
struct pbl_uart_sf32lb {
  /** Serial port. */
  struct pbl_uart uart;
  /** Driver runtime state. */
  struct pbl_uart_sf32lb_state *state;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** Receive DMA channel, or NULL. */
  const struct pbl_dma_chan *rx_dma;
  /** Receive buffer of @ref PBL_UART_SF32LB_RX_BUF_SIZE bytes, with a receive DMA channel. */
  uint8_t *rx_buf;
  /** USART instance, from 1. */
  uint8_t index;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_sf32lb_ops;

#define PBL_UART_SF32LB_DEFINE_(sym, _name, _idx, _baudrate, _pinctrl, _rx_dma, _rx_buf, _deps) \
  PBL_UART_STATE_DEFINE(sym);                                                                   \
  static struct pbl_uart_sf32lb_state sym##_sf32lb_state;                                       \
  const struct pbl_uart_sf32lb sym = {                                                          \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_sf32lb_ops, _baudrate, _deps),                  \
    .state = &sym##_sf32lb_state,                                                               \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                               \
    .rx_dma = (_rx_dma),                                                                        \
    .rx_buf = (_rx_buf),                                                                        \
    .index = (_idx),                                                                            \
  };                                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.uart.dev)
/** @endcond */

/**
 * @brief Define a serial port on an SF32LB USART, receiving through DMA.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _idx USART instance, from 1.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _pinctrl Default pin control state.
 * @param _rx_dma Receive DMA channel.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_UART_SF32LB_DMA_DEFINE(sym, _name, _idx, _baudrate, _pinctrl, _rx_dma, _deps)     \
  static uint8_t sym##_rx_buf[PBL_UART_SF32LB_RX_BUF_SIZE] PBL_ALIGNED(DCACHE_LINE_SIZE_MAX); \
  PBL_UART_SF32LB_DEFINE_(sym, _name, _idx, _baudrate, _pinctrl, _rx_dma, sym##_rx_buf, _deps)

/**
 * @brief Define a serial port on an SF32LB USART.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _idx USART instance, from 1.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _pinctrl Default pin control state.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_UART_SF32LB_DEFINE(sym, _name, _idx, _baudrate, _pinctrl, _deps) \
  PBL_UART_SF32LB_DEFINE_(sym, _name, _idx, _baudrate, _pinctrl, nullptr, nullptr, _deps)

/**
 * @brief USART interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param uart Port.
 */
void pbl_uart_sf32lb_irq_handler(const struct pbl_uart_sf32lb *uart);

/** @} */
