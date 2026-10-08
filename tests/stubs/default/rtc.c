/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/services/time.h>

PBL_WEAK void rtc_init(void) {
}

PBL_WEAK void rtc_init_timers(void) {
}

PBL_WEAK bool rtc_sanitize_struct_tm(struct pbl_tm *t) {
  return false;
}

PBL_WEAK bool rtc_sanitize_time_t(time_t *t) {
  return false;
}

PBL_WEAK void rtc_set_time(time_t time) {
}

PBL_WEAK time_t rtc_get_time(void) {
  return 0;
}

// Wrappers for the above functions that take struct pbl_tm instead of time_t
PBL_WEAK void rtc_set_time_tm(struct pbl_tm *time_tm) {
}

PBL_WEAK void rtc_get_time_tm(struct pbl_tm *time_tm) {
}

PBL_WEAK void rtc_get_time_ms(time_t *out_seconds, uint16_t *out_ms) {
}

PBL_WEAK void rtc_set_timezone(TimezoneInfo *tzinfo) {
}

PBL_WEAK void rtc_get_timezone(TimezoneInfo *tzinfo) {
}

PBL_WEAK uint16_t rtc_get_timezone_id(void) {
  return 0;
}

PBL_WEAK bool rtc_is_timezone_set(void) {
  return false;
}

// RTC ticks
///////////////////////////////////////////////////////////////////////////////

PBL_WEAK RtcTicks rtc_get_ticks(void) {
  return 0;
}
