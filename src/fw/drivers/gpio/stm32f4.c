/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include <stdint.h>

#define MAX_GPIO (9)

#include "FreeRTOS.h"
#include "debug/power_tracking.h"

static const PowerSystem s_gpio_to_power_system_map[MAX_GPIO] = {
  [0] = PowerSystemMcuGpioA,
  [1] = PowerSystemMcuGpioB,
  [2] = PowerSystemMcuGpioC,
  [3] = PowerSystemMcuGpioD,
  [7] = PowerSystemMcuGpioH,
};

static void prv_init_common(const InputConfig *input_config, GPIO_InitTypeDef *gpio_init) {
  gpio_use(input_config->gpio);
  GPIO_Init(input_config->gpio, gpio_init);
  gpio_release(input_config->gpio);
}

static uint8_t s_gpio_clock_count[MAX_GPIO];

void gpio_use(GPIO_TypeDef* GPIOx) {
  uint8_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  portENTER_CRITICAL();
  if((idx < MAX_GPIO) && !(s_gpio_clock_count[idx]++)) {
    SET_BIT(RCC->AHB1ENR, (0x1 << idx));
    power_tracking_start(s_gpio_to_power_system_map[idx]);
  }
  portEXIT_CRITICAL();
}

void gpio_release(GPIO_TypeDef* GPIOx) {
  uint8_t idx = ((((uint32_t)GPIOx) - AHB1PERIPH_BASE) / 0x0400);
  portENTER_CRITICAL();
  if((idx < MAX_GPIO) && s_gpio_clock_count[idx] && !(--s_gpio_clock_count[idx])) {
    CLEAR_BIT(RCC->AHB1ENR, (0x1 << idx));
    power_tracking_stop(s_gpio_to_power_system_map[idx]);
  }
  portEXIT_CRITICAL();
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = pin_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_OUT,
    .GPIO_Speed = speed,
    .GPIO_OType = otype,
    .GPIO_PuPd = GPIO_PuPd_NOPULL
  };

  gpio_use(pin_config->gpio);
  GPIO_Init(pin_config->gpio, &init);
  gpio_release(pin_config->gpio);
}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
  if (!pin_config->active_high) {
    asserted = !asserted;
  }
  gpio_use(pin_config->gpio);
  GPIO_WriteBit(pin_config->gpio, pin_config->gpio_pin,
                asserted? Bit_SET : Bit_RESET);
  gpio_release(pin_config->gpio);
}

void gpio_af_init(const AfConfig *af_config, GPIOOType_TypeDef otype,
                  GPIOSpeed_TypeDef speed, GPIOPuPd_TypeDef pupd) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = af_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_AF,
    .GPIO_Speed = speed,
    .GPIO_OType = otype,
    .GPIO_PuPd = pupd
  };

  gpio_use(af_config->gpio);
  GPIO_PinAFConfig(af_config->gpio, af_config->gpio_pin_source,
                   af_config->gpio_af);
  GPIO_Init(af_config->gpio, &init);
  gpio_release(af_config->gpio);
}

void gpio_af_configure_low_power(const AfConfig *af_config) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = af_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_AN,
    .GPIO_Speed = GPIO_Speed_2MHz,
    .GPIO_PuPd  = GPIO_PuPd_NOPULL
  };

  gpio_use(af_config->gpio);
  GPIO_Init(af_config->gpio, &init);
  gpio_release(af_config->gpio);
}

void gpio_af_configure_fixed_output(const AfConfig *af_config, bool asserted) {
  GPIO_InitTypeDef init = {
    .GPIO_Pin = af_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_OUT,
    .GPIO_Speed = GPIO_Speed_2MHz,
    .GPIO_OType = GPIO_OType_PP,
    .GPIO_PuPd = GPIO_PuPd_NOPULL
  };

  gpio_use(af_config->gpio);
  GPIO_Init(af_config->gpio, &init);
  GPIO_WriteBit(af_config->gpio, af_config->gpio_pin,
                asserted? Bit_SET : Bit_RESET);
  gpio_release(af_config->gpio);
}

void gpio_input_init(const InputConfig *input_config) {
  if (input_config->gpio == NULL) {
    // TODO: PBL-15063
    return;
  }

  gpio_input_init_pull_up_down(input_config, GPIO_PuPd_NOPULL);
}

void gpio_input_init_pull_up_down(const InputConfig *input_config, GPIOPuPd_TypeDef pupd) {
  GPIO_InitTypeDef gpio_init = {
    .GPIO_Mode = GPIO_Mode_IN,
    .GPIO_PuPd = pupd,
    .GPIO_Pin = input_config->gpio_pin
  };

  prv_init_common(input_config, &gpio_init);
}

bool gpio_input_read(const InputConfig *input_config) {
  gpio_use(input_config->gpio);
  uint8_t bit = GPIO_ReadInputDataBit(input_config->gpio, input_config->gpio_pin);
  gpio_release(input_config->gpio);

  return bit != 0;
}

void gpio_analog_init(const InputConfig *input_config) {
  GPIO_InitTypeDef gpio_init = {
    .GPIO_Pin = input_config->gpio_pin,
    .GPIO_Mode = GPIO_Mode_AN,
    .GPIO_Speed = GPIO_Speed_2MHz,
    .GPIO_PuPd  = GPIO_PuPd_NOPULL
  };

  prv_init_common(input_config, &gpio_init);
}
