/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
  bool enable_flow_control;
  AfConfig tx_gpio;
  AfConfig rx_gpio;
  AfConfig cts_gpio;
  AfConfig rts_gpio;
  USART_TypeDef *periph;
  uint32_t rcc_apb_periph;
  uint8_t irq_channel;
  uint8_t irq_priority;
  DMARequest *rx_dma;
} UARTDevice;

// thinly wrapped by the IRQ handler in board_*.c
void uart_irq_handler(UARTDevice *dev);
