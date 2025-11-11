/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

#include "board/board.h"
#include "drivers/rtc.h"
#include "drivers/rtc_private.h"
#include "mcu/interrupts.h"
#include "system/passert.h"
#include "util/time/time.h"
#include "system/logging.h"
#include "services/common/new_timer/new_timer.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bf0_hal_rtc.h"

// The RTC clock, CLK_RTC, can be configured to use the LXT32 (32.768 kHz) or
// LRC10 (9.8 kHz). The prescaler values need to be set such that the CLK1S
// event runs at 1 Hz. The formula that relates prescaler values with the
// clock frequency is as follows:
//
//   F(CLK1S) = CLK_RTC / (DIV_A_INT + DIV_A_FRAC / 2^14) / DIV_B

#ifndef SF32LB52_USE_LXT
#define DIV_A_INT 38U
#define DIV_A_FRAC 4608U
#define DIV_B 256U

// RC calibration clock cyles
#define LXT_LP_CYCLE 200

// The deviation limit of the current calibration value of RC10K relative to the average
// calibration value.
#define MAX_DELTA_BETWEEN_RTC_AVE (LXT_LP_CYCLE / 2)

// The number of RC10K calibrations required for one RTC calibration
#define RC10K_CALS_PER_RTC_CAL 20

TimerID s_rc10k_cal_timer;
#else
#define DIV_A_INT 128U
#define DIV_A_FRAC 0U
#define DIV_B 256U
#endif

static RTC_HandleTypeDef RTC_Handler = {
    .Instance = (RTC_TypeDef*)RTC_BASE,
    .Init =
        {
            .HourFormat = RTC_HOURFORMAT_24,
            .DivAInt = DIV_A_INT,
            .DivAFrac = DIV_A_FRAC,
            .DivB = DIV_B,
        },
};

#ifndef SF32LB52_USE_LXT
static uint32_t prv_rtc_get_lpcycle() {
  uint32_t value;

  value = HAL_Get_backup(RTC_BACKUP_LPCYCLE_AVE);
  if (value == 0) {
    value = 1200000;
  }

  value += 1;  // Calibrate in initial with 8 cycle
  HAL_Set_backup(RTC_BACKUP_LPCYCLE, (uint32_t)value);

  return value;
}

void prv_rtc_rc10_calculate_div(RTC_HandleTypeDef* hdl, uint32_t value) {
  hdl->Init.DivB = RC10K_SUB_SEC_DIVB;

  // 1 seconds has total 1/(x/(48*8))/256=1.5M/x cycles, times 2^14 for DIVA
  uint32_t divider = RTC_Handler.Init.DivB * value;
  value = ((uint64_t)48000000 * LXT_LP_CYCLE * (1 << 14) + (divider >> 1)) / divider;
  hdl->Init.DivAInt = (uint32_t)(value >> 14);
  hdl->Init.DivAFrac = (uint32_t)(value & ((1 << 14) - 1));
}

static void prv_rtc_reconfig() {
  uint32_t cur_ave;
  HAL_StatusTypeDef ret;
  cur_ave = prv_rtc_get_lpcycle();
  prv_rtc_rc10_calculate_div(&RTC_Handler, cur_ave);

  ret = HAL_RTC_Init(&RTC_Handler, RTC_INIT_REINIT);
  PBL_ASSERTN(ret == HAL_OK);
  HAL_Set_backup(RTC_BACKUP_LPCYCLE, cur_ave);
}

