/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"
#include <pbl/drivers/uart.h>

/**
 * @defgroup drivers_uart_posix POSIX
 * @ingroup drivers_uart
 * @brief UART backed by a host file descriptor.
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
  /** Host file descriptor the transmitted bytes go to. */
  int fd;
  /** Receives what is typed on the host's terminal. */
  bool console;
};

/** @} */
