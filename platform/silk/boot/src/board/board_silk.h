/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/misc.h"

#define BOARD_LSE_MODE RCC_LSE_Bypass

#define USE_PARALLEL_FLASH 1 // FIXME PBL-28872: Hack to get the "modern" flash layout.
                             // Fix when we add support for new flash

#define BOARD_I2C_BUS_COUNT (ARRAY_LENGTH(SILK_I2C_BUS_CONFIGS))

static const I2cBusConfig SILK_I2C_BUS_CONFIGS[] = {
  // PMIC I2c
  [0] = {
    .i2c = I2C3,
    .i2c_scl = { GPIOA, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF_I2C3 },
    .i2c_sda = { GPIOB, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF9_I2C3 },
    .clock_speed = 400000,
    .duty_cycle = I2C_DutyCycle_16_9,
    .clock_ctrl = RCC_APB1Periph_I2C3,
    .ev_irq_channel = I2C3_EV_IRQn,
    .er_irq_channel = I2C3_ER_IRQn,
  },
};

static const uint8_t SILK_I2C_DEVICE_MAP[] = {
  [I2C_DEVICE_AS3701B] = 0,
};

static const BoardConfig BOARD_CONFIG = {
  .i2c_bus_configs = SILK_I2C_BUS_CONFIGS,
  .i2c_bus_count = BOARD_I2C_BUS_COUNT,
  .i2c_device_map = SILK_I2C_DEVICE_MAP,
  .i2c_device_count = ARRAY_LENGTH(SILK_I2C_DEVICE_MAP),
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK] =
        { "Back",   GPIOC, GPIO_Pin_13, { EXTI_PortSourceGPIOC, 13 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_UP] =
        { "Up",     GPIOD, GPIO_Pin_2, { EXTI_PortSourceGPIOD, 2 }, GPIO_PuPd_DOWN },
    [BUTTON_ID_SELECT] =
        { "Select", GPIOH, GPIO_Pin_0, { EXTI_PortSourceGPIOH, 0 }, GPIO_PuPd_DOWN },
    [BUTTON_ID_DOWN] =
        { "Down",   GPIOH, GPIO_Pin_1, { EXTI_PortSourceGPIOH, 1 }, GPIO_PuPd_DOWN },
  },

  .button_com = { 0 },
};

typedef enum {
  QSpiPin_CS,
  QSpiPin_SCLK,
  QSpiPin_DQ0,
  QSpiPin_DQ1,
  QSpiPin_DQ2,
  QSpiPin_DQ3,
  QSpiPinCount,
} QSpiPin;

static const AfConfig BOARD_CONFIG_FLASH_PINS[] = {
  [QSpiPin_CS] = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF10_QUADSPI,
  },
  [QSpiPin_SCLK] = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_2,
    .gpio_pin_source = GPIO_PinSource2,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ0] = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ1] = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ2] = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ3] = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
};
