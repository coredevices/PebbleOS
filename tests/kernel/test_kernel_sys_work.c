/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/kernel.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <kernel_test.h>

#define FLOOD (3 * CONFIG_KERNEL_SYS_WORKQ_CALLS)

void sys_workq_init(void);
void sys_workq_reset_for_test(void);
void sys_work_reset_for_test(void);
void stub_pbl_malloc_set_should_fail(bool should_fail);

static struct pbl_thread s_main;
static uint8_t s_main_stack[4096] __attribute__((aligned(8)));
static bool s_panicked;
static int s_ran[FLOOD];
static int s_num_ran;
static pbl_tick_t s_flood_start;
static pbl_tick_t s_flood_ticks;
static pbl_work_call_fn_t s_first_fn;
static struct pbl_work_pool *s_main_pool;

// ---- stubs ------------------------------------------------------------------

void mcu_fpu_cleanup(void) {
}

[[noreturn]] void pbl_panic_at(const char *filename, int line, const char *fmt, ...) {
  s_panicked = true;
  s_flood_ticks = pbl_uptime_ticks() - s_flood_start;
  pbl_test_kernel_stop();
  abort();
}

// ---- helpers ----------------------------------------------------------------

static void prv_record(void *data) {
  s_ran[s_num_ran++] = (int)(uintptr_t)data;
}

static void prv_stop(void *data) {
  pbl_test_kernel_stop();
}

static void prv_flood(void *data) {
  s_flood_start = pbl_uptime_ticks();
  for (int i = 0; i < FLOOD; i++) {
    pbl_sys_work_call(prv_record, (void *)(uintptr_t)i);
  }
  s_flood_ticks = pbl_uptime_ticks() - s_flood_start;
  pbl_sys_work_call(prv_stop, nullptr);
}

static void prv_check_culprit(void *data) {
  cl_assert(pbl_sys_work_current_fn() == (void *)prv_check_culprit);
  s_num_ran++;
}

static void prv_queue_culprits(void *data) {
  // Through the pool, then on the heap once it fills up.
  for (int i = 0; i < FLOOD; i++) {
    pbl_sys_work_call(prv_check_culprit, nullptr);
  }
  pbl_sys_work_call(prv_stop, nullptr);
}

static void prv_main_entry(void *arg) {
  sys_workq_init();
  pbl_sys_work_call(s_first_fn, nullptr);
  pbl_thread_sleep(PBL_FOREVER);
}

static void prv_run(void) {
  struct pbl_thread_attr attr = {
    .name = "main",
    .entry = prv_main_entry,
    .prio = 3,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
    .work_pool = s_main_pool,
  };
  cl_assert_equal_i(pbl_thread_create(&s_main, &attr), 0);
  pbl_test_kernel_run();
}

void test_kernel_sys_work__initialize(void) {
  memset(&s_main, 0, sizeof(s_main));
  memset(s_ran, 0, sizeof(s_ran));
  s_panicked = false;
  s_num_ran = 0;
  s_flood_start = 0;
  s_flood_ticks = 0;
  s_first_fn = prv_flood;
  s_main_pool = nullptr;
  stub_pbl_malloc_set_should_fail(false);
  sys_workq_reset_for_test();
  sys_work_reset_for_test();
}

void test_kernel_sys_work__cleanup(void) {
  stub_pbl_malloc_set_should_fail(false);
}

// ---- tests ------------------------------------------------------------------

void test_kernel_sys_work__overflowing_own_queue_never_blocks(void) {
  prv_run();
  cl_assert(!s_panicked);
  cl_assert_equal_i(s_flood_ticks, 0);
  cl_assert_equal_i(s_num_ran, FLOOD);
  for (int i = 0; i < FLOOD; i++) {
    cl_assert_equal_i(s_ran[i], i);
  }
}

void test_kernel_sys_work__overflow_without_memory_stops_the_system(void) {
  stub_pbl_malloc_set_should_fail(true);
  prv_run();
  cl_assert(s_panicked);
  cl_assert_equal_i(s_flood_ticks, 0);
}

void test_kernel_sys_work__reports_the_function_behind_a_heap_call(void) {
  s_first_fn = prv_queue_culprits;
  prv_run();
  cl_assert(!s_panicked);
  cl_assert_equal_i(s_num_ran, FLOOD);
}

static void prv_high_record(void *data) {
  cl_assert(pbl_workq_is_current(&pbl_sys_workq_high));
  s_ran[s_num_ran++] = (int)(uintptr_t)data;
}

static void prv_high_burst(void *data) {
  // All of them, on one wake-up.
  pbl_test_isr_enter();
  pbl_sys_work_call_high_from_isr(prv_high_record, (void *)1);
  pbl_sys_work_call_high_from_isr(prv_high_record, (void *)2);
  pbl_sys_work_call_high_from_isr(prv_high_record, (void *)3);
  pbl_test_isr_exit();
  cl_assert_equal_i(s_num_ran, 3);
  cl_assert_equal_i(s_ran[2], 3);
  pbl_test_kernel_stop();
}

void test_kernel_sys_work__high_calls_run_on_one_wake(void) {
  s_first_fn = prv_high_burst;
  prv_run();
}

static void prv_blocked(void *data) {
  pbl_sys_work_block(true);
  cl_assert(!pbl_sys_work_call(prv_record, nullptr));
  cl_assert(!pbl_sys_work_try_call(prv_record, nullptr));
  pbl_sys_work_block(false);
  cl_assert(pbl_sys_work_try_call(prv_stop, nullptr));
}

void test_kernel_sys_work__blocked_calls_are_refused(void) {
  s_first_fn = prv_blocked;
  prv_run();
  cl_assert_equal_i(s_num_ran, 0);
}

void test_kernel_sys_work__a_thread_with_its_own_slots_uses_them(void) {
  static struct pbl_work_call slots[2];
  static struct pbl_work_pool pool;
  pbl_work_pool_init(&pool, slots, ARRAY_LENGTH(slots));
  s_main_pool = &pool;
  s_first_fn = prv_stop;
  prv_run();
  cl_assert(!s_panicked);
  cl_assert_equal_i(pbl_work_pool_num_free(&pool), 2);
}
