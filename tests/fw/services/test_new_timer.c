/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/kernel.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/new_timer/new_timer_service.h>
#include <pbl/task_wdt/task_wdt.h>

#include <clar.h>
#include <kernel/pebble_tasks.h>
#include <kernel_test.h>

// Timers on the real kernel, with time delivered by the POSIX arch.

void new_timer_reset_for_test(void);

#define NUM_TIMERS 8

static struct pbl_thread *s_nt;
static struct pbl_thread s_main;
static uint8_t s_main_stack[4096] __attribute__((aligned(8)));
static uint64_t s_fired_at[8];
static int s_fires;
static TimerID s_id;
static uint32_t s_data;

// ---- stubs ------------------------------------------------------------------

void task_init(void) {
}

int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                     pbl_task_wdt_callback_t callback, void *user_data) {
  return 0;
}

void pbl_task_wdt_set_waiting(bool waiting) {
}

void pebble_task_register(PebbleTask pebble_task, struct pbl_thread *thread) {
  s_nt = thread;
}

// ---- helpers ----------------------------------------------------------------

static void prv_record(void *data) {
  cl_assert(data == &s_data);
  cl_assert(pbl_thread_current() == s_nt);
  cl_assert(new_timer_debug_get_current_callback() == (void *)prv_record);
  s_fired_at[s_fires++] = pbl_uptime_ticks();
}

static void prv_run(void (*entry)(void *)) {
  struct pbl_thread_attr attr = {
    .name = "main",
    .entry = entry,
    .prio = 1,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
  };
  cl_assert_equal_i(pbl_thread_create(&s_main, &attr), 0);
  new_timer_service_init();
  pbl_test_kernel_run();
}

void test_new_timer__initialize(void) {
  s_nt = nullptr;
  memset(&s_main, 0, sizeof(s_main));
  memset(s_fired_at, 0, sizeof(s_fired_at));
  s_fires = 0;
  s_id = TIMER_INVALID_ID;
  new_timer_reset_for_test();
}

void test_new_timer__cleanup(void) {
}

// ---- tests ------------------------------------------------------------------

static void prv_one_shot(void *arg) {
  uint32_t expire_ms = 0;
  TimerID id = new_timer_create();
  cl_assert(id != TIMER_INVALID_ID);
  cl_assert(!new_timer_scheduled(id, nullptr));
  cl_assert(new_timer_start(id, pbl_ticks_to_ms(10), prv_record, &s_data, 0));
  cl_assert(new_timer_scheduled(id, &expire_ms));
  cl_assert(expire_ms > 0);
  pbl_thread_sleep(PBL_TICKS(20));
  cl_assert_equal_i(s_fires, 1);
  cl_assert(!new_timer_scheduled(id, nullptr));
  new_timer_delete(id);
  pbl_test_kernel_stop();
}

void test_new_timer__one_shot(void) {
  prv_run(prv_one_shot);
}

static void prv_zero_timeout(void *arg) {
  TimerID id = new_timer_create();
  cl_assert(new_timer_start(id, 0, prv_record, &s_data, 0));
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_fires, 1);
  new_timer_delete(id);
  pbl_test_kernel_stop();
}

void test_new_timer__zero_timeout_fires(void) {
  prv_run(prv_zero_timeout);
}

static void prv_repeating(void *arg) {
  TimerID id = new_timer_create();
  const pbl_tick_t period = pbl_ms_to_ticks(100);
  const uint64_t start = pbl_uptime_ticks();
  cl_assert(new_timer_start(id, 100, prv_record, &s_data, TIMER_START_FLAG_REPEATING));
  pbl_thread_sleep(PBL_TICKS(3 * period + 1));
  cl_assert_equal_i(s_fires, 3);
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(s_fired_at[i], start + (uint64_t)(i + 1) * period);
  }
  cl_assert(new_timer_stop(id));
  pbl_thread_sleep(PBL_TICKS(2 * period));
  cl_assert_equal_i(s_fires, 3);
  new_timer_delete(id);
  pbl_test_kernel_stop();
}

void test_new_timer__repeating_stays_on_its_period(void) {
  prv_run(prv_repeating);
}

static void prv_slow_record(void *data) {
  s_fired_at[s_fires++] = pbl_uptime_ticks();
  pbl_thread_sleep(PBL_TICKS(25));
}

static void prv_repeating_overrun(void *arg) {
  TimerID id = new_timer_create();
  cl_assert(new_timer_start(id, pbl_ticks_to_ms(10), prv_slow_record, &s_data,
                            TIMER_START_FLAG_REPEATING));
  pbl_thread_sleep(PBL_TICKS(70));
  // The third callback is still running.
  cl_assert(!new_timer_stop(id));
  pbl_thread_sleep(PBL_TICKS(30));
  // No burst for the missed periods: the next fire is a tick after the callback returns.
  cl_assert_equal_i(s_fires, 3);
  cl_assert_equal_i(s_fired_at[1] - s_fired_at[0], 26);
  cl_assert_equal_i(s_fired_at[2] - s_fired_at[1], 26);
  new_timer_delete(id);
  pbl_test_kernel_stop();
}

void test_new_timer__repeating_overrun_does_not_burst(void) {
  prv_run(prv_repeating_overrun);
}

