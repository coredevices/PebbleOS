/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/services/clock.h>
#include <pbl/services/time.h>

#include <services/clock/tz_storage.h>

void PBL_WEAK clock_get_since_time(char *buffer, int buf_size, time_t timestamp) {
}

void PBL_WEAK clock_get_until_time(char *buffer, int buf_size, time_t timestamp,
                                   int max_relative_hrs) {
}

void PBL_WEAK clock_get_until_time_without_fulltime(char *buffer, int buf_size, time_t timestamp,
                                                    int max_relative_hrs) {
}

void PBL_WEAK clock_hour_and_minute_add(int *hour, int *minute, int delta_minutes) {
}

size_t PBL_WEAK clock_copy_time_string_timestamp(char *buffer, uint8_t size, time_t timestamp) {
  return 0;
}

void PBL_WEAK clock_get_friendly_date(char *buffer, int buf_size, time_t timestamp) {
}

size_t PBL_WEAK clock_get_time_number(char *number_buffer, size_t number_buffer_size,
                                      time_t timestamp) {
  return 0;
}

size_t PBL_WEAK clock_get_time_word(char *buffer, size_t buffer_size, time_t timestamp) {
  return 0;
}

void PBL_WEAK clock_get_time_tm(struct pbl_tm *time_tm) {
  time_t t = pbl_realtime();

  pbl_gmtime_r(&t, time_tm);
}

bool PBL_WEAK clock_is_timezone_set(void) {
  return tz_storage_is_set();
}

void PBL_WEAK clock_timezone_clear(void) {
  tz_storage_clear();
}