static void prv_rtc_calibrate() {
  static uint32_t s_rtc_cycle_count_init = 0;
  static double s_rtc_a = 0.0;
  static double s_delta_total = 0.0;

  if (s_rtc_cycle_count_init == 0) {
    uint16_t sub;
    time_t t;

    prv_rtc_reconfig();
    // Get initial lpcycle, RTC is running based on it.
    s_rtc_cycle_count_init = HAL_Get_backup(RTC_BACKUP_LPCYCLE);
    s_delta_total = 0.0;
    rtc_get_time_ms(&t, &sub);
    s_rtc_a = 1.0 * t + ((float)(1.0 * sub)) / RC10K_SUB_SEC_DIVB;
  } else {
    uint16_t sub2 = 0;
    double rtc_cal = 0.0;
    double delta = 0.0;
    time_t t2;
    uint32_t cur_ave;
    uint32_t ref_cycle;
    double rtc_b;

    rtc_get_time_ms(&t2, &sub2);
    cur_ave = HAL_Get_backup(RTC_BACKUP_LPCYCLE_AVE);
    ref_cycle = cur_ave;
    rtc_b = 1.0 * t2 + ((double)(1.0 * sub2)) / RC10K_SUB_SEC_DIVB;

    // Delta time between s_rtc_a to rtc_b, in seconds.
    delta = rtc_b - s_rtc_a;
    // Calculate accurate rtc_b
    rtc_cal = delta * ref_cycle / s_rtc_cycle_count_init + s_rtc_a;
    // Detla time of accurrate rtc_b and current rtc_b
    delta = rtc_cal - rtc_b;

    // Accumulate error
    s_delta_total += delta;

    if (s_delta_total > 1.0 || s_delta_total < -1.0) {
      // Accurate time
      rtc_cal = s_delta_total + rtc_b;
      // Apply integal part difference.
      rtc_set_time((uint32_t)rtc_cal);
      // Continue with subseconds
      s_delta_total = rtc_cal - (uint32_t)rtc_cal;
      // Next inteval start time
      s_rtc_a = (uint32_t)rtc_cal;
      if ((cur_ave > s_rtc_cycle_count_init &&
           (cur_ave - s_rtc_cycle_count_init) > MAX_DELTA_BETWEEN_RTC_AVE) ||
          (cur_ave < s_rtc_cycle_count_init &&
           (s_rtc_cycle_count_init - cur_ave) > MAX_DELTA_BETWEEN_RTC_AVE)) {
        prv_rtc_reconfig();
        s_rtc_cycle_count_init = HAL_Get_backup(RTC_BACKUP_LPCYCLE);
      }
    } else {
      // Next inteval start time
      s_rtc_a = rtc_b;
    }

    PBL_LOG(LOG_LEVEL_INFO,
            "origin: f=%dHz,cycle=%d avr: f=%dHz cycle_ave=%d delta=%d, delta_sum=%d\n",
            (int)((uint64_t)48000 * LXT_LP_CYCLE * 1000 / s_rtc_cycle_count_init),
            (int)s_rtc_cycle_count_init, (int)((uint64_t)48000 * LXT_LP_CYCLE * 1000 / ref_cycle),
            (int)ref_cycle, (int)(delta * 1000), (int)(s_delta_total * 1000));
  }
}

void prv_rc10k_cal_timer_cb(void* data) {
  static uint8_t s_rtc10k_cal_cnt;

  s_rtc10k_cal_cnt++;

  HAL_RC_CAL_update_reference_cycle_on_48M(LXT_LP_CYCLE);

  if (s_rtc10k_cal_cnt == RC10K_CALS_PER_RTC_CAL) {
    s_rtc10k_cal_cnt = 0U;
    prv_rtc_calibrate();
  }
}
#endif

void rtc_init(void) {
  HAL_StatusTypeDef ret;

#ifdef SF32LB52_USE_LXT
  ret = HAL_PMU_LXTReady();
  PBL_ASSERTN(ret == HAL_OK);

  ret = HAL_RTC_Init(&RTC_Handler, RTC_INIT_NORMAL);
  PBL_ASSERTN(ret == HAL_OK);
#else
  prv_rtc_reconfig();
#endif
}

void rtc_init_timers(void) {}

static RtcTicks get_ticks(void) {
  static TickType_t s_last_freertos_tick_count = 0;
  static RtcTicks s_coarse_ticks = 0;

  bool ints_enabled = mcu_state_are_interrupts_enabled();
  if (ints_enabled) {
    __disable_irq();
  }

  TickType_t freertos_tick_count = xTaskGetTickCount();
  if (freertos_tick_count < s_last_freertos_tick_count) {
    TickType_t rollover_amount = -1;
    s_coarse_ticks += rollover_amount;
  }

  s_last_freertos_tick_count = freertos_tick_count;
  RtcTicks ret_value = freertos_tick_count + s_coarse_ticks;

  if (ints_enabled) {
    __enable_irq();
  }

  return ret_value;
}

