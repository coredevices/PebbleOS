/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_qemu QEMU
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the QEMU simple UART.
 *
 * The port has no line settings: any baud rate is accepted. While reception is disabled, QEMU
 * holds the incoming bytes back. The interrupt is connected by the board:
 *
 * @code{.c}
 * PBL_UART_QEMU_DEFINE(s_uart2, "uart2", QEMU_UART2_BASE, PBL_IRQN(UART2), 115200);
 * PBL_IRQ_CONNECT(UART2, 5, pbl_uart_qemu_irq_handler, &s_uart2, 0);
 * @endcode
 * @{
 */

/** @brief QEMU serial port. */
struct pbl_uart_qemu {
  /** Serial port. */
  struct pbl_uart uart;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irqn;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU serial port.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Register base address.
 * @param _irqn Interrupt line, from PBL_IRQN().
 * @param _baudrate Initial baud rate, in bits per second.
 */
#define PBL_UART_QEMU_DEFINE(sym, _name, _base, _irqn, _baudrate)              \
  PBL_UART_STATE_DEFINE(sym);                                                  \
  const struct pbl_uart_qemu sym = {                                           \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_qemu_ops, _baudrate, nullptr), \
    .base = (_base),                                                           \
    .irqn = (_irqn),                                                           \
  };                                                                           \
  PBL_DEVICE_REGISTER(sym, &sym.uart.dev)

/**
 * @brief Interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param uart Port.
 */
void pbl_uart_qemu_irq_handler(const struct pbl_uart_qemu *uart);

/** @} */
