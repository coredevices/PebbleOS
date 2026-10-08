/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/services/tick_timer.h>

#include <applib/tick_timer_service.h>
#include <clar.h>
#include <fake_pebble_tasks.h>
#include <fake_regular_timer.h>
#include <kernel/events.h>
#include <stubs_logging.h>
#include <stubs_mutex.h>
#include <stubs_passert.h>
#include <syscall/syscall.h>

// The App task's queue to KernelMain.
static PebbleEvent s_queue[8];
static int s_queued;

static int s_ticks;
static PebbleTaskBitset s_tick_mask;

bool event_try_put_from_process(PebbleTask task, PebbleEvent *event) {
  cl_assert(s_queued < (int)(sizeof(s_queue) / sizeof(s_queue[0])));
  s_queue[s_queued++] = *event;
  return true;
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_TICK_EVENT) {
    s_ticks++;
    s_tick_mask = event->task_mask;
  }
}

time_t rtc_get_time(void) {
  return 0;
}

void syscall_failed(void) {
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

static bool prv_ticking_seconds(void) {
  return list_get_next(&s_seconds_callbacks) != NULL;
}

static bool prv_ticking_minutes(void) {
  return list_get_next(&s_minutes_callbacks) != NULL;
}

void test_tick_timer__initialize(void) {
  s_queued = 0;
  s_ticks = 0;
  s_tick_mask = 0;
  stub_pebble_tasks_set_current(PebbleTask_App);
}

void test_tick_timer__cleanup(void) {
  while (s_queued > 0) {
    prv_run_next_request();
  }
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