void rtc_set_time(time_t time) {
  struct tm t;
  gmtime_r(&time, &t);

  PBL_ASSERTN(!rtc_sanitize_struct_tm(&t));

  RTC_TimeTypeDef rtc_time_struct = {.Hours = t.tm_hour, .Minutes = t.tm_min, .Seconds = t.tm_sec};

  RTC_DateTypeDef rtc_date_struct = {
      .Month = t.tm_mon + 1,
      .Date = t.tm_mday,
      .Year = t.tm_year % 100,
  };

  HAL_RTC_SetTime(&RTC_Handler, &rtc_time_struct, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&RTC_Handler, &rtc_date_struct, RTC_FORMAT_BIN);

  PBL_LOG(LOG_LEVEL_INFO, "RTC set time to %lu", time);
  PBL_LOG(LOG_LEVEL_INFO, "%u:%u:%u, %u/%u/%u (%u)",
          rtc_time_struct.Hours, rtc_time_struct.Minutes, rtc_time_struct.Seconds,
          rtc_date_struct.Month, rtc_date_struct.Date, rtc_date_struct.Year,
          rtc_date_struct.WeekDay);
}

void rtc_get_time_ms(time_t* out_seconds, uint16_t* out_ms) {
  RTC_DateTypeDef rtc_date;
  RTC_TimeTypeDef rtc_time;

  HAL_RTC_GetTime(&RTC_Handler, &rtc_time, RTC_FORMAT_BIN);
  while (HAL_RTC_GetDate(&RTC_Handler, &rtc_date, RTC_FORMAT_BIN) == HAL_ERROR) {
    // HAL_ERROR is returned if a rollover occurs, so just keep trying
    HAL_RTC_GetTime(&RTC_Handler, &rtc_time, RTC_FORMAT_BIN);
  };

  struct tm current_time = {
      .tm_sec = rtc_time.Seconds,
      .tm_min = rtc_time.Minutes,
      .tm_hour = rtc_time.Hours,
      .tm_mday = rtc_date.Date,
      .tm_mon = rtc_date.Month - 1,
      .tm_year = rtc_date.Year + 100,
      .tm_wday = rtc_date.WeekDay,
      .tm_yday = 0,
      .tm_isdst = 0,
  };

  *out_seconds = mktime(&current_time);
  *out_ms = (uint16_t)((rtc_time.SubSeconds * 1000) / DIV_B);
}

time_t rtc_get_time(void) {
  time_t seconds;
  uint16_t ms;

  rtc_get_time_ms(&seconds, &ms);

  return seconds;
}

RtcTicks rtc_get_ticks(void) {
  return get_ticks();
}

void rtc_alarm_init(void) {}

void rtc_alarm_set(RtcTicks num_ticks) {}

RtcTicks rtc_alarm_get_elapsed_ticks(void) {
  return 0;
}

bool rtc_alarm_is_initialized(void) {
  return true;
}

bool rtc_sanitize_struct_tm(struct tm* t) {
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

bool rtc_sanitize_time_t(time_t* t) {
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
void rtc_set_timezone(TimezoneInfo* tzinfo) {}

void rtc_get_timezone(TimezoneInfo* tzinfo) {}

void rtc_timezone_clear(void) {}

uint16_t rtc_get_timezone_id(void) {
  return 0;
}

bool rtc_is_timezone_set(void) {
  return 0;
}

void rtc_enable_backup_regs(void) {}

void rtc_calibrate_frequency(uint32_t frequency) {
  prv_rtc_calibrate();

  s_rc10k_cal_timer = new_timer_create();
  PBL_ASSERTN(s_rc10k_cal_timer != TIMER_INVALID_ID);

  bool success = new_timer_start(s_rc10k_cal_timer, 15000, prv_rc10k_cal_timer_cb, NULL,
                                 TIMER_START_FLAG_REPEATING);
  PBL_ASSERTN(success);
}
