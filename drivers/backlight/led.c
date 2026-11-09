/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/led.h>
#include <pbl/drivers/led.h>
#include <pbl/util/misc.h>

static const struct pbl_led *prv_led(const struct pbl_backlight *bl) {
  return container_of(bl, const struct pbl_backlight_led, backlight)->led;
}

static uint8_t prv_led_brightness(uint8_t brightness) {
  return (uint8_t)((brightness * PBL_LED_BRIGHTNESS_MAX + PBL_BACKLIGHT_BRIGHTNESS_MAX / 2U) /
                   PBL_BACKLIGHT_BRIGHTNESS_MAX);
}

static int prv_init(const struct pbl_backlight *bl) {
  return pbl_device_init(&prv_led(bl)->dev);
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  if (brightness == bl->state->brightness) {
    return 0;
  }

  return pbl_led_set(prv_led(bl), prv_led_brightness(brightness), bl->state->color);
}

static int prv_refresh(const struct pbl_backlight *bl) {
  return pbl_led_refresh(prv_led(bl), prv_led_brightness(bl->state->brightness), bl->state->color);
}

static int prv_set_color(const struct pbl_backlight *bl, uint32_t rgb_color) {
  return pbl_led_set(prv_led(bl), prv_led_brightness(bl->state->brightness), rgb_color);
}

const struct pbl_backlight_ops pbl_backlight_led_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
  .refresh = prv_refresh,
  .set_color = prv_set_color,
};

#include PBL_DT_INSTANCES(led_backlight)
