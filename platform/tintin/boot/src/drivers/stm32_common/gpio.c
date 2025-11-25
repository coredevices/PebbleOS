/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include <stdint.h>

#define MAX_GPIO (9)


static uint32_t s_gpio_clock_count[MAX_GPIO];

void gpio_use(GPIO_TypeDef* GPIOx) {
  uint32_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  if ((idx < MAX_GPIO) && !(s_gpio_clock_count[idx]++)) {
    SET_BIT(RCC->AHB1ENR, (0x1 << idx));
  }
}

void gpio_release(GPIO_TypeDef* GPIOx) {
  uint32_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  if ((idx < MAX_GPIO) && s_gpio_clock_count[idx] && !(--s_gpio_clock_count[idx])) {
    CLEAR_BIT(RCC->AHB1ENR, (0x1 << idx));
  }
}
