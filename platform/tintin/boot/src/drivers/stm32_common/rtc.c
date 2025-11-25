/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

#include "drivers/dbgserial.h"
#include "drivers/periph_config.h"
#include "drivers/rtc.h"
#include "util/delay.h"
#include "system/rtc_registers.h"

#include "stm32f2xx_rcc.h"
#include "stm32f2xx_rtc.h"

//! LSE startup time, about 4 seconds empirically,
//! but we give it 30 seconds since it it fails we sadwatch
static const int LSE_READY_TIMEOUT_MS = 30000;
static const unsigned int LSE_FREQUENCY_HZ = 32768;
static const int RTC_ASYNC_PRESCALER = 7;
static const int RTC_SYNC_PRESCALER = 3;

static const unsigned int RTC_TICKS_HZ = 1024;
static const unsigned int TICKS_IN_INTERVAL = 60 * 60 * 24;

static uint32_t prv_get_asynchronous_prescaler(void) {
  return (RTC->PRER >> 16) & 0x7f;
}

static uint32_t prv_get_synchronous_prescaler(void) {
  return RTC->PRER & 0x1fff;
}

//! Are we in slow mode?
static bool prv_slow_mode() {
  return prv_get_asynchronous_prescaler() == 0x7f && prv_get_synchronous_prescaler() == 0xff;
}

static bool prv_clocksource_is_lse_started(void) {
  return RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET;
}

static bool prv_clocksource_lse_configure(void) {
  if (prv_clocksource_is_lse_started()) {
    // LSE remains on through standby and resets so often don't need to do anything
    return true;
  }

  dbgserial_putstr("Starting LSE oscillator");
  RCC_LSEConfig(RCC_LSE_ON);
  for (int i = 0; i < LSE_READY_TIMEOUT_MS; i++) {
    if (prv_clocksource_is_lse_started()) {
      return true;
    }
    delay_us(1000);
  }

  dbgserial_putstr("LSE oscillator did not start");
  return false;
}

//! This routine relies on bootbits already having enabled
//! access to the PWR clock and backup domain. Re-enabling
//! it here breaks wakeup for some reason
//! Returns false if configuring LSE failed
bool rtc_init(void) {
  if (!prv_clocksource_lse_configure()) {
    return false;
  }

  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
  RCC_RTCCLKCmd(ENABLE);
  RTC_WaitForSynchro();
  return true;
}

// Before entering standby we set the RTC to it's default time (Jan 1, 2000)
// here we calculate the seconds elapsed since then
static int32_t prv_seconds_since_standby(void) {
  // This function assumes the RTC is running in slow mode

  RTC_TimeTypeDef rtc_time;
  RTC_GetTime(RTC_Format_BIN, &rtc_time);

  RTC_DateTypeDef rtc_date;
  RTC_GetDate(RTC_Format_BIN, &rtc_date);

  // Unlike mktime there's no error checking here since if something goes wrong
  // it'll just give us the wrong time anyway

  unsigned days = rtc_date.RTC_Year * 365; // RTC_Year is 0-99
  days += (rtc_date.RTC_Year / 4); // Leap years

  // Cumulative days from previous months
  const unsigned month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  days += month_days[rtc_date.RTC_Month - 1 ]; // RTC_Month is 1-12
  if ((rtc_date.RTC_Year + 1) % 4 == 0 && rtc_date.RTC_Month > 2) {
    // On a leap year and past February so add a leap day.
    days++;
  }

  // Add in previous days of the current month
  days += rtc_date.RTC_Date - 1;

  return rtc_time.RTC_Seconds + 60 * (rtc_time.RTC_Minutes + 60 *
         (rtc_time.RTC_Hours + 24 * days));
}

