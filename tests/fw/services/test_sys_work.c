/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/kernel.h>
#include <pbl/services/sys_work.h>

#include <clar.h>
#include <kernel/pebble_tasks.h>
#include <kernel_test.h>
#include <system/reboot_reason.h>

#define QUEUE_LENGTH 30
void sys_workq_init(void);
void sys_workq_reset_for_test(void);

#define FLOOD (3 * QUEUE_LENGTH)

void sys_work_reset_for_test(void);
void stub_pbl_malloc_set_should_fail(bool should_fail);

static struct pbl_thread *s_bg;
static struct pbl_thread s_main;
static uint8_t s_main_stack[4096] __attribute__((aligned(8)));

static RebootReason s_reason;
static bool s_reset;

static int s_ran[FLOOD];
static int s_num_ran;
static pbl_tick_t s_flood_start;
static pbl_tick_t s_flood_ticks;
static SysWorkCallback s_first_cb;

// ---- stubs ------------------------------------------------------------------

void task_init(void) {
}

void mcu_fpu_cleanup(void) {
}

void pebble_task_register(PebbleTask task, struct pbl_thread *thread) {
  if (task == PebbleTask_KernelBackground) {
    s_bg = thread;
  }
}

PebbleTask pebble_task_get_current(void) {
  return pbl_thread_current() == s_bg ? PebbleTask_KernelBackground : PebbleTask_KernelMain;
}

struct pbl_thread *pebble_task_get_thread(PebbleTask task) {
  return task == PebbleTask_KernelBackground ? s_bg : nullptr;
}

void reboot_reason_set(RebootReason *reason) {
  s_reason = *reason;
}

[[noreturn]] void reset_due_to_software_failure(void) {
  s_reset = true;
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
    sys_work_add(prv_record, (void *)(uintptr_t)i);
  }
  s_flood_ticks = pbl_uptime_ticks() - s_flood_start;
  sys_work_add(prv_stop, nullptr);
}

static void prv_check_culprit(void *data) {
  cl_assert(sys_work_get_current_callback() == (void *)prv_check_culprit);
  s_num_ran++;
}

static void prv_queue_culprits(void *data) {
  // Through the pool, then the heap fallback once it fills up.
  for (int i = 0; i < FLOOD; i++) {
    sys_work_add(prv_check_culprit, nullptr);
  }
  cl_assert(sys_work_try_add_raised(prv_check_culprit, nullptr));
  sys_work_add(prv_stop, nullptr);
}

static void prv_main_entry(void *arg) {
  sys_workq_init();
  sys_work_init();
  sys_work_add(s_first_cb, nullptr);
  pbl_thread_sleep(PBL_FOREVER);
}

static void prv_run(void) {
  struct pbl_thread_attr attr = {
    .name = "KernelMain",
    .entry = prv_main_entry,
    .prio = 3,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
  };
  cl_assert_equal_i(pbl_thread_create(&s_main, &attr), 0);
  pbl_test_kernel_run();
}

void test_sys_work__initialize(void) {
  s_bg = nullptr;
  memset(&s_main, 0, sizeof(s_main));
  memset(&s_reason, 0, sizeof(s_reason));
  memset(s_ran, 0, sizeof(s_ran));
  s_reset = false;
  s_num_ran = 0;
  s_flood_start = 0;
  s_flood_ticks = 0;
  s_first_cb = prv_flood;
  stub_pbl_malloc_set_should_fail(false);
  sys_workq_reset_for_test();
  sys_work_reset_for_test();
}

void test_sys_work__cleanup(void) {
  stub_pbl_malloc_set_should_fail(false);
}

// ---- tests ------------------------------------------------------------------

void test_sys_work__overflowing_own_queue_never_blocks(void) {
  prv_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_flood_ticks, 0);
  cl_assert_equal_i(s_num_ran, FLOOD);
  for (int i = 0; i < FLOOD; i++) {
    cl_assert_equal_i(s_ran[i], i);
  }
}

void test_sys_work__overflow_without_memory_resets(void) {
  stub_pbl_malloc_set_should_fail(true);
  prv_run();
  cl_assert(s_reset);
  cl_assert_equal_i(s_reason.code, RebootReasonCode_EventQueueFull);
  cl_assert_equal_i(s_flood_ticks, 0);
}

static void prv_high_record(void *data) {
  cl_assert(pbl_workq_is_current(&pbl_sys_workq_high));
  s_ran[s_num_ran++] = (int)(uintptr_t)data;
}

static void prv_high_burst(void *data) {
  // All of them, on one wake-up.
  pbl_test_isr_enter();
  sys_work_add_high_from_isr(prv_high_record, (void *)1);
  sys_work_add_high_from_isr(prv_high_record, (void *)2);
  sys_work_add_high_from_isr(prv_high_record, (void *)3);
  pbl_test_isr_exit();
  cl_assert_equal_i(s_num_ran, 3);
  cl_assert_equal_i(s_ran[2], 3);
  pbl_test_kernel_stop();
}

void test_sys_work__high_callbacks_run_on_one_wake(void) {
  s_first_cb = prv_high_burst;
  prv_run();
}

void test_sys_work__reports_the_callback_behind_a_wrapper(void) {
  s_first_cb = prv_queue_culprits;
  prv_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_num_ran, FLOOD + 1);
}
