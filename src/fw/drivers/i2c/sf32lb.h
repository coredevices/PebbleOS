/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "definitions.h"

typedef struct I2CBusHalState {
  I2C_HandleTypeDef hdl;
  DMA_HandleTypeDef hdma_tx;
  DMA_HandleTypeDef hdma_rx;
  struct dma_config dma_tx;
  struct dma_config dma_rx;
} I2CBusHalState;

typedef const struct I2CBusHal {
  I2CBusHalState *state;
  Pinmux scl;
  Pinmux sda;
  RCC_MODULE_TYPE module;
  IRQn_Type irqn;
  uint8_t irq_priority;
  IRQn_Type dma_tx_irqn;
  IRQn_Type dma_rx_irqn;
  uint8_t dma_irq_priority;
} I2CBusHal;

void i2c_irq_handler(I2CBus *bus);
void i2c_dma_tx_irq_handler(I2CBus *bus);
void i2c_dma_rx_irq_handler(I2CBus *bus);
