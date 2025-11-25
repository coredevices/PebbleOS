/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/i18n/i18n.h"
#include "util/attributes.h"
#include "util/math.h"

#include "stubs_i18n.h"

#include <inttypes.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

WEAK const char *string_strip_leading_whitespace(const char *string) {
  const char *result_string = string;
  while (*result_string != '\0') {
    if (*result_string != ' ' &&
        *result_string != '\n') {
      break;
    }
    result_string++;
  }

  return result_string;
}

WEAK int time_util_get_num_hours(int hours, bool is24h) {
  return is24h ? hours : (hours + 12 - 1) % 12 + 1;
}

WEAK bool clock_is_24h_style() {
  return false;
}

static size_t prv_format_time(char *buffer, int buf_size, const char *format, time_t timestamp) {
  struct tm time_tm;
  localtime_r(&timestamp, &time_tm);
  const size_t ret_val = strftime(buffer, buf_size, i18n_get(format, buffer), &time_tm);
  i18n_free(format, buffer);
  return ret_val;
}

size_t clock_get_time_number(char *number_buffer, size_t number_buffer_size, time_t timestamp) {
  const size_t written =
      prv_format_time(number_buffer, number_buffer_size,
                      (clock_is_24h_style() ? i18n_noop("%R") : i18n_noop("%l:%M")), timestamp);
  const char *number_buffer_ptr = string_strip_leading_whitespace(number_buffer);
  memmove(number_buffer,
          number_buffer_ptr,
          number_buffer_size - (number_buffer_ptr - number_buffer));
  return written - (number_buffer_ptr - number_buffer);
}

size_t clock_get_time_word(char *buffer, size_t buffer_size, time_t timestamp) {
  if (clock_is_24h_style()) {
    buffer[0] = '\0';
    return 0;
  } else {
    return prv_format_time(buffer, buffer_size, i18n_noop("%p"), timestamp);
  }
}

size_t clock_format_time(char *buffer, uint8_t size, int16_t hours, int16_t minutes,
                         bool add_space) {
  if (size == 0 || buffer == NULL) {
    return 0;
  }

  bool is24h = clock_is_24h_style();
  const char *format;

  // [INTL] you want to have layout resources that specify time formatting,
  // and be able to set a default one for each locale.
  if (is24h) {
    format = "%u:%02u";
  } else {
    if (hours < 12) {
      format = add_space ? "%u:%02u AM" : "%u:%02uAM";
    } else {
      format = add_space ? "%u:%02u PM" : "%u:%02uPM";
    }
  }
  return sniprintf(buffer, size, format, time_util_get_num_hours(hours, is24h), minutes);
}

size_t clock_copy_time_string_timestamp(char *buffer, uint8_t size, time_t timestamp) {
  struct tm time;
  localtime_r(&timestamp, &time);
  return clock_format_time(buffer, size, time.tm_hour, time.tm_min, true);
}

void clock_copy_time_string(char *buffer, uint8_t size) {
  time_t t = 0;
  clock_copy_time_string_timestamp(buffer, size, t);
}

size_t clock_get_date(char *buffer, int buf_size, time_t timestamp) {
  return prv_format_time(buffer, buf_size, i18n_noop("%m/%d"), timestamp);
}

size_t clock_get_day_date(char *buffer, int buf_size, time_t timestamp) {
  return prv_format_time(buffer, buf_size, i18n_noop("%d"), timestamp);
}

void clock_hour_and_minute_add(int *hour, int *minute, int delta_minutes) {
  const int new_minutes = positive_modulo(*hour * MINUTES_PER_HOUR + *minute + delta_minutes,
                                          MINUTES_PER_DAY);
  *hour = new_minutes / MINUTES_PER_HOUR;
  *minute = new_minutes % MINUTES_PER_HOUR;
}
