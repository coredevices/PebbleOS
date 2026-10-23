/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/kernel.h>
#include <pbl/services/system_task.h>

#include <clar.h>
#include <kernel/pebble_tasks.h>
#include <kernel_test.h>
#include <system/reboot_reason.h>

#define QUEUE_LENGTH 30
#define FLOOD        (3 * QUEUE_LENGTH)

void system_task_reset_for_test(void);
void stub_pbl_malloc_set_kernel_malloc_should_fail(bool should_fail);

static struct pbl_thread s_bg;
static struct pbl_thread s_main;
static uint8_t s_bg_stack[4096] __attribute__((aligned(8)));
static uint8_t s_main_stack[4096] __attribute__((aligned(8)));

static RebootReason s_reason;
static bool s_reset;

static int s_ran[FLOOD];
static int s_num_ran;
static pbl_tick_t s_flood_start;
static pbl_tick_t s_flood_ticks;

// ---- stubs ------------------------------------------------------------------

void task_init(void) {
}

void mcu_fpu_cleanup(void) {
}

struct pbl_thread *pebble_task_create(PebbleTask pebble_task, struct pbl_thread_attr *attr) {
  struct pbl_thread_attr bg = *attr;
  bg.stack = s_bg_stack;
  bg.stack_size = sizeof(s_bg_stack);
  cl_assert_equal_i(pbl_thread_create(&s_bg, &bg), 0);
  return &s_bg;
}

PebbleTask pebble_task_get_current(void) {
  return pbl_thread_current() == &s_bg ? PebbleTask_KernelBackground : PebbleTask_KernelMain;
}

struct pbl_thread *pebble_task_get_thread(PebbleTask task) {
  return task == PebbleTask_KernelBackground ? &s_bg : nullptr;
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
    system_task_add_callback(prv_record, (void *)(uintptr_t)i);
  }
  s_flood_ticks = pbl_uptime_ticks() - s_flood_start;
  system_task_add_callback(prv_stop, nullptr);
}

static void prv_main_entry(void *arg) {
  system_task_init();
  system_task_add_callback(prv_flood, nullptr);
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

void test_system_task__initialize(void) {
  memset(&s_bg, 0, sizeof(s_bg));
  memset(&s_main, 0, sizeof(s_main));
  memset(&s_reason, 0, sizeof(s_reason));
  memset(s_ran, 0, sizeof(s_ran));
  s_reset = false;
  s_num_ran = 0;
  s_flood_start = 0;
  s_flood_ticks = 0;
  stub_pbl_malloc_set_kernel_malloc_should_fail(false);
  system_task_reset_for_test();
}

void test_system_task__cleanup(void) {
  stub_pbl_malloc_set_kernel_malloc_should_fail(false);
}

// ---- tests ------------------------------------------------------------------

void test_system_task__overflowing_own_queue_never_blocks(void) {
  prv_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_flood_ticks, 0);
  cl_assert_equal_i(s_num_ran, FLOOD);
  for (int i = 0; i < FLOOD; i++) {
    cl_assert_equal_i(s_ran[i], i);
  }
}

void test_system_task__overflow_without_memory_resets(void) {
  stub_pbl_malloc_set_kernel_malloc_should_fail(true);
  prv_run();
  cl_assert(s_reset);
  cl_assert_equal_i(s_reason.code, RebootReasonCode_EventQueueFull);
  cl_assert_equal_i(s_flood_ticks, 0);
}
