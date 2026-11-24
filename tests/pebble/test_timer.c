/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/time.h>
#include <pbl/services/timers/timer.h>
#include <pbl/util/units.h>
#include <pbl/util/uuid.h>

#include <clar.h>
#include <fake_work.h>
#include <kernel/events.h>

static const Uuid s_owner_a = UuidMake(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
static const Uuid s_owner_b = UuidMake(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);

static uint64_t s_uptime_ms;
static time_t s_wall_time;
static int s_num_timer_events;
static PebbleTimerEvent s_last_timer_event;

// The test harness's uuid helper always returns false; compare for real here.
bool uuid_equal(const Uuid *uu1, const Uuid *uu2) {
  return uu1 && uu2 && memcmp(uu1, uu2, sizeof(Uuid)) == 0;
}

uint64_t pbl_uptime_ms(void) {
  return s_uptime_ms;
}

time_t pbl_realtime(void) {
  return s_wall_time;
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_TIMER_EVENT) {
    s_num_timer_events++;
    s_last_timer_event = event->timer;
  }
}

void test_timer__initialize(void) {
  fake_work_cleanup();
  s_uptime_ms = 0;
  s_wall_time = 1447286400;
  timer_init();
  s_num_timer_events = 0;
  s_last_timer_event = (PebbleTimerEvent){};
}

void test_timer__cleanup(void) {
  fake_work_cleanup();
  timer_init();
}

static void prv_advance_time(uint32_t seconds) {
  s_uptime_ms += (uint64_t)seconds * 1000U;
}

static void prv_fire_next(void) {
  struct pbl_delayed_work *next = fake_work_get_next();
  cl_assert(next != nullptr);
  fake_work_fire(next);
}

void test_timer__create_and_fire(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(timer_can_create());
  cl_assert_equal_i(s_num_timer_events, 0);

  prv_advance_time(59);
  cl_assert_equal_i(s_num_timer_events, 0);

  prv_advance_time(1);
  prv_fire_next();
  cl_assert_equal_i(s_num_timer_events, 1);
  cl_assert_equal_i(s_last_timer_event.duration_s, 60);
}

void test_timer__schedules_a_full_duration_from_now(void) {
  s_uptime_ms = 999;
  const TimerId id = timer_start(1, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);

  struct pbl_delayed_work *work = fake_work_get_next();
  cl_assert(work != nullptr);
  cl_assert_equal_i(fake_work_timeout_ms(work), 1000);
}

void test_timer__rejects_invalid_arguments(void) {
  cl_assert_equal_i(timer_start(0, &s_owner_a), TIMER_INVALID_ID);
  cl_assert_equal_i(timer_start(TIMER_MAX_DURATION_S + 1, &s_owner_a), TIMER_INVALID_ID);
  cl_assert_equal_i(timer_start(60, nullptr), TIMER_INVALID_ID);
  cl_assert_equal_i(s_num_timer_events, 0);
}

void test_timer__cancel(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(timer_cancel(id));
  cl_assert(!timer_cancel(id));
  cl_assert(fake_work_get_next() == nullptr);

  prv_advance_time(60);
  cl_assert_equal_i(s_num_timer_events, 0);
}

void test_timer__cancel_all_only_affects_the_owner(void) {
  const TimerId a1 = timer_start(60, &s_owner_a);
  const TimerId a2 = timer_start(120, &s_owner_a);
  const TimerId b1 = timer_start(60, &s_owner_b);
  cl_assert(a1 != TIMER_INVALID_ID && a2 != TIMER_INVALID_ID && b1 != TIMER_INVALID_ID);

  cl_assert_equal_i(timer_cancel_all(&s_owner_a), 2);
  cl_assert(!timer_cancel(a1));
  cl_assert(!timer_cancel(a2));

  prv_advance_time(60);
  prv_fire_next();
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

  cl_assert(timer_cancel(ids[0]));
  cl_assert(timer_can_create());
  cl_assert(timer_start(3600, &s_owner_a) != TIMER_INVALID_ID);
}

void test_timer__ids_are_not_reused(void) {
  const TimerId first = timer_start(60, &s_owner_a);
  cl_assert(first != TIMER_INVALID_ID);
  cl_assert(timer_cancel(first));

  const TimerId second = timer_start(60, &s_owner_a);
  cl_assert(second != TIMER_INVALID_ID);
  cl_assert(second != first);
}

