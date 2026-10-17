/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/posix.h>

static int prv_init(const struct pbl_backlight *bl) {
  return 0;
}

static int prv_set_brightness(const struct pbl_backlight *bl, uint8_t brightness) {
  return 0;
}

static const struct pbl_backlight_ops s_ops = {
  .init = prv_init,
  .set_brightness = prv_set_brightness,
};

PBL_BACKLIGHT_STATE_DEFINE(pbl_backlight_posix);
const struct pbl_backlight pbl_backlight_posix =
    PBL_BACKLIGHT_INIT(pbl_backlight_posix, "backlight", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_backlight_posix, &pbl_backlight_posix.dev);
