#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "drivers/i2c_definitions.h"

typedef struct I2CBusHalState {
  I2C_HandleTypeDef hdl;
} I2CBusHalState;

typedef const struct I2CBusHal {
  I2CBusHalState *state;
  Pinmux scl;
  Pinmux sda;
  RCC_MODULE_TYPE module;
  IRQn_Type irqn;
  uint8_t irq_priority;
} I2CBusHal;

void i2c_irq_handler(I2CBus *bus);
