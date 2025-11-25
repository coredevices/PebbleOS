/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_rtc.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static RtcTicks s_rtc_tick_count;
static RtcTicks s_rtc_auto_increment = 0;

// Defined identically to the static variables in rtc.c
static time_t s_time_base = 0;
static int16_t s_time_ms_base = 0;
static int64_t s_time_tick_base = 0;
static TimezoneInfo s_tzinfo = { {0} };

/*
// TODO: Unused right now
void rtc_init(void);
void rtc_init_timers(void);
void rtc_set_time_tm(struct tm* time_tm);


void rtc_next_tick_alarm_init(void);
void rtc_set_next_tick_alarm(RtcTicks tick);
void rtc_disable_next_tick_alarm(void);
bool rtc_is_tick_alarm_initialized(void);

bool rtc_is_lse_started(void);

//! @param buffer Buffer used to write the string into. Must be at least TIME_STRING_BUFFER_SIZE
const char* time_t_to_string(char* buffer, time_t t);
*/


// Stubs
////////////////////////////////////
//! @param buffer Buffer used to write the string into. Must be at least TIME_STRING_BUFFER_SIZE
const char* rtc_get_time_string(char* buffer) {return NULL;}

void rtc_get_time_tm(struct tm* time_tm) {
  if (time_tm) {
    time_t temp = rtc_get_time();
    gmtime_r(&temp, time_tm);
  }
}

void rtc_set_time(time_t time) {
  s_time_base = time;
}

void rtc_set_timezone(TimezoneInfo *tzinfo) {
  s_tzinfo = *tzinfo;
}

bool rtc_is_timezone_set(void) {
  // The actual driver checks for the first 4 chars as a uint32_t being 0
  return memcmp(s_tzinfo.tm_zone, "\0\0\0\0", 4) != 0;
}

void rtc_get_timezone(TimezoneInfo *tzinfo) {
  *tzinfo = s_tzinfo;
}

void rtc_timezone_clear(void) {
  memset(&s_tzinfo, 0, sizeof(s_tzinfo));
}

bool rtc_sanitize_struct_tm(struct tm *t) {
  return false;
}

bool rtc_sanitize_time_t(time_t *t) {
  return rtc_sanitize_struct_tm(NULL);
}

uint16_t rtc_get_timezone_id(void) {
  if (rtc_is_timezone_set()) {
    return s_tzinfo.timezone_id;
  }
  return -1;
}

//! @return millisecond port of the current second.
void rtc_get_time_ms(time_t* out_seconds, uint16_t* out_ms) {
  *out_ms = s_time_ms_base;
  *out_seconds = s_time_base;
}

// @return a unix timestamp from the host machine
time_t rtc_get_time(void) {
  return s_time_base;
}

time_t sys_get_time(void) {
  return rtc_get_time();
}

//! @return Absolute number of ticks since system start.
RtcTicks rtc_get_ticks(void) {
  RtcTicks result = s_rtc_tick_count;
  s_rtc_tick_count += s_rtc_auto_increment;
  return result;
}

RtcTicks sys_get_ticks(void) {
  return rtc_get_ticks();
}

//
// Fake Functions!
//
void fake_rtc_init(RtcTicks initial_ticks, time_t initial_time) {
  s_rtc_tick_count = initial_ticks;
  s_time_tick_base = initial_ticks;
  s_time_base = initial_time;
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

void fake_rtc_set_ticks(RtcTicks new_ticks) {
  s_rtc_tick_count = new_ticks;
}

void fake_rtc_increment_ticks(RtcTicks tick_increment) {
  s_rtc_tick_count += tick_increment;
}

void fake_rtc_auto_increment_ticks(RtcTicks auto_increment) {
  s_rtc_auto_increment = auto_increment;
}
