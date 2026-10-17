/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/backlight.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_backlight_class, "backlight");

static void prv_lock(const struct pbl_backlight *bl) {
  pbl_mutex_lock(&bl->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_backlight *bl) {
  pbl_mutex_unlock(&bl->state->lock);
}

int pbl_backlight_dev_init(const struct pbl_device *dev) {
  const struct pbl_backlight *bl = container_of(dev, const struct pbl_backlight, dev);
  int ret;

  prv_lock(bl);
  ret = bl->ops->init(bl);
  prv_unlock(bl);

  return ret;
}

int pbl_backlight_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  int ret;

  if (!pbl_device_is_ready(&bl->dev)) {
    return -ENODEV;
  }

  if (brightness > PBL_BACKLIGHT_BRIGHTNESS_MAX) {
    brightness = PBL_BACKLIGHT_BRIGHTNESS_MAX;
  }

  prv_lock(bl);
  ret = bl->ops->set_brightness(bl, brightness);
  if (ret == 0) {
    bl->state->brightness = brightness;
  }
  prv_unlock(bl);

  return ret;
}

uint8_t pbl_backlight_get_level(const struct pbl_backlight *bl, uint8_t brightness) {
  if (brightness > PBL_BACKLIGHT_BRIGHTNESS_MAX) {
    brightness = PBL_BACKLIGHT_BRIGHTNESS_MAX;
  }

  if (bl->ops->get_level == nullptr) {
    return brightness;
  }

  return bl->ops->get_level(bl, brightness);
}

int pbl_backlight_refresh(const struct pbl_backlight *bl) {
  int ret;

  if (!pbl_device_is_ready(&bl->dev)) {
    return -ENODEV;
  }

  if (bl->ops->refresh == nullptr) {
    return 0;
  }

  prv_lock(bl);
  ret = (bl->state->brightness != 0U) ? bl->ops->refresh(bl) : 0;
  prv_unlock(bl);

  return ret;
}

int pbl_backlight_set_color(const struct pbl_backlight *bl, uint32_t rgb_color) {
  int ret;

  if (!pbl_device_is_ready(&bl->dev)) {
    return -ENODEV;
  }

  if (bl->ops->set_color == nullptr) {
    return -ENOTSUP;
  }

  prv_lock(bl);
  ret = bl->ops->set_color(bl, rgb_color);
  if (ret == 0) {
    bl->state->color = rgb_color;
  }
  prv_unlock(bl);

  return ret;
}

uint32_t pbl_backlight_get_color(const struct pbl_backlight *bl) {
  return bl->state->color;
}
