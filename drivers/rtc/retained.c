/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/rtc/retained.h>
#include <pbl/kernel/time.h>

#include <system/rtc_registers.h>

// The time last set, and the uptime when it was: a reset restarts the uptime, so the time
// resumes from the last value set.
static uint64_t s_set_uptime_ms;

static int prv_get_time(const struct pbl_rtc *rtc, time_t *t) {
  *t = (time_t)retained_read(CURRENT_TIME_REGISTER) +
       (time_t)((pbl_uptime_ms() - s_set_uptime_ms) / 1000U);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *rtc, time_t t) {
  retained_write(CURRENT_TIME_REGISTER, (uint32_t)t);
  s_set_uptime_ms = pbl_uptime_ms();

  return 0;
}

static const struct pbl_rtc_ops s_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

PBL_DEVICE_STATE_DEFINE(pbl_rtc_retained);
const struct pbl_rtc pbl_rtc_retained = PBL_RTC_INIT(pbl_rtc_retained, "rtc", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_rtc_retained, &pbl_rtc_retained.dev);
