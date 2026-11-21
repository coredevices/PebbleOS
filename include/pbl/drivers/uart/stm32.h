/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_stm32 STM32
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the STM32H7 USART and UART.
 *
 * Bytes are sent polled, with the FIFOs off. Received bytes come from the receive interrupt or,
 * with a receive DMA channel, from a cyclic transfer into a receive buffer, drained when the line
 * goes idle and at each half of the buffer. The baud rate comes from the kernel clock of the port.
 * An instance connects its interrupt itself, with priority @c CONFIG_UART_STM32_IRQ_PRIO:
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_usart1_rx_dma =
 *     PBL_DMA_CHAN(&s_dmamux1, 41, 41, DMA_SCR_TRBUFF);
 *
 * PBL_UART_STM32_DEFINE(s_usart1, "usart1", 0x40011000, PBL_SOC_IRQN_USART1,
 *                       PBL_CLK(STM32H7_RCC, STM32H7_CLK_USART1_CK), 115200,
 *                       &s_usart1_default, &s_usart1_rx_dma);
 * @endcode
 * @{
 */

/** @brief Size of the receive buffer of a port with a receive DMA channel, in bytes. */
#define PBL_UART_STM32_RX_BUF_SIZE 256U

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_stm32_state {
  bool tx_pending;
  uint32_t rx_pos;
};
/** @endcond */

/** @brief STM32 serial port. */
struct pbl_uart_stm32 {
  /** Serial port. */
  struct pbl_uart uart;
  /** Driver runtime state. */
  struct pbl_uart_stm32_state *state;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Kernel clock. */
  struct pbl_clk clk;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** Receive DMA channel, or NULL. */
  const struct pbl_dma_chan *rx_dma;
  /** Receive buffer of @ref PBL_UART_STM32_RX_BUF_SIZE bytes, with a receive DMA channel. */
  uint8_t *rx_buf;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_stm32_ops;

#define PBL_UART_STM32_FIRST_(_first, ...) _first
/** @endcond */

/**
 * @brief Define a serial port on an STM32 USART or UART.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _pinctrl Default pin control state.
 * @param ... Receive DMA channel, if any, which also gives the port its receive buffer.
 */
#define PBL_UART_STM32_DEFINE(sym, _name, _base, _irq, _clk, _baudrate, _pinctrl, ...)            \
  PBL_UART_STATE_DEFINE(sym);                                                                     \
  static struct pbl_uart_stm32_state sym##_stm32_state;                                           \
  __VA_OPT__(                                                                                     \
      static uint8_t sym##_rx_buf[PBL_UART_STM32_RX_BUF_SIZE] PBL_ALIGNED(PBL_DCACHE_LINE_SIZE);) \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_stm32, struct pbl_uart, uart){                         \
    .uart = PBL_UART_INIT(sym, _name, nullptr, &pbl_uart_stm32_ops, _baudrate),                   \
    .state = &sym##_stm32_state,                                                                  \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .clk = _clk,                                                                                  \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                 \
    .rx_dma = PBL_UART_STM32_FIRST_(__VA_ARGS__ __VA_OPT__(, ) nullptr),                          \
    .rx_buf = PBL_UART_STM32_FIRST_(__VA_OPT__(sym##_rx_buf, ) nullptr),                          \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_STM32_IRQ_PRIO, pbl_uart_stm32_irq_handler, &sym##_inst, 0)

/**
 * @brief USART interrupt handler, connected by PBL_UART_STM32_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_stm32_irq_handler(const struct pbl_uart_stm32 *uart);

/** @} */
