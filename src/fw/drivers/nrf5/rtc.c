#include "drivers/rtc.h"
#include "drivers/rtc_private.h"
#include "drivers/stm32f2/rtc_calibration.h"

#include "console/dbgserial.h"

#include "drivers/exti.h"
#include "drivers/periph_config.h"
#include "drivers/watchdog.h"

#include "kernel/util/stop.h"
#include "mcu/interrupts.h"

#include "services/common/regular_timer.h"

#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"

#include "util/time/time.h"

#define NRF5_COMPATIBLE
#include <mcu.h>
#include <hal/nrf_rtc.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

//! The value of the RTC registers last time we checked them.
static uint32_t s_last_ticks = 0;
//! This value is added to the current value of the RTC ticks to get the number
//! of ticks since system start. Incremented whenever we detect a rollover.
static RtcTicks s_coarse_ticks = 1;

void rtc_set_time(time_t time) {
#ifdef PBL_LOG_ENABLED
  char buffer[TIME_STRING_BUFFER_SIZE];
  PBL_LOG(LOG_LEVEL_INFO, "Setting time to %lu <%s>", time, time_t_to_string(buffer, time));
#endif
}

time_t rtc_get_time(void) {
  return 0;
}

void rtc_get_time_ms(time_t* out_seconds, uint16_t* out_ms) {
  *out_seconds = 0;
  *out_ms = 0;
}

RtcTicks rtc_get_ticks(void) {
  // Prevent this from being interrupted
  bool ints_enabled = mcu_state_are_interrupts_enabled();
  if (ints_enabled) {
    __disable_irq();
  }

  RtcTicks rtc_interval_ticks = nrf_rtc_counter_get(NRF_RTC1);
  if (rtc_interval_ticks < s_last_ticks) {
    s_coarse_ticks += RTC_COUNTER_COUNTER_Msk;
  }
  s_last_ticks = rtc_interval_ticks;

  if (ints_enabled) {
    __enable_irq();
  }

  return s_coarse_ticks + rtc_interval_ticks;
}

void rtc_alarm_init(void) {
}

void rtc_alarm_set(RtcTicks num_ticks) {
}

RtcTicks rtc_alarm_get_elapsed_ticks(void) {
  return 0;
}

bool rtc_alarm_is_initialized(void) {
  return 0;
}

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

  retained_write(RTC_TIMEZONE_ABBR_START, raw[0]);
  retained_write(RTC_TIMEZONE_ABBR_END_TZID_DSTID, raw[1]);
  retained_write(RTC_TIMEZONE_GMTOFFSET, raw[2]);
  retained_write(RTC_TIMEZONE_DST_START, raw[3]);
  retained_write(RTC_TIMEZONE_DST_END, raw[4]);
}


void rtc_get_timezone(TimezoneInfo *tzinfo) {
  uint32_t *raw = (uint32_t*)tzinfo;

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
}

void rtc_init(void) {
#ifndef NRF_RTC_FREQ_TO_PRESCALER
#define NRF_RTC_FREQ_TO_PRESCALER RTC_FREQ_TO_PRESCALER
#endif
  nrf_rtc_prescaler_set(NRF_RTC1, NRF_RTC_FREQ_TO_PRESCALER(RTC_TICKS_HZ));
  nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_START);
}

void rtc_init_timers(void) {
}