void rtc_initialize_fast_mode(void) {
  // We configure the RTC to run in "fast time". This means that the calendar will
  // be completely wrong, as we're incrementing the second count many times for every
  // real second. The firmware's driver will hide this fact from the rest of the
  // system. The reason we're doing this is because the STM32F2 micro doesn't offer
  // a subsecond field in their calendar, so we resort to crazy workarounds to get
  // a higher resolution timer.
  RTC_InitTypeDef rtc_init_struct;
  RTC_StructInit(&rtc_init_struct);

  _Static_assert((LSE_FREQUENCY_HZ / ((RTC_ASYNC_PRESCALER + 1) * (RTC_SYNC_PRESCALER + 1))) ==
                 RTC_TICKS_HZ, "Our prescalers won't create the clock we want");
  _Static_assert(RTC_ASYNC_PRESCALER >= 6, "PREDIV_A < 6 - Coarse calibration will not work.");

  rtc_init_struct.RTC_AsynchPrediv = RTC_ASYNC_PRESCALER;
  rtc_init_struct.RTC_SynchPrediv = RTC_SYNC_PRESCALER;

  RTC_Init(&rtc_init_struct);

  // Reset RTC time to 0, fast mode doesn't use the date register so leave it alone
  RTC_TimeTypeDef rtc_time;
  RTC_TimeStructInit(&rtc_time);
  RTC_SetTime(RTC_Format_BIN, &rtc_time);
}

void rtc_speed_up(void) {
  if (!prv_slow_mode()) {
    // If we're not in slow mode there's nothing to do
    return;
  }
  // On standby the RTC is reset to date 0, so the RTC's time is really
  // the number of seconds we've been in standby
  int32_t elapsed_since_standby = prv_seconds_since_standby();

  int32_t saved_time = RTC_ReadBackupRegister(CURRENT_TIME_REGISTER);
  // Correct the saved time with the number of seconds we've been in standby mode
  saved_time += elapsed_since_standby;

  // Save time in the backup register so the firmware can read it once it boots
  RTC_WriteBackupRegister(CURRENT_TIME_REGISTER, saved_time);
  RTC_WriteBackupRegister(CURRENT_INTERVAL_TICKS_REGISTER, 0);

  rtc_initialize_fast_mode();
}

static uint32_t prv_bcd_to_byte(uint32_t value) {
  const uint32_t tmp = ((value & 0xF0) >> 0x4) * 10;
  return (tmp + (value & 0x0F));
}

static uint32_t prv_cur_ticks(void) {
  uint32_t time_register = RTC->TR;

  const uint32_t hours = prv_bcd_to_byte((time_register & (RTC_TR_HT | RTC_TR_HU)) >> 16);
  const uint32_t minutes = prv_bcd_to_byte((time_register & (RTC_TR_MNT | RTC_TR_MNU)) >> 8);
  const uint32_t seconds = prv_bcd_to_byte(time_register & (RTC_TR_ST | RTC_TR_SU));

  return (((hours * 60) + minutes) * 60) + seconds;
}

static uint32_t prv_elapsed_ticks(uint32_t before, uint32_t after) {
  int32_t result = after - before;
  if (result < 0) {
    result = (TICKS_IN_INTERVAL - before) + after;
  }
  return result;
}

void rtc_slow_down(void) {
  if (prv_slow_mode()) {
    // If we're already slowed down there is nothing to do
    return;
  }

  // Calculate the current time and then save it back into the backup register
  int32_t last_save_time = RTC_ReadBackupRegister(CURRENT_TIME_REGISTER);
  uint32_t last_save_ticks = RTC_ReadBackupRegister(CURRENT_INTERVAL_TICKS_REGISTER);
  uint32_t ticks_since_save = prv_elapsed_ticks(last_save_ticks, prv_cur_ticks());

  int32_t cur_time = last_save_time + ticks_since_save / RTC_TICKS_HZ;
  // Save the current time into the backup registers
  RTC_WriteBackupRegister(CURRENT_TIME_REGISTER, cur_time);

  // Set the RTC back to defaults (normal prescalers)
  RTC_InitTypeDef rtc_init_struct;
  RTC_StructInit(&rtc_init_struct);
  RTC_Init(&rtc_init_struct);

  // Set the RTC to default date and time.
  // When we speed up the clock we'll add the elapsed seconds
  // to the saved register to get the correct time
  RTC_TimeTypeDef rtc_default_time;
  RTC_TimeStructInit(&rtc_default_time);
  RTC_SetTime(RTC_Format_BIN, &rtc_default_time);
  RTC_DateTypeDef rtc_default_date;
  RTC_DateStructInit(&rtc_default_date);
  RTC_SetDate(RTC_Format_BIN, &rtc_default_date);
}
