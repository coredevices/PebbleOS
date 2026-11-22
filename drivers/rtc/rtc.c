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

int pbl_rtc_set_alarm(const struct pbl_rtc *rtc, time_t t, pbl_rtc_alarm_callback_t cb,
                      void *user_data) {
  if (!pbl_device_is_ready(&rtc->dev)) {
    return -ENODEV;
  }

  if (rtc->ops->set_alarm == nullptr) {
    return -ENOTSUP;
  }

  return rtc->ops->set_alarm(rtc, t, cb, user_data);
}

int pbl_rtc_cancel_alarm(const struct pbl_rtc *rtc) {
  if (!pbl_device_is_ready(&rtc->dev)) {
    return -ENODEV;
  }

  if (rtc->ops->cancel_alarm == nullptr) {
    return -ENOTSUP;
  }

  return rtc->ops->cancel_alarm(rtc);
}
