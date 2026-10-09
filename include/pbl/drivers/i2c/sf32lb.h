/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// clang-format off
#include <stdint.h>

#include <board/board.h>
#include "definitions.h"
// clang-format on

/**
 * @defgroup drivers_i2c_sf32lb SF32LB I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c_hal implementation for the SF32LB I2C controller.
 *
 * Deep sleep is blocked while a transfer is in flight. A bus given a receive DMA channel serves
 * i2c_read_register_block_dma() reads of @ref I2C_SF32LB_DMA_MIN_BYTES or more through DMA, so the
 * CPU can sleep through the transfer instead of taking an interrupt per byte.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct I2CBusHalState {
  I2C_HandleTypeDef hdl;
  bool deepsleep_blocked;
  /** Receive DMA; the board sets Instance and Init.Request to enable it. */
  DMA_HandleTypeDef hdma_rx;
  /** Buffer of the DMA read in flight, NULL otherwise. */
  uint8_t *dma_data;
  uint32_t dma_size;
} I2CBusHalState;
/** @endcond */

/** @brief Shortest read that goes through DMA. */
#define I2C_SF32LB_DMA_MIN_BYTES 32

/** @brief SF32LB bus configuration. */
typedef const struct I2CBusHal {
  /** Driver runtime state. */
  I2CBusHalState *state;
  /** SCL pin. */
  Pinmux scl;
  /** SDA pin. */
  Pinmux sda;
  /** Controller clock module. */
  RCC_MODULE_TYPE module;
  /** Controller interrupt. */
  IRQn_Type irqn;
  /** Receive DMA interrupt, when the bus has a receive DMA channel. */
  IRQn_Type dma_irqn;
} I2CBusHal;

/**
 * @brief Controller interrupt handler.
 *
 * @param bus Bus.
 */
void i2c_irq_handler(I2CBus *bus);

/**
 * @brief Receive DMA interrupt handler.
 *
 * @param bus Bus.
 */
void i2c_dma_irq_handler(I2CBus *bus);

/** @} */
