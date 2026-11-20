/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_stm32 STM32
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the STM32H7 USART and UART.
 *
 * Bytes are sent polled and received from the receive interrupt, with the FIFOs off. The baud rate
 * comes from the kernel clock of the port. An instance connects its interrupt itself, with
 * priority @c CONFIG_UART_STM32_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_UART_STM32_DEFINE(s_usart1, "usart1", 0x40011000, PBL_SOC_IRQN_USART1,
 *                       PBL_CLK(STM32H7_RCC, STM32H7_CLK_USART1_CK), 115200,
 *                       &s_usart1_default);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_stm32_state {
  bool tx_pending;
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
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_stm32_ops;
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
 */
#define PBL_UART_STM32_DEFINE(sym, _name, _base, _irq, _clk, _baudrate, _pinctrl) \
  PBL_UART_STATE_DEFINE(sym);                                                     \
  static struct pbl_uart_stm32_state sym##_stm32_state;                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_stm32, struct pbl_uart, uart){         \
    .uart = PBL_UART_INIT(sym, _name, nullptr, &pbl_uart_stm32_ops, _baudrate),   \
    .state = &sym##_stm32_state,                                                  \
    .base = (_base),                                                              \
    .irq = (_irq),                                                                \
    .clk = _clk,                                                                  \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                 \
  };                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_STM32_IRQ_PRIO, pbl_uart_stm32_irq_handler, &sym##_inst, 0)

/**
 * @brief USART interrupt handler, connected by PBL_UART_STM32_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_stm32_irq_handler(const struct pbl_uart_stm32 *uart);

/** @} */
