/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/kernel/kernel.h"
#include "pbl/os/assert.h"
#include "system/reboot_reason.h"
#include <pbl/drivers/watchdog.h>
#include <pbl/task_wdt/task_wdt.h>

#include "kernel_test.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stubs_passert.h"

// The subsystem on the POSIX kernel: time only moves when every thread is
// blocked, so the watchdog thread's periodic sleep drives the clock.

PBL_NORETURN void os_assertion_failed(const char *filename, int line) {
  fprintf(stderr, "kernel assert at %s:%d\n", filename, line);
  abort();
}

PBL_NORETURN void os_assertion_failed_lr(const char *filename, int line, uint32_t lr) {
  os_assertion_failed(filename, line);
}

void pbl_task_wdt_reset_for_test(void);

#define STACK 4096
static uint8_t s_stacks[4][STACK] __attribute__((aligned(8)));
static struct pbl_thread s_threads[4];

#define PERIOD_MS  CONFIG_TASK_WDT_CHECK_PERIOD_MS
#define GRACE_MS   CONFIG_TASK_WDT_GRACE_MS
#define TIMEOUT_MS 2000

static int s_hw_feeds;
static RebootReason s_reason;

static int s_callbacks;
static int s_callback_channel;
static uint32_t s_first_callback_ms;
static void *s_callback_work;

static bool s_reset;
static uint32_t s_reset_ms;

static uint32_t prv_now_ms(void) {
  return pbl_ticks_to_ms(pbl_uptime_ticks());
}

// ---- stubs ------------------------------------------------------------------

void pbl_log_sync(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
                  ...) {
}

void pbl_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
}

void watchdog_feed(void) {
  s_hw_feeds++;
}

void reboot_reason_set(RebootReason *reason) {
  if (s_reason.code == RebootReasonCode_Unknown) {
    s_reason = *reason;
  }
}

void reboot_reason_get(RebootReason *reason) {
  *reason = s_reason;
}

void reboot_reason_clear(void) {
  memset(&s_reason, 0, sizeof(s_reason));
}

PBL_NORETURN void reset_due_to_software_failure(void) {
  s_reset = true;
  s_reset_ms = prv_now_ms();
  pbl_test_kernel_stop();
  abort();
}

// ---- helpers ----------------------------------------------------------------

static struct pbl_thread *prv_spawn(int i, const char *name, void (*entry)(void *), void *arg) {
  struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .arg = arg,
    .prio = 2,
    .privileged = true,
    .stack = s_stacks[i],
    .stack_size = STACK,
  };
  cl_assert_equal_i(pbl_thread_create(&s_threads[i], &attr), 0);
  return &s_threads[i];
}

static void *prv_callback(int channel_id, void *user_data) {
  if (s_callbacks++ == 0) {
    s_first_callback_ms = prv_now_ms();
    s_callback_channel = channel_id;
  }
  s_callback_work = user_data;
  return user_data;
}

static void prv_sleep_ms(uint32_t ms) {
  pbl_thread_sleep(PBL_MSEC(ms));
}

void test_task_wdt__initialize(void) {
  memset(s_threads, 0, sizeof(s_threads));
  memset(&s_reason, 0, sizeof(s_reason));
  s_hw_feeds = 0;
  s_callbacks = 0;
  s_callback_channel = -1;
  s_first_callback_ms = 0;
  s_callback_work = NULL;
  s_reset = false;
  s_reset_ms = 0;
  pbl_task_wdt_reset_for_test();
  pbl_task_wdt_init();
}

void test_task_wdt__cleanup(void) {
}

// ---- tests ------------------------------------------------------------------

static void prv_feeding_entry(void *arg) {
  int ch = pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL);
  cl_assert(ch >= 0);
  for (int i = 0; i < 10; i++) {
    prv_sleep_ms(TIMEOUT_MS / 2);
    cl_assert_equal_i(pbl_task_wdt_feed(ch), 0);
  }
  pbl_test_kernel_stop();
}

