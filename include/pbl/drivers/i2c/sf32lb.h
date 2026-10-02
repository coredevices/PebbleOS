/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "definitions.h"

/**
 * @defgroup drivers_i2c_sf32lb SF32LB I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c_hal implementation for the SF32LB I2C controller.
 *
 * Deep sleep is blocked while a transfer is in flight.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct I2CBusHalState {
  I2C_HandleTypeDef hdl;
  bool deepsleep_blocked;
} I2CBusHalState;
/** @endcond */

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
} I2CBusHal;

/**
 * @brief Controller interrupt handler.
 *
 * @param bus Bus.
 */
void i2c_irq_handler(I2CBus *bus);

/** @} */
