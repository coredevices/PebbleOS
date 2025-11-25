/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button.h"
#include "util/misc.h"

#include "stm32f2xx_rcc.h"
#include "stm32f2xx_gpio.h"
#include "stm32f2xx_syscfg.h"

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .vusb_stat = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_12,
  },
  .wake_on_usb_power = false
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK]    = { "Back",   GPIOC, GPIO_Pin_3 },
    [BUTTON_ID_UP]      = { "Up",     GPIOA, GPIO_Pin_2 },
    [BUTTON_ID_SELECT]  = { "Select", GPIOC, GPIO_Pin_6 },
    [BUTTON_ID_DOWN]    = { "Down",   GPIOA, GPIO_Pin_1 },
  },

  .button_com = { GPIOA, GPIO_Pin_0 }
};