void test_timer__formats_duration(void) {
  char buffer[TIMER_DESCRIPTION_SIZE];
  timer_format_duration(buffer, sizeof(buffer), 5);
  cl_assert_equal_s(buffer, "0:05");
  timer_format_duration(buffer, sizeof(buffer), 300);
  cl_assert_equal_s(buffer, "5:00");
  timer_format_duration(buffer, sizeof(buffer), 3600);
  cl_assert_equal_s(buffer, "1:00:00");
}

void test_timer__lists_all_timers_soonest_first(void) {
  const TimerId long_timer = timer_start(3600, &s_owner_a);
  const TimerId short_timer = timer_start(30, &s_owner_a);
  const TimerId medium_timer = timer_start(120, &s_owner_a);
  const TimerId other_timer = timer_start(10, &s_owner_b);
  cl_assert(long_timer != TIMER_INVALID_ID && short_timer != TIMER_INVALID_ID &&
            medium_timer != TIMER_INVALID_ID && other_timer != TIMER_INVALID_ID);

  TimerInfo timers[TIMER_MAX_COUNT];
  cl_assert_equal_i(timer_get_timers(timers, TIMER_MAX_COUNT), 4);

  cl_assert_equal_i(timers[0].id, other_timer);
  cl_assert_equal_i(timers[0].duration_s, 10);
  cl_assert_equal_i(timers[0].remaining_s, 10);
  cl_assert_equal_s(timers[0].description, "0:10");

  cl_assert_equal_i(timers[1].id, short_timer);
  cl_assert_equal_i(timers[1].duration_s, 30);
  cl_assert_equal_i(timers[1].remaining_s, 30);
  cl_assert_equal_s(timers[1].description, "0:30");

  cl_assert_equal_i(timers[2].id, medium_timer);
  cl_assert_equal_i(timers[2].duration_s, 120);
  cl_assert_equal_i(timers[2].remaining_s, 120);
  cl_assert_equal_s(timers[2].description, "2:00");

  cl_assert_equal_i(timers[3].id, long_timer);
  cl_assert_equal_i(timers[3].duration_s, 3600);
  cl_assert_equal_i(timers[3].remaining_s, 3600);
  cl_assert_equal_s(timers[3].description, "1:00:00");

  cl_assert_equal_i(timer_get_timers(timers, 2), 2);
  cl_assert_equal_i(timers[0].id, other_timer);
  cl_assert_equal_i(timers[1].id, short_timer);

  cl_assert_equal_i(timer_get_timers(timers, 0), 0);
  cl_assert_equal_i(timer_get_timers(nullptr, 1), 0);
}

void test_timer__remaining_time_counts_down(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);

  prv_advance_time(10);
  TimerInfo timers[1];
  cl_assert_equal_i(timer_get_timers(timers, 1), 1);
  cl_assert_equal_i(timers[0].id, id);
  cl_assert_equal_i(timers[0].remaining_s, 50);
}

void test_timer__wall_clock_changes_do_not_affect_remaining(void) {
  const TimerId id = timer_start(60, &s_owner_a);
  cl_assert(id != TIMER_INVALID_ID);

  prv_advance_time(10);
  TimerInfo timers[1];
  cl_assert_equal_i(timer_get_timers(timers, 1), 1);
  cl_assert_equal_i(timers[0].remaining_s, 50);

  s_wall_time -= 3600;
  cl_assert_equal_i(timer_get_timers(timers, 1), 1);
  cl_assert_equal_i(timers[0].remaining_s, 50);

  s_wall_time += 2 * 3600;
  cl_assert_equal_i(timer_get_timers(timers, 1), 1);
  cl_assert_equal_i(timers[0].remaining_s, 50);
}

void test_timer__list_is_empty_after_fire_or_cancel(void) {
  const TimerId fired = timer_start(10, &s_owner_a);
  const TimerId cancelled = timer_start(20, &s_owner_a);
  cl_assert(fired != TIMER_INVALID_ID && cancelled != TIMER_INVALID_ID);
  cl_assert(timer_cancel(cancelled));

  TimerInfo timers[TIMER_MAX_COUNT];
  cl_assert_equal_i(timer_get_timers(timers, TIMER_MAX_COUNT), 1);
  cl_assert_equal_i(timers[0].id, fired);

  prv_advance_time(10);
  prv_fire_next();
  cl_assert_equal_i(timer_get_timers(timers, TIMER_MAX_COUNT), 0);
}
