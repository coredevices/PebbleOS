/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <sys/time.h>

#include <pbl/drivers/rtc.h>

#include <pbl/services/time.h>

// Wall time follows the host clock, plus whatever offset the firmware sets.

#define NUM_BACKUP_REGS 16

static int64_t s_offset_s;
static uint32_t s_backup[NUM_BACKUP_REGS];
static struct timeval s_boot;
static TimezoneInfo s_tzinfo;
static bool s_tz_set;

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value) {
  if (reg_id < NUM_BACKUP_REGS) {
    s_backup[reg_id] = value;
  }
}

uint32_t RTC_ReadBackupRegister(uint32_t reg_id) {
  return reg_id < NUM_BACKUP_REGS ? s_backup[reg_id] : 0;
}

void rtc_init(void) {
  gettimeofday(&s_boot, NULL);
}

void rtc_calibrate_frequency(uint32_t frequency) {
}

void rtc_init_timers(void) {
}

void rtc_set_time(time_t time) {
  struct timeval now;
  gettimeofday(&now, NULL);
  s_offset_s = (int64_t)time - now.tv_sec;
}

void rtc_get_time_ms(time_t *out_seconds, uint16_t *out_ms) {
  struct timeval now;
  gettimeofday(&now, NULL);
  *out_seconds = (time_t)(now.tv_sec + s_offset_s);
  *out_ms = (uint16_t)(now.tv_usec / 1000);
}

time_t rtc_get_time(void) {
  time_t t;
  uint16_t ms;
  rtc_get_time_ms(&t, &ms);
  return t;
}

void rtc_set_time_tm(struct tm *time_tm) {
  rtc_set_time(mktime(time_tm));
}

void rtc_get_time_tm(struct tm *time_tm) {
  time_t t = rtc_get_time();
  localtime_r(&t, time_tm);
}

bool rtc_sanitize_struct_tm(struct tm *t) {
  if (t->tm_year < 100) {
    t->tm_year = 100;
    return true;
  } else if (t->tm_year > 137) {
    t->tm_year = 137;
    return true;
  }
  return false;
}

bool rtc_sanitize_time_t(time_t *t) {
  struct tm time_struct;
  gmtime_r(t, &time_struct);
  const bool result = rtc_sanitize_struct_tm(&time_struct);
  *t = mktime(&time_struct);
  return result;
}

RtcTicks rtc_get_ticks(void) {
  struct timeval now;
  gettimeofday(&now, NULL);
  int64_t us = (int64_t)(now.tv_sec - s_boot.tv_sec) * 1000000 + (now.tv_usec - s_boot.tv_usec);
  return (RtcTicks)(us * RTC_TICKS_HZ / 1000000);
}

void rtc_alarm_init(void) {
}

void rtc_alarm_set(RtcTicks num_ticks) {
}

RtcTicks rtc_alarm_get_elapsed_ticks(void) {
  return 0;
}

bool rtc_alarm_is_initialized(void) {
  return true;
}

void rtc_set_timezone(TimezoneInfo *tzinfo) {
  s_tzinfo = *tzinfo;
  s_tz_set = true;
}

void rtc_get_timezone(TimezoneInfo *tzinfo) {
  *tzinfo = s_tzinfo;
}

uint16_t rtc_get_timezone_id(void) {
  return s_tzinfo.timezone_id;
}

bool rtc_is_timezone_set(void) {
  return s_tz_set;
}

void rtc_timezone_clear(void) {
  memset(&s_tzinfo, 0, sizeof(s_tzinfo));
  s_tz_set = false;
}

const char *rtc_get_time_string(char *buffer) {
  return time_t_to_string(buffer, rtc_get_time());
}

const char *time_t_to_string(char *buffer, time_t t) {
  struct tm time;
  localtime_r(&t, &time);
  strftime(buffer, TIME_STRING_BUFFER_SIZE, "%c", &time);
  return buffer;
}
