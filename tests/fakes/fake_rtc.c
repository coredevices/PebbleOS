/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_rtc.h"

#include <string.h>

static uint64_t s_tick_count;
static uint64_t s_tick_auto_increment;

static time_t s_time_base;
static uint16_t s_time_ms_base;
static TimezoneInfo s_tzinfo;

// Kernel wall clock and uptime
////////////////////////////////////

time_t pbl_realtime(void) {
  return s_time_base;
}

uint64_t pbl_realtime_ms(void) {
  return (uint64_t)s_time_base * 1000U + s_time_ms_base;
}

void pbl_realtime_set(time_t t) {
  s_time_base = t;
}

uint64_t pbl_uptime_ticks(void) {
  uint64_t result = s_tick_count;
  s_tick_count += s_tick_auto_increment;
  return result;
}

time_t sys_get_time(void) {
  return pbl_realtime();
}

uint64_t sys_get_ticks(void) {
  return pbl_uptime_ticks();
}

// Timezone storage
////////////////////////////////////

void tz_storage_save(const TimezoneInfo *tz_info) {
  s_tzinfo = *tz_info;
}

void tz_storage_load(TimezoneInfo *tz_info) {
  *tz_info = s_tzinfo;
}

bool tz_storage_is_set(void) {
  return memcmp(s_tzinfo.tm_zone, "\0\0\0\0", 4) != 0;
}

void tz_storage_clear(void) {
  memset(&s_tzinfo, 0, sizeof(s_tzinfo));
}

// Fake functions
////////////////////////////////////

void fake_rtc_init(uint64_t initial_ticks, time_t initial_time) {
  s_tick_count = initial_ticks;
  s_time_base = initial_time;
  s_time_ms_base = 0;
}

void fake_rtc_increment_time(time_t inc) {
  s_time_base += inc;
}

void fake_rtc_increment_time_ms(uint32_t inc) {
  const uint32_t new_ms = s_time_ms_base + inc;
  if (new_ms >= 1000) {
    s_time_base += new_ms / 1000;
  }
  s_time_ms_base = new_ms % 1000;
}

void fake_rtc_set_ticks(uint64_t new_ticks) {
  s_tick_count = new_ticks;
}

void fake_rtc_increment_ticks(uint64_t tick_increment) {
  s_tick_count += tick_increment;
}

void fake_rtc_auto_increment_ticks(uint64_t auto_increment) {
  s_tick_auto_increment = auto_increment;
}
