/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/uart.h"

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>


typedef struct UARTState {
  bool initialized;
  UARTRXInterruptHandler rx_irq_handler;
  UARTTXInterruptHandler tx_irq_handler;
  bool rx_int_enabled;
  bool tx_int_enabled;
  uint8_t *rx_dma_buffer;
  uint32_t rx_dma_length;
  uint32_t rx_dma_index;
} UARTDeviceState;

typedef const struct UARTDevice {
  UARTDeviceState *state;
  bool half_duplex;
  // Conveniently, the F7 supports swapping RX/TX if they get laid out incorrectly :)
  bool do_swap_rx_tx;
  AfConfig tx_gpio;
  AfConfig rx_gpio;
  USART_TypeDef *periph;
  uint32_t rcc_apb_periph;
  uint8_t irq_channel;
  uint8_t irq_priority;
  DMARequest *rx_dma;
} UARTDevice;

// thinly wrapped by the IRQ handler in board_*.c
void uart_irq_handler(UARTDevice *dev);
