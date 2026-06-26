/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include "board/board.h"

#ifdef CONFIG_SOC_NRF52
#include <hal/nrf_gpio.h>

typedef enum {
  GPIO_OType_PP,
  GPIO_OType_OD,
} GPIOOType_TypeDef;

typedef enum {
  GPIO_PuPd_NOPULL,
  GPIO_PuPd_UP,
  GPIO_PuPd_DOWN,
} GPIOPuPd_TypeDef;

#endif

//! Initialize a GPIO as an output.
//!
//! @param pin_config the BOARD_CONFIG pin configuration struct
//! @param otype the output type of the pin (GPIO_OType_PP or GPIO_OType_OD)
void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype);

//! Assert or deassert the output pin.
//!
//! Asserting the output drives the pin high if pin_config.active_high
//! is true, and drives it low if pin_config.active_high is false.
void gpio_output_set(const OutputConfig *pin_config, bool asserted);

//! Configure gpios as inputs (suitable for things like exti lines)
void gpio_input_init(const InputConfig *input_cfg);

//! Configure gpio as an input with internal pull up/pull down configured.
void gpio_input_init_pull_up_down(const InputConfig *input_cfg, GPIOPuPd_TypeDef pupd);

//! @return bool the current state of the GPIO pin
bool gpio_input_read(const InputConfig *input_cfg);
