/*
 * Copyright 2024 Google LLC
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

#include "date_time_selection_window_private.h"

#include "services/common/clock.h"
#include "services/common/i18n/i18n.h"
#include "util/date.h"
#include "util/math.h"

#include <stdio.h>

#pragma GCC diagnostic ignored "-Wformat-truncation"

static const int MIN_SELECTABLE_YEAR = 2010;
static const int MAX_SELECTABLE_YEAR = 2037;  // Work around Y2038 problem

static int prv_wrap(int x, int max, int delta) {
  x = (x + delta) % max;
  return x < 0 ? x + max : x;
}

int date_time_selection_step_hour(int hour, int delta) {
  return prv_wrap(hour, 24, delta);
}

int date_time_selection_step_minute(int minute, int delta) {
  return prv_wrap(minute, 60, delta);
}

int date_time_selection_step_day(int year, int month, int day, int delta) {
  bool is_leap_year = date_util_is_leap_year(year);
  // This function expects Jan == 0, but date_util_get_max_days_in_month expects Jan == 1
  int max_days = date_util_get_max_days_in_month(month + 1, is_leap_year);
  // This functions expects the first day of the month is 1, but wrap expects the first day of the
  // month is 0 (based off the mday element of the "tm" struct)
  return prv_wrap(day - 1, max_days, delta) + 1;
}

int date_time_selection_step_month(int month, int delta) {
  return prv_wrap(month, 12, delta);
}

int date_time_selection_truncate_date(int year, int month, int day) {
  bool is_leap_year = date_util_is_leap_year(year);

  // date_util_get_max_days_in_month expects Jan == 1, but this function expects Jan == 0
  int max_days = date_util_get_max_days_in_month(month + 1, is_leap_year);
  return MIN(day, max_days);
}

int date_time_selection_step_year(int year, int delta) {
  year += delta;
  return CLIP(year, MIN_SELECTABLE_YEAR - STDTIME_YEAR_OFFSET,
      MAX_SELECTABLE_YEAR - STDTIME_YEAR_OFFSET);
}

char *date_time_selection_get_text(TimeData *data, TimeInputIndex index, char *buf) {
  switch (index) {
    case TimeInputIndexHour: {
      unsigned hour = data->hour;
      if (!clock_is_24h_style()) {
        hour = hour % 12;
        if (hour == 0) {
          hour = 12;
        }
      }
      snprintf(buf, 3, "%02u", hour);
      return buf;
    }
    case TimeInputIndexMinute:
      snprintf(buf, 3, "%02u", data->minute);
      return buf;
    case TimeInputIndexAMPM: // We should only get this in 12h style
      if (data->hour < 12) {
        i18n_get_with_buffer("AM", buf, 3);
      } else {
        i18n_get_with_buffer("PM", buf, 3);
      }
      return buf;
    default:
      return "";
  }
}

void date_time_handle_time_change(TimeData *data, TimeInputIndex index, int delta) {
  switch (index) {
    case TimeInputIndexHour:
      data->hour = date_time_selection_step_hour(data->hour, delta);
      break;
    case TimeInputIndexMinute:
      data->minute = date_time_selection_step_minute(data->minute, delta);
      break;
    case TimeInputIndexAMPM: // We should only get this in 12h style
      data->hour = date_time_selection_step_hour(data->hour, 12 * delta);
      break;
  }
}
