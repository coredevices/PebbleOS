/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include "stm32f4xx_gpio.h"

#include "board/board.h"

void gpio_enable_all(void);
void gpio_disable_all(void);
void gpio_af_init(const AfConfig *af_config, GPIOOType_TypeDef otype, GPIOSpeed_TypeDef speed,
                  GPIOPuPd_TypeDef pupd);
