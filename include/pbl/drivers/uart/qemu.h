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
 * holds the incoming bytes back.
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
 * @param _irq Interrupt line.
 * @param _baudrate Initial baud rate, in bits per second.
 */
#define PBL_UART_QEMU_DEFINE(sym, _name, _base, _irq, _baudrate)               \
  PBL_UART_STATE_DEFINE(sym);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_qemu, struct pbl_uart, uart){       \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_qemu_ops, _baudrate, nullptr), \
    .base = (_base),                                                           \
    .irqn = (_irq),                                                            \
  };                                                                           \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_QEMU_IRQ_PRIO, pbl_uart_qemu_irq_handler, &sym##_inst, 0)

/**
 * @brief Interrupt handler, connected by PBL_UART_QEMU_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_qemu_irq_handler(const struct pbl_uart_qemu *uart);

/** @} */
