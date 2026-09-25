/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/regular_timer.h"

#include "fake_new_timer.h"
#include "fake_rtc.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

#define START_TIME 1790000000
#define MARGIN_MS  10

static int s_seconds_fired;
static time_t s_last_fired_time;

static void prv_seconds_cb(void *data) {
  s_seconds_fired++;
  s_last_fired_time = rtc_get_time();
}

static RegularTimerInfo s_seconds_info = {.cb = prv_seconds_cb};

static TimerID prv_timer(void) {
  return stub_new_timer_get_next();
}

static uint32_t prv_timeout(void) {
  return stub_new_timer_timeout(prv_timer());
}

static void prv_advance_and_fire(void) {
  fake_rtc_increment_time_ms(prv_timeout());
  stub_new_timer_fire(prv_timer());
}

void test_regular_timer__initialize(void) {
  s_seconds_fired = 0;
  s_last_fired_time = 0;
  fake_rtc_init(0, START_TIME);
  regular_timer_init();
  regular_timer_add_seconds_callback(&s_seconds_info);
}

void test_regular_timer__cleanup(void) {
  regular_timer_remove_callback(&s_seconds_info);
  regular_timer_deinit();
  stub_new_timer_cleanup();
}

void test_regular_timer__first_tick_aligned_to_second(void) {
  regular_timer_deinit();
  stub_new_timer_cleanup();
  fake_rtc_increment_time_ms(300);
  regular_timer_init();

  cl_assert_equal_i(prv_timeout(), 700 + MARGIN_MS);
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 1);
  cl_assert_equal_i(s_last_fired_time, START_TIME + 1);
  cl_assert_equal_i(prv_timeout(), 1000);
}

void test_regular_timer__realigns_after_time_set(void) {
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 1);

  // Time set to the next second 600 ms into the current one; the RTC restarts the second there
  fake_rtc_increment_time_ms(600);
  fake_rtc_init(0, START_TIME + 2);
  fake_rtc_increment_time_ms(prv_timeout() - 600);
  stub_new_timer_fire(prv_timer());
  cl_assert_equal_i(s_seconds_fired, 2);
  cl_assert_equal_i(s_last_fired_time, START_TIME + 2);

  cl_assert_equal_i(prv_timeout(), 600 + MARGIN_MS);
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 3);
  cl_assert_equal_i(s_last_fired_time, START_TIME + 3);
  cl_assert_equal_i(prv_timeout(), 1000);
}

void test_regular_timer__no_duplicate_second(void) {
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 1);

  // Time set back into the second that just fired
  fake_rtc_increment_time_ms(300);
  fake_rtc_init(0, START_TIME + 1);
  fake_rtc_increment_time_ms(prv_timeout() - 300);
  stub_new_timer_fire(prv_timer());
  cl_assert_equal_i(s_seconds_fired, 1);

  cl_assert_equal_i(prv_timeout(), 300 + MARGIN_MS);
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 2);
  cl_assert_equal_i(s_last_fired_time, START_TIME + 2);
}

void test_regular_timer__backwards_jump(void) {
  prv_advance_and_fire();
  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 2);

  fake_rtc_init(0, START_TIME - 60);
  fake_rtc_increment_time_ms(prv_timeout());
  stub_new_timer_fire(prv_timer());
  cl_assert_equal_i(s_seconds_fired, 3);
  cl_assert_equal_i(s_last_fired_time, START_TIME - 59);

  prv_advance_and_fire();
  cl_assert_equal_i(s_seconds_fired, 4);
  cl_assert_equal_i(s_last_fired_time, START_TIME - 58);
}
