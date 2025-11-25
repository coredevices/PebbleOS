/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// ----------------------------------------------
//  Board definitions for Robert BB (C2 Bigboard)
// ----------------------------------------------
//

#include "util/size.h"

#define BOARD_LSE_MODE RCC_LSE_Bypass

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK] = {
      .input = {
        .gpio = GPIOG,
        .gpio_pin = GPIO_Pin_6,
      },
      .pupd = GPIO_PuPd_UP
    },
    [BUTTON_ID_UP] = {
      .input = {
        .gpio = GPIOG,
        .gpio_pin = GPIO_Pin_3,
      },
      .pupd = GPIO_PuPd_NOPULL
    },
    [BUTTON_ID_SELECT] = {
      .input = {
        .gpio = GPIOG,
        .gpio_pin = GPIO_Pin_5,
      },
      .pupd = GPIO_PuPd_UP
    },
    [BUTTON_ID_DOWN] = {
      .input = {
        .gpio = GPIOG,
        .gpio_pin = GPIO_Pin_4,
      },
      .pupd = GPIO_PuPd_UP
    },
  },
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .rail_4V5_ctrl = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_5,
    .active_high = true,
  },
  .rail_6V6_ctrl = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_3,
    .active_high = true,
  },

};

static const BoardConfigFlash BOARD_CONFIG_FLASH = {
};

static const BoardConfigAccessory BOARD_CONFIG_ACCESSORY = {
  .power_en = { GPIOA, GPIO_Pin_11, true },
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
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_SCLK] = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ0] = {
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_11,
    .gpio_pin_source = GPIO_PinSource11,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ1] = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ2] = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_7,
    .gpio_pin_source = GPIO_PinSource7,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  [QSpiPin_DQ3] = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
};

extern I2CSlavePort * const I2C_MAX14690;
extern ICE40LPDevice * const ICE40LP;
