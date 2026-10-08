/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/uart.h>

#include <board/board.h>

/**
 * @defgroup drivers_uart_posix POSIX
 * @ingroup drivers_uart
 * @brief UART on the host: the terminal, a TCP port or a serial device.
 * @{
 */

/** @brief POSIX UART state, owned by the driver. */
typedef struct UARTDeviceState {
  /** Receive handler. */
  UARTRXInterruptHandler rx_irq_handler;
  /** Receive interrupt enabled. */
  bool rx_int_enabled;
} UARTDeviceState;

/** @brief POSIX UART device. */
struct UARTDevice {
  /** Driver state. */
  UARTDeviceState *state;
  /** Host end: 0 for the console, 1 for the QEMU serial protocol, 2 for Bluetooth HCI. */
  uint8_t channel;
};

/** @} */
