/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include "drivers/button_id.h"

#include "stm32f2xx_gpio.h"

#include <stdint.h>
#include <stdbool.h>

#define GPIO_Port_NULL ((GPIO_TypeDef *) 0)
#define GPIO_Pin_NULL ((uint16_t)0x0000)

typedef struct {
  const char* const name; ///< Name for debugging purposes.
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} ButtonConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} ButtonComConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} InputConfig;

// Power Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const InputConfig vusb_stat;
  const bool wake_on_usb_power;
} BoardConfigPower;

// Button Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const ButtonConfig buttons[NUM_BUTTONS];
  const ButtonComConfig button_com;
} BoardConfigButton;

#include "board_definitions.h"
