/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include <stdint.h>

#define MAX_GPIO (9)


static uint8_t s_gpio_clock_count[MAX_GPIO];

void gpio_use(GPIO_TypeDef* GPIOx) {
  uint8_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  if((idx < MAX_GPIO) && !(s_gpio_clock_count[idx]++)) {
    SET_BIT(RCC->AHB1ENR, (0x1 << idx));
  }
}

void gpio_release(GPIO_TypeDef* GPIOx) {
  uint8_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  if((idx < MAX_GPIO) && s_gpio_clock_count[idx] && !(--s_gpio_clock_count[idx])) {
    CLEAR_BIT(RCC->AHB1ENR, (0x1 << idx));
  }
}

void gpio_output_init(OutputConfig pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = pin_config.gpio_pin,
    .GPIO_Mode = GPIO_Mode_OUT,
    .GPIO_Speed = speed,
    .GPIO_OType = otype,
    .GPIO_PuPd = GPIO_PuPd_NOPULL
  };

  gpio_use(pin_config.gpio);
  GPIO_Init(pin_config.gpio, &init);
  gpio_release(pin_config.gpio);
}

void gpio_output_set(OutputConfig pin_config, bool asserted) {
  if (!pin_config.active_high) {
    asserted = !asserted;
  }
  gpio_use(pin_config.gpio);
  GPIO_WriteBit(pin_config.gpio, pin_config.gpio_pin,
                asserted? Bit_SET : Bit_RESET);
  gpio_release(pin_config.gpio);
}

void gpio_af_init(AfConfig af_config, GPIOOType_TypeDef otype,
                  GPIOSpeed_TypeDef speed, GPIOPuPd_TypeDef pupd) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = af_config.gpio_pin,
    .GPIO_Mode = GPIO_Mode_AF,
    .GPIO_Speed = speed,
    .GPIO_OType = otype,
    .GPIO_PuPd = pupd
  };

  gpio_use(af_config.gpio);
  GPIO_Init(af_config.gpio, &init);
  GPIO_PinAFConfig(af_config.gpio, af_config.gpio_pin_source,
                   af_config.gpio_af);
  gpio_release(af_config.gpio);
}
