/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum I2CDutyCycle {
  I2CDutyCycle_16_9,
  I2CDutyCycle_2
} I2CDutyCycle;

typedef struct I2CBusHal {
  I2C_TypeDef *const i2c;
  uint32_t clock_ctrl;  ///< Peripheral clock control flag
  uint32_t clock_speed; ///< Bus clock speed
  I2CDutyCycle duty_cycle;  ///< Bus clock duty cycle in fast mode
  IRQn_Type ev_irq_channel; ///< I2C Event interrupt (One of X_IRQn). For example, I2C1_EV_IRQn.
  IRQn_Type er_irq_channel; ///< I2C Error interrupt (One of X_IRQn). For example, I2C1_ER_IRQn.
} I2CBusHal;

void i2c_hal_event_irq_handler(I2CBus *device);
void i2c_hal_error_irq_handler(I2CBus *device);
