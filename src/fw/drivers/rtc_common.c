/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rtc_private.h"
#include "drivers/rtc.h"

#include "drivers/gpio.h"
#include "drivers/pwr.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/reset.h"

#include <stdio.h>

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

bool rtc_sanitize_struct_tm(struct tm *t) {
  // These values come from time_t (which suffers from the 2038 problem) and our hardware which
  // only stores a 2 digit year, so we only represent values after 2000.

  // Remember tm_year is years since 1900.
  if (t->tm_year < 100) {
    // Bump it up to the year 2000 to work with our hardware.
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

void rtc_get_time_tm(struct tm* time_tm) {
  time_t t = rtc_get_time();
  localtime_r(&t, time_tm);
}

const char* rtc_get_time_string(char* buffer) {
  return time_t_to_string(buffer, rtc_get_time());
}

const char* time_t_to_string(char* buffer, time_t t) {
  struct tm time;
  localtime_r(&t, &time);

  strftime(buffer, TIME_STRING_BUFFER_SIZE, "%c", &time);

  return buffer;
}


//! We attempt to save registers by placing both the timezone abbreviation
//! timezone index and the daylight_savingtime into the same register set
void rtc_set_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t*)tzinfo;
  _Static_assert(sizeof(TimezoneInfo) <= 5 * sizeof(uint32_t),
      "RTC Set Timezone invalid data size");

  RTC_WriteBackupRegister(RTC_TIMEZONE_ABBR_START, raw[0]);
  RTC_WriteBackupRegister(RTC_TIMEZONE_ABBR_END_TZID_DSTID, raw[1]);
  RTC_WriteBackupRegister(RTC_TIMEZONE_GMTOFFSET, raw[2]);
  RTC_WriteBackupRegister(RTC_TIMEZONE_DST_START, raw[3]);
  RTC_WriteBackupRegister(RTC_TIMEZONE_DST_END, raw[4]);
}


void rtc_get_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t*)tzinfo;

  raw[0] = RTC_ReadBackupRegister(RTC_TIMEZONE_ABBR_START);
  raw[1] = RTC_ReadBackupRegister(RTC_TIMEZONE_ABBR_END_TZID_DSTID);
  raw[2] = RTC_ReadBackupRegister(RTC_TIMEZONE_GMTOFFSET);
  raw[3] = RTC_ReadBackupRegister(RTC_TIMEZONE_DST_START);
  raw[4] = RTC_ReadBackupRegister(RTC_TIMEZONE_DST_END);
}

void rtc_timezone_clear(void) {
  RTC_WriteBackupRegister(RTC_TIMEZONE_ABBR_START, 0);
  RTC_WriteBackupRegister(RTC_TIMEZONE_ABBR_END_TZID_DSTID, 0);
  RTC_WriteBackupRegister(RTC_TIMEZONE_GMTOFFSET, 0);
  RTC_WriteBackupRegister(RTC_TIMEZONE_DST_START, 0);
  RTC_WriteBackupRegister(RTC_TIMEZONE_DST_END, 0);
}

uint16_t rtc_get_timezone_id(void) {
  return ((RTC_ReadBackupRegister(RTC_TIMEZONE_ABBR_END_TZID_DSTID) >> 16) & 0xFFFF);
}

bool rtc_is_timezone_set(void) {
  // True if the timezone abbreviation has been set (including UNK for unknown)
  return (RTC_ReadBackupRegister(RTC_TIMEZONE_ABBR_START) != 0);
}

void rtc_enable_backup_regs(void) {
  pwr_access_backup_domain(true);
}
