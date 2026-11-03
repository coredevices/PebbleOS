/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/rtc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_rtc_class, "rtc");

int pbl_rtc_get_time(const struct pbl_rtc *rtc, time_t *t) {
  if (!pbl_device_is_ready(&rtc->dev)) {
    return -ENODEV;
  }

  return rtc->ops->get_time(rtc, t);
}

int pbl_rtc_set_time(const struct pbl_rtc *rtc, time_t t) {
  if (!pbl_device_is_ready(&rtc->dev)) {
    return -ENODEV;
  }

  return rtc->ops->set_time(rtc, t);
}
