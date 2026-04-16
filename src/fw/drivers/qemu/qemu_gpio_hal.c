/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"

#include "board/board.h"

#include <stddef.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// GPIO MMIO register offsets
#define GPIO_STATE   0x00  // r: bit per button
#define GPIO_OUTPUT  0x04  // w: output state bits

void gpio_use(GPIO_TypeDef *gpios) {
  (void)gpios;
}

void gpio_release(GPIO_TypeDef *gpios) {
  (void)gpios;
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {
  (void)pin_config;
  (void)otype;
  (void)speed;
}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
  if (pin_config == NULL) {
    return;
  }
  uint32_t output = REG32(QEMU_GPIO_BASE + GPIO_OUTPUT);
  bool drive_high = pin_config->active_high ? asserted : !asserted;
  if (drive_high) {
    output |= (1U << pin_config->gpio_pin);
  } else {
    output &= ~(1U << pin_config->gpio_pin);
  }
  REG32(QEMU_GPIO_BASE + GPIO_OUTPUT) = output;
}

void gpio_af_init(const AfConfig *af_config, GPIOOType_TypeDef otype,
                  GPIOSpeed_TypeDef speed, GPIOPuPd_TypeDef pupd) {
  (void)af_config;
  (void)otype;
  (void)speed;
  (void)pupd;
}

void gpio_af_configure_low_power(const AfConfig *af_config) {
  (void)af_config;
}

void gpio_af_configure_fixed_output(const AfConfig *af_config, bool asserted) {
  (void)af_config;
  (void)asserted;
}

void gpio_init_all(void) {
  // Nothing to configure for QEMU
}

void gpio_input_init(const InputConfig *input_cfg) {
  (void)input_cfg;
}

void gpio_input_init_pull_up_down(const InputConfig *input_cfg, GPIOPuPd_TypeDef pupd) {
  (void)input_cfg;
  (void)pupd;
}

bool gpio_input_read(const InputConfig *input_cfg) {
  if (input_cfg == NULL) {
    return false;
  }
  uint32_t state = REG32(QEMU_GPIO_BASE + GPIO_STATE);
  return (state & (1U << input_cfg->gpio_pin)) != 0;
}

void gpio_analog_init(const InputConfig *input_cfg) {
  (void)input_cfg;
}
