/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>
#include <pbl/util/misc.h>

static const struct pbl_backlight_pwm *prv_get(const struct pbl_backlight *bl) {
  return container_of(bl, const struct pbl_backlight_pwm, backlight);
}

static int prv_init(const struct pbl_backlight *bl) {
  const struct pbl_backlight_pwm *dev = prv_get(bl);
  int ret;

  if (pbl_gpio_is_connected(&dev->enable)) {
    ret = pbl_gpio_configure(&dev->enable, PBL_GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_device_init(&dev->pwm.ctrl->dev);
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  const struct pbl_backlight_pwm *dev = prv_get(bl);
  struct pbl_pwm_state state;
  int ret;

  if (brightness == 0U) {
    ret = pbl_pwm_disable(&dev->pwm);
    if (ret != 0) {
      return ret;
    }

    if (pbl_gpio_is_connected(&dev->enable)) {
      (void)pbl_gpio_set(&dev->enable, false);
    }

    return 0;
  }

  if (pbl_gpio_is_connected(&dev->enable)) {
    (void)pbl_gpio_set(&dev->enable, true);
  }

  pbl_pwm_init_state(&dev->pwm, &state);
  ret = pbl_pwm_set_relative_duty_cycle(&state, (uint32_t)brightness * dev->max_duty_cycle_percent,
                                        PBL_BACKLIGHT_BRIGHTNESS_MAX * 100U);
  if (ret != 0) {
    return ret;
  }

  state.enabled = true;

  return pbl_pwm_apply(&dev->pwm, &state);
}

const struct pbl_backlight_ops pbl_backlight_pwm_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
};
