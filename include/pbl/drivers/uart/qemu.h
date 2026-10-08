/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/uart.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart_qemu QEMU
 * @ingroup drivers_uart
 * @brief QEMU UART device definition.
 * @{
 */

/** @brief QEMU UART state, owned by the driver. */
typedef struct UARTDeviceState {
  /** Receive handler. */
  UARTRXInterruptHandler rx_irq_handler;
  /** Transmit handler. */
  UARTTXInterruptHandler tx_irq_handler;
  /** Receive interrupt enabled. */
  bool rx_int_enabled;
  /** Transmit interrupt enabled. */
  bool tx_int_enabled;
} UARTDeviceState;

/**
 * @brief QEMU UART device.
 *
 * Completes the UARTDevice type forward-declared by the board header.
 */
struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** Register base address. */
  uint32_t base_addr;
  /** Interrupt number. */
  int irqn;
};

/** @cond INTERNAL_HIDDEN */
/* UART interrupt handler, documented with the nRF5 backend. */
void uart_irq_handler(UARTDevice *dev);
/** @endcond */

/** @} */
