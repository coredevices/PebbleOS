/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include <stdint.h>

#define GPIO_EN_MASK ((RCC_AHB1ENR_GPIOHEN << 1) - 1)

void gpio_enable_all(void) {
  RCC->AHB1ENR |= GPIO_EN_MASK;
}

void gpio_disable_all(void) {
  RCC->AHB1ENR &= ~GPIO_EN_MASK;
}

void gpio_af_init(const AfConfig *af_config, GPIOOType_TypeDef otype, GPIOSpeed_TypeDef speed,
                  GPIOPuPd_TypeDef pupd) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = af_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_AF,
    .GPIO_Speed = speed,
    .GPIO_OType = otype,
    .GPIO_PuPd = pupd
  };

  GPIO_PinAFConfig(af_config->gpio, af_config->gpio_pin_source, af_config->gpio_af);
  GPIO_Init(af_config->gpio, &init);
}
