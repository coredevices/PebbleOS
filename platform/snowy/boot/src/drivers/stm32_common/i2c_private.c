/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include "drivers/pmic.h"
#include "drivers/gpio.h"
#include "util/delay.h"
#include "board/board.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_gpio.h"
#include "stm32f2xx_rcc.h"
#include "stm32f2xx_i2c.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"
#include "drivers/pmic.h"
#endif

extern void i2c_bus_rail_ctl_config(OutputConfig pin_config);

static void do_rail_power(bool up, GPIO_TypeDef* const gpio, const uint32_t gpio_pin, const bool active_high) {
  if (up) {
    gpio_use(gpio);
    // enable the bus supply
    GPIO_WriteBit(gpio, gpio_pin, active_high ? Bit_SET : Bit_RESET);

    // wait for the bus supply to stabilize and the peripherals to start up.
    // the MFI chip requires its reset pin to be stable for at least 10ms from startup.
    delay_ms(20);
    gpio_release(gpio);
  } else {
    gpio_use(gpio);
    // disable the bus supply
    GPIO_WriteBit(gpio, gpio_pin, active_high ? Bit_RESET : Bit_SET);
    gpio_release(gpio);
  }
}

// SNOWY
/////////
void snowy_i2c_rail_1_ctl_fn(bool enable) {
  set_ldo3_power_state(enable);
}

// bb2
/////////
void bb2_rail_ctl_fn(bool enable) {
  do_rail_power(enable, GPIOH, GPIO_Pin_0, true);
}

void bb2_rail_cfg_fn(void) {
  i2c_bus_rail_ctl_config((OutputConfig){ GPIOH, GPIO_Pin_0, true});
}

// v1_5
/////////
void v1_5_rail_ctl_fn(bool enable) {
  do_rail_power(enable, GPIOH, GPIO_Pin_0, true);
}

void v1_5_rail_cfg_fn(void) {
  i2c_bus_rail_ctl_config((OutputConfig){ GPIOH, GPIO_Pin_0, true});
}

// v2_0
/////////
void v2_0_rail_ctl_fn(bool enable) {
  do_rail_power(enable, GPIOH, GPIO_Pin_0, true);
}

void v2_0_rail_cfg_fn(void) {
  i2c_bus_rail_ctl_config((OutputConfig){ GPIOH, GPIO_Pin_0, true});
}

// ev2_4
/////////
void ev2_4_rail_ctl_fn(bool enable) {
  do_rail_power(enable, GPIOH, GPIO_Pin_0, true);
}

void ev2_4_rail_cfg_fn(void) {
  i2c_bus_rail_ctl_config((OutputConfig){ GPIOH, GPIO_Pin_0, true});
}

// bigboard
////////////
void bigboard_rail_ctl_fn(bool enable) {
  do_rail_power(enable, GPIOC, GPIO_Pin_5, true);
}

void bigboard_rail_cfg_fn(void) {
  i2c_bus_rail_ctl_config((OutputConfig){ GPIOC, GPIO_Pin_5, true});
}
