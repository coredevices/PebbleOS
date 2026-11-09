/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/services/tick_timer.h>

#include <applib/tick_timer_service.h>
#include <clar.h>
#include <fake_pebble_tasks.h>
#include <fake_work.h>
#include <kernel/events.h>
#include <syscall/syscall.h>

// The App task's queue to KernelMain.
static PebbleEvent s_queue[8];
static int s_queued;

static int s_ticks;
static PebbleTaskBitset s_tick_mask;
static time_t s_now;
static time_t s_tick_time;

bool event_try_put_from_process(PebbleTask task, PebbleEvent *event) {
  cl_assert(s_queued < (int)(sizeof(s_queue) / sizeof(s_queue[0])));
  s_queue[s_queued++] = *event;
  return true;
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_TICK_EVENT) {
    s_ticks++;
    s_tick_mask = event->task_mask;
    s_tick_time = event->clock_tick.tick_time;
  }
}

time_t pbl_realtime(void) {
  return s_now;
}

void pbl_syscall_fail(void) {
  abort();
}

//! Runs the next queued App request on "KernelMain".
static void prv_run_next_request(void) {
  cl_assert(s_queued > 0);
  PebbleEvent e = s_queue[0];
  s_queued--;
  for (int i = 0; i < s_queued; i++) {
    s_queue[i] = s_queue[i + 1];
  }
  cl_assert_equal_i(e.type, PEBBLE_CALLBACK_EVENT);
  e.callback.callback(e.callback.data);
}

//! Lets one boundary of @p unit_s seconds pass, and whether it published one tick.
static bool prv_ticks_every(uint32_t unit_s) {
  const int ticks = s_ticks;
  s_now += unit_s;
  fake_wallclock_work_advance(unit_s, 1);
  if (s_ticks == ticks) {
    return false;
  }
  cl_assert_equal_i(s_ticks, ticks + 1);
  cl_assert_equal_i(s_tick_mask, 0);
  cl_assert_equal_i(s_tick_time, s_now);
  return true;
}

static bool prv_ticking_seconds(void) {
  return prv_ticks_every(1);
}

static bool prv_ticking_minutes(void) {
  return prv_ticks_every(60);
}

void test_tick_timer__initialize(void) {
  s_queued = 0;
  s_ticks = 0;
  s_tick_mask = 0;
  s_now = 1000000;
  s_tick_time = 0;
  stub_pebble_tasks_set_current(PebbleTask_App);
}

void test_tick_timer__cleanup(void) {
  while (s_queued > 0) {
    prv_run_next_request();
  }
  fake_wallclock_work_cleanup();
}

void test_tick_timer__first_tick_follows_the_subscription(void) {
  // The app queues its subscription, then the units request.
  sys_tick_timer_subscribe(MINUTE_UNIT);
  cl_assert_equal_i(s_ticks, 0);

  tick_timer_add_subscriber(PebbleTask_App);
  prv_run_next_request();
  cl_assert_equal_i(s_ticks, 1);
  cl_assert_equal_i(s_tick_mask, (PebbleTaskBitset) ~(1u << PebbleTask_App));
  cl_assert(prv_ticking_minutes());
  cl_assert(!prv_ticking_seconds());

  sys_tick_timer_unsubscribe();
  tick_timer_remove_subscriber(PebbleTask_App);
  prv_run_next_request();
  cl_assert(!prv_ticking_minutes());
}

void test_tick_timer__resubscribe_keeps_seconds(void) {
  sys_tick_timer_subscribe(SECOND_UNIT);
  tick_timer_add_subscriber(PebbleTask_App);
  prv_run_next_request();
  cl_assert(prv_ticking_seconds());

  // Unsubscribe and subscribe again before KernelMain drains anything.
  sys_tick_timer_unsubscribe();
  sys_tick_timer_subscribe(SECOND_UNIT);

  prv_run_next_request();
  tick_timer_remove_subscriber(PebbleTask_App);
  tick_timer_add_subscriber(PebbleTask_App);
  prv_run_next_request();
  cl_assert(prv_ticking_seconds());

  sys_tick_timer_unsubscribe();
  tick_timer_remove_subscriber(PebbleTask_App);
  prv_run_next_request();
  cl_assert(!prv_ticking_seconds());
}

void test_tick_timer__kernel_main_applies_at_once(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  tick_timer_add_subscriber(PebbleTask_KernelMain);
  sys_tick_timer_subscribe(SECOND_UNIT);
  cl_assert_equal_i(s_queued, 0);
  cl_assert_equal_i(s_ticks, 1);
  cl_assert(prv_ticking_seconds());

  sys_tick_timer_unsubscribe();
  tick_timer_remove_subscriber(PebbleTask_KernelMain);
  cl_assert(!prv_ticking_seconds());
  cl_assert(!prv_ticking_minutes());
}

void test_tick_timer__ticks_on_each_boundary(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  tick_timer_add_subscriber(PebbleTask_KernelMain);
  sys_tick_timer_subscribe(SECOND_UNIT | MINUTE_UNIT);
  for (int i = 0; i < 3; i++) {
    cl_assert(prv_ticking_seconds());
  }
  cl_assert(!prv_ticking_minutes());

  sys_tick_timer_subscribe(MINUTE_UNIT);
  cl_assert(!prv_ticking_seconds());
  for (int i = 0; i < 3; i++) {
    cl_assert(prv_ticking_minutes());
  }

  sys_tick_timer_unsubscribe();
  tick_timer_remove_subscriber(PebbleTask_KernelMain);
  cl_assert(!prv_ticking_seconds());
  cl_assert(!prv_ticking_minutes());
}
