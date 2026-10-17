/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>
#include <pbl/util/misc.h>

//! The counter reload value. The timer will count from 0 to this value and then reset again.
#define TIMER_PERIOD_RESOLUTION 1024U

//! The number of periods we have per second.
#define PWM_OUTPUT_FREQUENCY_HZ 256U

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

  pwm_init(&dev->pwm, TIMER_PERIOD_RESOLUTION, TIMER_PERIOD_RESOLUTION * PWM_OUTPUT_FREQUENCY_HZ);

  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  const struct pbl_backlight_pwm *dev = prv_get(bl);

  if (brightness == 0U) {
    pwm_enable(&dev->pwm, false);
    if (pbl_gpio_is_connected(&dev->enable)) {
      (void)pbl_gpio_set(&dev->enable, false);
    }
    return 0;
  }

  if (pbl_gpio_is_connected(&dev->enable)) {
    (void)pbl_gpio_set(&dev->enable, true);
  }

  pwm_enable(&dev->pwm, true);
  pwm_set_duty_cycle(&dev->pwm, (uint32_t)brightness * dev->max_duty_cycle_percent *
                                    TIMER_PERIOD_RESOLUTION / 10000U);

  return 0;
}

const struct pbl_backlight_ops pbl_backlight_pwm_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
};
