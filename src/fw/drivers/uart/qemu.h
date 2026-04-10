/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "drivers/uart.h"

typedef struct UARTDeviceState {
  UARTRXInterruptHandler rx_irq_handler;
  UARTTXInterruptHandler tx_irq_handler;
  bool rx_int_enabled;
  bool tx_int_enabled;
} UARTDeviceState;

// UARTDevice is already forward-declared as a typedef in board_qemu.h;
// provide the full struct definition here.
struct UARTDevice {
  UARTDeviceState *state;
  uint32_t base_addr;
  int irqn;
  int irq_priority;
};

// Called from the IRQ handler trampoline (IRQ_MAP)
void uart_irq_handler(UARTDevice *dev);
