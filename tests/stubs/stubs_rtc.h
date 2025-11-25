/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/time/time.h"

void rtc_init(void) {
}

void rtc_init_timers(void) {
}

bool rtc_sanitize_struct_tm(struct tm* t) {
  return false;
}

bool rtc_sanitize_time_t(time_t* t) {
  return false;
}

void rtc_set_time(time_t time) {
}

time_t rtc_get_time(void) {
  return 0;
}

// Wrappers for the above functions that take struct tm instead of time_t
void rtc_set_time_tm(struct tm* time_tm) {
}

void rtc_get_time_tm(struct tm* time_tm) {
}

void rtc_get_time_ms(time_t* out_seconds, uint16_t* out_ms) {
}

void rtc_set_timezone(TimezoneInfo *tzinfo) {
}

void rtc_get_timezone(TimezoneInfo *tzinfo) {
}

uint16_t rtc_get_timezone_id(void) {
  return 0;
}

bool rtc_is_timezone_set(void) {
  return false;
}

// RTC ticks
///////////////////////////////////////////////////////////////////////////////

RtcTicks rtc_get_ticks(void) {
  return 0;
}
