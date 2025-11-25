/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/time/time.h"

#include <clar.h>

// Fakes
///////////////////////////////////////////////////////////
#include "../../fakes/fake_rtc.h"

// Overrides
///////////////////////////////////////////////////////////
int16_t clock_get_timezone_region_id(void) {
  return rtc_get_timezone_id();
}

void clock_set_timezone_by_region_id(uint16_t region_id) {
  return;
}

// Tests
///////////////////////////////////////////////////////////

void test_time__initialize(void) {
}

void test_time__cleanup(void) {
}

void test_time__serial_distance32(void) {
  uint32_t day, hour, minute, second;

  {
    time_util_split_seconds_into_parts(1, &day, &hour, &minute, &second);
    cl_assert_equal_i(day, 0);
    cl_assert_equal_i(hour, 0);
    cl_assert_equal_i(minute, 0);
    cl_assert_equal_i(second, 1);
  }

  {
    time_util_split_seconds_into_parts(61, &day, &hour, &minute, &second);
    cl_assert_equal_i(day, 0);
    cl_assert_equal_i(hour, 0);
    cl_assert_equal_i(minute, 1);
    cl_assert_equal_i(second, 1);
  }

  {
    second = 1;

    time_util_split_seconds_into_parts((3 * (24 * 60 * 60)), &day, &hour, &minute, &second);
    cl_assert_equal_i(day, 3);
    cl_assert_equal_i(hour, 0);
    cl_assert_equal_i(minute, 0);
    cl_assert_equal_i(second, 0);
  }


  {
    time_util_split_seconds_into_parts((3 * (24 * 60 * 60)) + (2 * (60 * 60)) + (4 * 60) + 5, &day, &hour, &minute, &second);
    cl_assert_equal_i(day, 3);
    cl_assert_equal_i(hour, 2);
    cl_assert_equal_i(minute, 4);
    cl_assert_equal_i(second, 5);
  }

}
