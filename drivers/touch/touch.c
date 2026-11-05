/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/touch.h>
#include <pbl/input/input.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_touch_class, "touch");

void pbl_touch_lock(const struct pbl_touch *touch) {
  pbl_mutex_lock(&touch->state->lock, PBL_FOREVER);
}

void pbl_touch_unlock(const struct pbl_touch *touch) {
  pbl_mutex_unlock(&touch->state->lock);
}

int pbl_touch_dev_init(const struct pbl_device *dev) {
  const struct pbl_touch *touch = container_of(dev, const struct pbl_touch, dev);
  int ret;

  pbl_touch_lock(touch);
  ret = touch->ops->init(touch);
  pbl_touch_unlock(touch);

  return ret;
}

int pbl_touch_enable(const struct pbl_touch *touch) {
  int ret = 0;

  if (!pbl_device_is_ready(&touch->dev)) {
    return -ENODEV;
  }

  pbl_touch_lock(touch);
  if (touch->state->users == 0U) {
    ret = touch->ops->enable(touch);
  }
  if (ret == 0) {
    touch->state->users++;
  }
  pbl_touch_unlock(touch);

  return ret;
}

int pbl_touch_disable(const struct pbl_touch *touch) {
  int ret = 0;

  if (!pbl_device_is_ready(&touch->dev)) {
    return -ENODEV;
  }

  pbl_touch_lock(touch);
  if (touch->state->users == 0U) {
    pbl_touch_unlock(touch);
    return -EALREADY;
  }

  touch->state->users--;
  if (touch->state->users == 0U) {
    ret = touch->ops->disable(touch);
    touch->state->last.down = false;
  }
  pbl_touch_unlock(touch);

  return ret;
}

bool pbl_touch_is_enabled(const struct pbl_touch *touch) {
  bool enabled;

  pbl_touch_lock(touch);
  enabled = touch->state->users != 0U;
  pbl_touch_unlock(touch);

  return enabled;
}

void pbl_touch_get_last(const struct pbl_touch *touch, struct pbl_touch_sample *sample) {
  pbl_touch_lock(touch);
  *sample = touch->state->last;
  pbl_touch_unlock(touch);
}

static void prv_apply_props(const struct pbl_touch_props *props, uint16_t *x, uint16_t *y) {
  uint16_t tmp;

  if (props->inverted_x) {
    *x = (*x < props->size_x) ? (props->size_x - 1U - *x) : 0U;
  }

  if (props->inverted_y) {
    *y = (*y < props->size_y) ? (props->size_y - 1U - *y) : 0U;
  }

  if (props->swapped_x_y) {
    tmp = *x;
    *x = *y;
    *y = tmp;
  }
}

void pbl_touch_report(const struct pbl_touch *touch, bool down, uint16_t x, uint16_t y) {
  prv_apply_props(&touch->props, &x, &y);

  pbl_touch_lock(touch);
  touch->state->last = (struct pbl_touch_sample){.down = down, .x = (int16_t)x, .y = (int16_t)y};
  pbl_touch_unlock(touch);

  pbl_input_report_key(PBL_INPUT_BTN_TOUCH, down, false);
  pbl_input_report_abs(PBL_INPUT_ABS_X, x, false);
  pbl_input_report_abs(PBL_INPUT_ABS_Y, y, true);
}

void pbl_touch_report_gesture(const struct pbl_touch *touch, uint16_t code, uint16_t x,
                              uint16_t y) {
  prv_apply_props(&touch->props, &x, &y);

  pbl_input_report_abs(PBL_INPUT_ABS_X, x, false);
  pbl_input_report_abs(PBL_INPUT_ABS_Y, y, false);
  pbl_input_report_ges(code, true);
}