void test_task_wdt__fed_channel_never_expires(void) {
  prv_spawn(0, "fed", prv_feeding_entry, NULL);
  pbl_test_kernel_run();
  cl_assert_equal_i(s_callbacks, 0);
  cl_assert(!s_reset);
  // Ten half-timeouts of feeding is 10 s: the hardware watchdog got fed
  // once per check period throughout.
  cl_assert(s_hw_feeds >= 10 * (TIMEOUT_MS / 2) / PERIOD_MS - 1);
}

static int s_work;

static void prv_stuck_entry(void *arg) {
  cl_assert(pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, &s_work) >= 0);
  // Blocked forever from the kernel's point of view; only the watchdog runs.
  prv_sleep_ms(60 * 1000);
  cl_fail("the stuck thread outlived the watchdog");
}

void test_task_wdt__unfed_channel_calls_back_then_resets(void) {
  prv_spawn(0, "stuck", prv_stuck_entry, NULL);
  pbl_test_kernel_run();

  cl_assert(s_callbacks >= 1);
  cl_assert(s_first_callback_ms >= TIMEOUT_MS);
  cl_assert(s_first_callback_ms < TIMEOUT_MS + PERIOD_MS);
  cl_assert_equal_i(s_callback_channel, 0);

  cl_assert(s_reset);
  cl_assert(s_reset_ms >= TIMEOUT_MS + GRACE_MS);
  cl_assert(s_reset_ms < TIMEOUT_MS + GRACE_MS + PERIOD_MS);
  // The callback ran on every check inside the grace period.
  cl_assert_equal_i(s_callbacks, GRACE_MS / PERIOD_MS + 1);

  cl_assert_equal_i(s_reason.code, RebootReasonCode_Watchdog);
  cl_assert_equal_i(s_reason.data8[0], 0x0);
  cl_assert_equal_i(s_reason.data8[1], 0x1);
  cl_assert_equal_i(s_reason.watchdog.stuck_task_callback, (uint32_t)(uintptr_t)&s_work);
}

static void prv_late_feed_entry(void *arg) {
  int ch = pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL);
  // Expire, then recover inside the grace period.
  prv_sleep_ms(TIMEOUT_MS + PERIOD_MS / 2);
  cl_assert(s_callbacks >= 1);
  cl_assert_equal_i(s_reason.code, RebootReasonCode_Watchdog);
  pbl_task_wdt_feed(ch);
  prv_sleep_ms(TIMEOUT_MS / 2);
  pbl_test_kernel_stop();
}

void test_task_wdt__feed_inside_grace_recovers(void) {
  prv_spawn(0, "late", prv_late_feed_entry, NULL);
  pbl_test_kernel_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_reason.code, RebootReasonCode_Unknown);
}

static void prv_suspended_entry(void *arg) {
  pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL);
  pbl_task_wdt_suspend(3 * TIMEOUT_MS);
  prv_sleep_ms(2 * TIMEOUT_MS);
  cl_assert_equal_i(s_callbacks, 0);
  // The suspension ends at 3 timeouts and the channel restarts from there.
  prv_sleep_ms(2 * TIMEOUT_MS + 2 * PERIOD_MS);
  cl_assert(s_callbacks >= 1);
  cl_assert(s_first_callback_ms >= 4 * TIMEOUT_MS);
  pbl_test_kernel_stop();
}

void test_task_wdt__timed_suspend_defers_expiry(void) {
  prv_spawn(0, "suspended", prv_suspended_entry, NULL);
  pbl_test_kernel_run();
}

static void prv_resume_entry(void *arg) {
  pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL);
  pbl_task_wdt_suspend(0);
  prv_sleep_ms(5 * TIMEOUT_MS);
  cl_assert_equal_i(s_callbacks, 0);
  uint32_t resumed_ms = prv_now_ms();
  pbl_task_wdt_resume();
  prv_sleep_ms(TIMEOUT_MS + 2 * PERIOD_MS);
  cl_assert(s_callbacks >= 1);
  cl_assert(s_first_callback_ms >= resumed_ms + TIMEOUT_MS);
  pbl_test_kernel_stop();
}

