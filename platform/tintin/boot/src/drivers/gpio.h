/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include "stm32f2xx_gpio.h"

#include "board/board.h"

void gpio_use(GPIO_TypeDef* GPIOx);
void gpio_release(GPIO_TypeDef* GPIOx);
