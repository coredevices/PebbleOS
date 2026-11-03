/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/rtc/posix.h>

#include <posix_host.h>
#include <sys/time.h>

// The host clock, plus whatever offset the firmware sets.
static int64_t s_offset_s;

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value) {
  if (reg_id < POSIX_HOST_RETAINED_WORDS) {
    posix_host_retained()[reg_id] = value;
  }
}

uint32_t RTC_ReadBackupRegister(uint32_t reg_id) {
  return reg_id < POSIX_HOST_RETAINED_WORDS ? posix_host_retained()[reg_id] : 0;
}

static int prv_get_time(const struct pbl_rtc *rtc, time_t *t) {
  struct timeval now;

  gettimeofday(&now, nullptr);
  *t = (time_t)(now.tv_sec + s_offset_s);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *rtc, time_t t) {
  struct timeval now;

  gettimeofday(&now, nullptr);
  s_offset_s = (int64_t)t - now.tv_sec;

  return 0;
}

static const struct pbl_rtc_ops s_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

PBL_DEVICE_STATE_DEFINE(pbl_rtc_posix);
const struct pbl_rtc pbl_rtc_posix = PBL_RTC_INIT(pbl_rtc_posix, "rtc", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_rtc_posix, &pbl_rtc_posix.dev);