static void prv_restart_pushes_back(void *arg) {
  TimerID id = new_timer_create();
  cl_assert(new_timer_start(id, pbl_ticks_to_ms(10), prv_record, &s_data, 0));
  pbl_thread_sleep(PBL_TICKS(5));
  cl_assert(!new_timer_start(id, 1, prv_record, &s_data, TIMER_START_FLAG_FAIL_IF_SCHEDULED));
  cl_assert(new_timer_start(id, pbl_ticks_to_ms(10), prv_record, &s_data, 0));
  pbl_thread_sleep(PBL_TICKS(8));
  cl_assert_equal_i(s_fires, 0);
  pbl_thread_sleep(PBL_TICKS(4));
  cl_assert_equal_i(s_fires, 1);
  new_timer_delete(id);
  pbl_test_kernel_stop();
}

void test_new_timer__restart_pushes_back(void) {
  prv_run(prv_restart_pushes_back);
}

static PBL_SEM_DEFINE(s_gate, 0, 1);

static void prv_blocks(void *data) {
  s_fires++;
  pbl_sem_take(&s_gate, PBL_FOREVER);
}

static void prv_executing(void *arg) {
  TimerID id = new_timer_create();
  cl_assert(new_timer_start(id, 0, prv_blocks, nullptr, TIMER_START_FLAG_REPEATING));
  pbl_thread_sleep(PBL_TICKS(1));
  cl_assert_equal_i(s_fires, 1);
  cl_assert(!new_timer_start(id, 1, prv_blocks, nullptr, TIMER_START_FLAG_FAIL_IF_EXECUTING));
  cl_assert(!new_timer_stop(id));
  // Freed once the callback returns, and a stop means no repeat.
  new_timer_delete(id);
  pbl_sem_give(&s_gate);
  pbl_thread_sleep(PBL_TICKS(5));
  cl_assert_equal_i(s_fires, 1);
  pbl_test_kernel_stop();
}

void test_new_timer__stop_and_delete_while_executing(void) {
  pbl_sem_init(&s_gate, 0, 1);
  prv_run(prv_executing);
}

static void prv_stops_itself(void *data) {
  s_fires++;
  cl_assert(!new_timer_stop(s_id));
}

static void prv_restarts_itself(void *data) {
  s_fires++;
  if (s_fires == 1) {
    // Replaces the repeat with a one-shot.
    cl_assert(new_timer_start(s_id, pbl_ticks_to_ms(3), prv_restarts_itself, nullptr, 0));
  }
}

static void prv_deletes_itself(void *data) {
  s_fires++;
  new_timer_delete(s_id);
}

static void prv_from_callback(void *arg) {
  s_id = new_timer_create();
  new_timer_start(s_id, 1, prv_stops_itself, nullptr, TIMER_START_FLAG_REPEATING);
  pbl_thread_sleep(PBL_TICKS(10));
  cl_assert_equal_i(s_fires, 1);

  s_fires = 0;
  new_timer_start(s_id, 1, prv_restarts_itself, nullptr, TIMER_START_FLAG_REPEATING);
  pbl_thread_sleep(PBL_TICKS(10));
  cl_assert_equal_i(s_fires, 2);

  s_fires = 0;
  new_timer_start(s_id, 1, prv_deletes_itself, nullptr, 0);
  pbl_thread_sleep(PBL_TICKS(10));
  cl_assert_equal_i(s_fires, 1);
  pbl_test_kernel_stop();
}

void test_new_timer__stop_restart_delete_from_callback(void) {
  prv_run(prv_from_callback);
}

static void prv_exhaust_reuse(void *arg) {
  // Slots are recycled well past the pool size, with new ids.
  TimerID last = TIMER_INVALID_ID;
  for (int i = 0; i < 4 * NUM_TIMERS; i++) {
    TimerID id = new_timer_create();
    cl_assert(id != TIMER_INVALID_ID && id != last);
    new_timer_delete(id);
    last = id;
  }
  TimerID ids[NUM_TIMERS];
  for (int i = 0; i < NUM_TIMERS; i++) {
    ids[i] = new_timer_create();
    new_timer_start(ids[i], pbl_ticks_to_ms(i + 1), prv_record, &s_data, 0);
  }
  pbl_thread_sleep(PBL_TICKS(NUM_TIMERS + 1));
  cl_assert_equal_i(s_fires, NUM_TIMERS);
  for (int i = 0; i < NUM_TIMERS; i++) {
    new_timer_delete(ids[i]);
  }
  pbl_test_kernel_stop();
}

void test_new_timer__ids_change_and_slots_recycle(void) {
  prv_run(prv_exhaust_reuse);
}

static void prv_work_a(void *data) {
  s_fired_at[s_fires++] = (uintptr_t)data;
}

static void prv_work_burst(void *arg) {
  // All of them, on one wake-up and with no timer due.
  pbl_test_isr_enter();
  new_timer_add_work_callback_from_isr(prv_work_a, (void *)1);
  new_timer_add_work_callback_from_isr(prv_work_a, (void *)2);
  new_timer_add_work_callback_from_isr(prv_work_a, (void *)3);
  pbl_test_isr_exit();
  cl_assert_equal_i(s_fires, 3);
  cl_assert_equal_i(s_fired_at[2], 3);
  pbl_test_kernel_stop();
}

void test_new_timer__work_callbacks_all_run_on_one_wake(void) {
  prv_run(prv_work_burst);
}
