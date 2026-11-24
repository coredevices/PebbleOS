/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/cron/cron.h>
#include <pbl/kernel/time.h>
#include <pbl/services/timers/timer.h>
#include <pbl/util/units.h>
#include <pbl/util/uuid.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_work.h>
#include <kernel/events.h>

static const Uuid s_owner_a = UuidMake(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
static const Uuid s_owner_b = UuidMake(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);

static int s_num_timer_events;
static PebbleTimerEvent s_last_timer_event;

// The test harness's uuid helper always returns false; compare for real here.
bool uuid_equal(const Uuid *uu1, const Uuid *uu2) {
  return uu1 && uu2 && memcmp(uu1, uu2, sizeof(Uuid)) == 0;
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_TIMER_EVENT) {
    s_num_timer_events++;
    s_last_timer_event = event->timer;
  }
}

void test_timer__initialize(void) {
  fake_work_cleanup();
  fake_rtc_init(0, 1447286400);
  pbl_cron_init();
  timer_init();
  s_num_timer_events = 0;
  s_last_timer_event = (PebbleTimerEvent){};
}

void test_timer__cleanup(void) {
  pbl_cron_deinit();
  fake_work_cleanup();
  timer_init();
}

static void prv_advance_time(uint32_t seconds) {
  fake_rtc_increment_time(seconds);
  pbl_cron_wakeup();
}

void test_timer__create_and_fire(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(timer_can_create());
  cl_assert_equal_i(s_num_timer_events, 0);

  prv_advance_time(59);
  cl_assert_equal_i(s_num_timer_events, 0);

  prv_advance_time(1);
  cl_assert_equal_i(s_num_timer_events, 1);
  cl_assert_equal_i(s_last_timer_event.duration_s, 60);
}

void test_timer__rejects_invalid_arguments(void) {
  cl_assert_equal_i(timer_start(0, &s_owner_a), TIMER_INVALID_ID);
  cl_assert_equal_i(timer_start(TIMER_MAX_DURATION_S + 1, &s_owner_a), TIMER_INVALID_ID);
  cl_assert_equal_i(timer_start(60, NULL), TIMER_INVALID_ID);
  cl_assert_equal_i(s_num_timer_events, 0);
}

void test_timer__cancel(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(timer_cancel(id, &s_owner_a));
  cl_assert(!timer_cancel(id, &s_owner_a));

  prv_advance_time(60);
  cl_assert_equal_i(s_num_timer_events, 0);
}

void test_timer__cancel_requires_owner(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(!timer_cancel(id, &s_owner_b));
  cl_assert(timer_cancel(id, &s_owner_a));
}

void test_timer__cancel_all_only_affects_the_owner(void) {
  const TimerId a1 = timer_start(60, &s_owner_a);
  const TimerId a2 = timer_start(120, &s_owner_a);
  const TimerId b1 = timer_start(60, &s_owner_b);
  cl_assert(a1 != TIMER_INVALID_ID && a2 != TIMER_INVALID_ID && b1 != TIMER_INVALID_ID);

  cl_assert_equal_i(timer_cancel_all(&s_owner_a), 2);
  cl_assert(!timer_cancel(a1, &s_owner_a));
  cl_assert(!timer_cancel(a2, &s_owner_a));

  prv_advance_time(60);
  cl_assert_equal_i(s_num_timer_events, 1);
  cl_assert_equal_i(s_last_timer_event.duration_s, 60);
}

void test_timer__capacity(void) {
  TimerId ids[TIMER_MAX_COUNT];
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    ids[i] = timer_start(3600, &s_owner_a);
    cl_assert(ids[i] != TIMER_INVALID_ID);
  }

  cl_assert(!timer_can_create());
  cl_assert_equal_i(timer_start(3600, &s_owner_a), TIMER_INVALID_ID);

  cl_assert(timer_cancel(ids[0], &s_owner_a));
  cl_assert(timer_can_create());
  cl_assert(timer_start(3600, &s_owner_a) != TIMER_INVALID_ID);
}

void test_timer__ids_are_not_reused(void) {
  const TimerId first = timer_start(60, &s_owner_a);
  cl_assert(first != TIMER_INVALID_ID);
  cl_assert(timer_cancel(first, &s_owner_a));

  const TimerId second = timer_start(60, &s_owner_a);
  cl_assert(second != TIMER_INVALID_ID);
  cl_assert(second != first);
}