void test_task_wdt__resume_restarts_timeouts(void) {
  prv_spawn(0, "resumed", prv_resume_entry, NULL);
  pbl_test_kernel_run();
}

static void prv_self_feeder_entry(void *arg) {
  cl_assert(pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL) >= 0);
  for (;;) {
    prv_sleep_ms(TIMEOUT_MS / 2);
    pbl_task_wdt_feed_self();
  }
}

static void prv_bystander_entry(void *arg) {
  cl_assert(pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL) >= 0);
  prv_sleep_ms(60 * 1000);
}

void test_task_wdt__feed_self_leaves_other_channels_alone(void) {
  prv_spawn(0, "feeder", prv_self_feeder_entry, NULL);
  prv_spawn(1, "bystander", prv_bystander_entry, NULL);
  pbl_test_kernel_run();
  cl_assert(s_reset);
  cl_assert_equal_i(s_callback_channel, 1);
  cl_assert_equal_i(s_reason.data8[0], 0x1);
  cl_assert_equal_i(s_reason.data8[1], 0x3);
}

static void prv_all_feeder_entry(void *arg) {
  for (int i = 0; i < 12; i++) {
    prv_sleep_ms(TIMEOUT_MS / 2);
    pbl_task_wdt_feed_all();
  }
  pbl_test_kernel_stop();
}

void test_task_wdt__feed_all_covers_every_channel(void) {
  prv_spawn(0, "feeder", prv_all_feeder_entry, NULL);
  prv_spawn(1, "bystander", prv_bystander_entry, NULL);
  pbl_test_kernel_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_callbacks, 0);
}

static void prv_thread_feeder_entry(void *arg) {
  struct pbl_thread *other = arg;
  for (int i = 0; i < 12; i++) {
    prv_sleep_ms(TIMEOUT_MS / 2);
    pbl_task_wdt_feed_thread(other);
  }
  pbl_test_kernel_stop();
}

void test_task_wdt__feed_thread_feeds_on_behalf(void) {
  struct pbl_thread *bystander = prv_spawn(1, "bystander", prv_bystander_entry, NULL);
  prv_spawn(0, "feeder", prv_thread_feeder_entry, bystander);
  pbl_test_kernel_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_callbacks, 0);
}

static void prv_delete_entry(void *arg) {
  int ch = pbl_task_wdt_add(NULL, TIMEOUT_MS, prv_callback, NULL);
  cl_assert_equal_i(pbl_task_wdt_delete(ch), 0);
  cl_assert_equal_i(pbl_task_wdt_delete(ch), -EINVAL);
  cl_assert_equal_i(pbl_task_wdt_feed(ch), -EINVAL);
  prv_sleep_ms(3 * TIMEOUT_MS);
  pbl_test_kernel_stop();
}

void test_task_wdt__deleted_channel_is_forgotten(void) {
  prv_spawn(0, "deleter", prv_delete_entry, NULL);
  pbl_test_kernel_run();
  cl_assert(!s_reset);
  cl_assert_equal_i(s_callbacks, 0);
}

static void prv_exhaust_entry(void *arg) {
  int ids[CONFIG_TASK_WDT_CHANNELS];
  for (int i = 0; i < CONFIG_TASK_WDT_CHANNELS; i++) {
    ids[i] = pbl_task_wdt_add(NULL, TIMEOUT_MS, NULL, NULL);
    cl_assert_equal_i(ids[i], i);
  }
  cl_assert_equal_i(pbl_task_wdt_add(NULL, TIMEOUT_MS, NULL, NULL), -ENOMEM);
  cl_assert_equal_i(pbl_task_wdt_delete(ids[1]), 0);
  cl_assert_equal_i(pbl_task_wdt_add(NULL, TIMEOUT_MS, NULL, NULL), 1);
  pbl_test_kernel_stop();
}

void test_task_wdt__channel_pool_is_bounded(void) {
  prv_spawn(0, "exhaust", prv_exhaust_entry, NULL);
  pbl_test_kernel_run();
}
