/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/led/gpio.h>
#include <pbl/util/misc.h>

static int prv_set(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  return pbl_gpio_set(&PBL_CONTAINER_OF(led, const struct pbl_led_gpio, led)->gpio,
                      brightness > 0U);
}

const struct pbl_led_ops pbl_led_gpio_ops = {
  .set = prv_set,
};

int pbl_led_gpio_init(const struct pbl_device *dev) {
  const struct pbl_led_gpio *led = PBL_CONTAINER_OF(dev, const struct pbl_led_gpio, led.dev);

  return pbl_gpio_configure(&led->gpio, PBL_GPIO_OUTPUT_INACTIVE);
}

#include PBL_DT_INSTANCES(gpio_leds)
