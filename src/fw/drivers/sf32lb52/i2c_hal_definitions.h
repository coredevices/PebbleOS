#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/board.h"
#include "drivers/i2c_definitions.h"

typedef struct I2CState {
  bool int_enabled;
  bool initialized;
} I2CDeviceState;

typedef const struct I2CBusHal {
  I2CDeviceState *i2c_state;
  I2C_HandleTypeDef hi2c;
  DMA_HandleTypeDef hdma;
  const void *dev;
  Pinmux scl;
  Pinmux sda;
  uint8_t core;
  RCC_MODULE_TYPE module;
  IRQn_Type irqn;
  uint8_t irq_priority;
  IRQn_Type dma_irqn;
  uint8_t dma_irq_priority;
  uint32_t timeout;
} I2CDeviceBusHal;

void i2c_irq_handler(I2CBus *bus);
void i2c_dma_irq_handler(I2CBus *bus);
