/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/led.h>

PBL_DEVICE_CLASS_DEFINE(pbl_led_class, "led");

int pbl_led_set(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  if (!pbl_device_is_ready(&led->dev)) {
    return -ENODEV;
  }

  return led->ops->set(led, brightness, rgb_color);
}

int pbl_led_refresh(const struct pbl_led *led, uint8_t brightness, uint32_t rgb_color) {
  if (!pbl_device_is_ready(&led->dev)) {
    return -ENODEV;
  }

  if (led->ops->refresh == nullptr) {
    return led->ops->set(led, brightness, rgb_color);
  }

  return led->ops->refresh(led, brightness, rgb_color);
}
