/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/mcu/cache.h>

/**
 * @defgroup drivers_uart_sf32lb SF32LB
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the SF32LB USART.
 *
 * Bytes are sent polled. Received bytes come from the receive interrupt or, with a receive DMA
 * channel, from a cyclic transfer into a receive buffer, drained when the line goes idle and at
 * each half of the buffer. Deep sleep is blocked while reception is enabled. An instance connects
 * its interrupt itself, with priority @c CONFIG_UART_SF32LB_IRQ_PRIO:
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_usart1_rx_dma =
 *     PBL_DMA_CHAN(&dt_dmac1, 0, SF32LB52_DMA_REQ_USART1_RX, SF32LB52_DMA_PL_MEDIUM);
 *
 * PBL_UART_SF32LB_DEFINE(s_usart1, "usart1", SF32LB52_USART1_BASE, PBL_SOC_IRQN_USART1,
 *                        PBL_CLK(nullptr, 0), 1000000, &s_usart1_default, &s_usart1_rx_dma);
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
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Clock, without a controller when it cannot be gated. */
  struct pbl_clk clk;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** Receive DMA channel, or NULL. */
  const struct pbl_dma_chan *rx_dma;
  /** Receive buffer of @ref PBL_UART_SF32LB_RX_BUF_SIZE bytes, with a receive DMA channel. */
  uint8_t *rx_buf;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_sf32lb_ops;

#define PBL_UART_SF32LB_FIRST_(_first, ...) _first
/** @endcond */

/**
 * @brief Define a serial port on an SF32LB USART.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _clk Clock, from PBL_CLK(), with a NULL controller when it cannot be gated.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _pinctrl Default pin control state.
 * @param ... Receive DMA channel, if any, which also gives the port its receive buffer.
 */
#define PBL_UART_SF32LB_DEFINE(sym, _name, _base, _irq, _clk, _baudrate, _pinctrl, ...)            \
  PBL_UART_STATE_DEFINE(sym);                                                                      \
  static struct pbl_uart_sf32lb_state sym##_sf32lb_state;                                          \
  __VA_OPT__(                                                                                      \
      static uint8_t sym##_rx_buf[PBL_UART_SF32LB_RX_BUF_SIZE] PBL_ALIGNED(DCACHE_LINE_SIZE_MAX);) \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_sf32lb, struct pbl_uart, uart){                         \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_sf32lb_ops, _baudrate, nullptr),                   \
    .state = &sym##_sf32lb_state,                                                                  \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                  \
    .rx_dma = PBL_UART_SF32LB_FIRST_(__VA_ARGS__ __VA_OPT__(, ) nullptr),                          \
    .rx_buf = PBL_UART_SF32LB_FIRST_(__VA_OPT__(sym##_rx_buf, ) nullptr),                          \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_SF32LB_IRQ_PRIO, pbl_uart_sf32lb_irq_handler, &sym##_inst, \
                      0)

/**
 * @brief USART interrupt handler, connected by PBL_UART_SF32LB_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_sf32lb_irq_handler(const struct pbl_uart_sf32lb *uart);

/** @} */
