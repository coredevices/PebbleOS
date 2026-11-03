/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/time.h>
#include <pbl/services/time.h>

#include <board/board.h>

//! How frequently we save the time state to the retained registers, in ticks.
#define SAVE_TIME_FREQUENCY (30 * RTC_TICKS_HZ)

/***
 * The RTC counter is reset with the rest of the chip, so the tick count is the kernel uptime and
 * the wall clock time restarts from the last time saved to retained RAM.
 */

static bool s_did_init_rtc = false;

//! UNIX timestamp of the wall clock time at tick s_time_tick_base.
static time_t s_time_base = 0;
//! Tick at which the wall clock time was s_time_base.
static RtcTicks s_time_tick_base = 0;
//! Tick at which the time state was last saved.
static RtcTicks s_last_save_ticks = 0;

static time_t prv_ticks_to_time(RtcTicks ticks) {
  return s_time_base + (time_t)((ticks - s_time_tick_base) / RTC_TICKS_HZ);
}

static void prv_save_rtc_time_state(RtcTicks ticks) {
  if (!s_did_init_rtc) {
    return;
  }

  retained_write(CURRENT_TIME_REGISTER, prv_ticks_to_time(ticks));
  s_last_save_ticks = ticks;
}

RtcTicks rtc_get_ticks(void) {
  RtcTicks ticks = pbl_uptime_ticks();

  if (ticks - s_last_save_ticks > SAVE_TIME_FREQUENCY) {
    prv_save_rtc_time_state(ticks);
  }

  return ticks;
}

void rtc_set_time(time_t time) {
  s_time_base = time;
  s_time_tick_base = pbl_uptime_ticks();

  prv_save_rtc_time_state(s_time_tick_base);
}

time_t rtc_get_time(void) {
  return prv_ticks_to_time(rtc_get_ticks());
}

void rtc_get_time_ms(time_t *out_seconds, uint16_t *out_ms) {
  RtcTicks ticks_since_time_base = rtc_get_ticks() - s_time_tick_base;

  *out_seconds = s_time_base + (time_t)(ticks_since_time_base / RTC_TICKS_HZ);
  *out_ms = (uint16_t)(((ticks_since_time_base % RTC_TICKS_HZ) * 1000) / RTC_TICKS_HZ);
}

/*** Logic that ought be refactored into rtc_common, were it not stm32-only. ***/

bool rtc_sanitize_struct_tm(struct pbl_tm *t) {
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
  struct pbl_tm time_struct;
  pbl_gmtime_r(t, &time_struct);

  const bool result = rtc_sanitize_struct_tm(&time_struct);
  *t = pbl_mktime(&time_struct);

  return result;
}

void rtc_get_time_tm(struct pbl_tm *time_tm) {
  time_t t = rtc_get_time();
  pbl_localtime_r(&t, time_tm);
}

const char *rtc_get_time_string(char *buffer) {
  return time_t_to_string(buffer, rtc_get_time());
}

const char *time_t_to_string(char *buffer, time_t t) {
  struct pbl_tm time;
  pbl_localtime_r(&t, &time);

  pbl_strftime_r(buffer, TIME_STRING_BUFFER_SIZE, "%c", &time);

  return buffer;
}

//! We attempt to save registers by placing both the timezone abbreviation
//! timezone index and the daylight_savings_time into the same register set
void rtc_set_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t *)tzinfo;
  static_assert(sizeof(TimezoneInfo) <= 5 * sizeof(uint32_t), "RTC Set Timezone invalid data size");

  retained_write(RTC_TIMEZONE_ABBR_START, raw[0]);
  retained_write(RTC_TIMEZONE_ABBR_END_TZID_DSTID, raw[1]);
  retained_write(RTC_TIMEZONE_GMTOFFSET, raw[2]);
  retained_write(RTC_TIMEZONE_DST_START, raw[3]);
  retained_write(RTC_TIMEZONE_DST_END, raw[4]);
}

void rtc_get_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t *)tzinfo;

  raw[0] = retained_read(RTC_TIMEZONE_ABBR_START);
  raw[1] = retained_read(RTC_TIMEZONE_ABBR_END_TZID_DSTID);
  raw[2] = retained_read(RTC_TIMEZONE_GMTOFFSET);
  raw[3] = retained_read(RTC_TIMEZONE_DST_START);
  raw[4] = retained_read(RTC_TIMEZONE_DST_END);
}

void rtc_timezone_clear(void) {
  retained_write(RTC_TIMEZONE_ABBR_START, 0);
  retained_write(RTC_TIMEZONE_ABBR_END_TZID_DSTID, 0);
  retained_write(RTC_TIMEZONE_GMTOFFSET, 0);
  retained_write(RTC_TIMEZONE_DST_START, 0);
  retained_write(RTC_TIMEZONE_DST_END, 0);
}

uint16_t rtc_get_timezone_id(void) {
  return ((retained_read(RTC_TIMEZONE_ABBR_END_TZID_DSTID) >> 16) & 0xFFFF);
}

bool rtc_is_timezone_set(void) {
  // True if the timezone abbreviation has been set (including UNK for unknown)
  return (retained_read(RTC_TIMEZONE_ABBR_START) != 0);
}

void rtc_enable_backup_regs(void) {
  /* we always use retained ram for this, so no problem */
}

void rtc_calibrate_frequency(uint32_t frequency) {
  /* On nRF5, there is no way to calibrate the RTC.  That crystal had better
   * be accurate!
   */
}

void rtc_init(void) {
  if (s_did_init_rtc) {
    return;
  }

  s_time_base = retained_read(CURRENT_TIME_REGISTER);
  s_time_tick_base = pbl_uptime_ticks();
  s_did_init_rtc = true;
}

void rtc_init_timers(void) {
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
