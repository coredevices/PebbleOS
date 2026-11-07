/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/analytics/analytics.h>
#include <pbl/analytics/backend.h>
#include <pbl/kernel/heap.h>
#include <pbl/services/time.h>

#include <clar.h>
#include <fake_sys_work.h>
#include <fake_work.h>
#include <system/reboot_reason.h>
#include <system/version.h>

const FirmwareMetadata TINTIN_METADATA = {};

RebootReasonCode reboot_reason_get_last_reboot_reason(void) {
  return RebootReasonCode_Unknown;
}

uint32_t time_get_uptime_seconds(void) {
  return 0;
}

size_t pbl_heap_min_headroom(const struct pbl_heap *heap) {
  return 0;
}

void pbl_heap_reset_high_water_mark(struct pbl_heap *heap) {
}

void pbl_heap_stats(struct pbl_heap *heap, unsigned int *used, unsigned int *free,
                    unsigned int *max_free) {
  *used = 0;
  *free = 0;
  *max_free = 0;
}

static int s_heartbeat_count;
static int s_collect_a_count;
static int s_collect_b_count;
static int s_collected_before_heartbeat;

void pbl_analytics__native_init(void) {
}

void pbl_analytics__native_heartbeat(void) {
  s_heartbeat_count++;
  s_collected_before_heartbeat = s_collect_a_count + s_collect_b_count;
}

static void prv_set_signed(enum pbl_analytics_key key, int32_t value) {
}

static void prv_set_unsigned(enum pbl_analytics_key key, uint32_t value) {
}

static void prv_set_string(enum pbl_analytics_key key, const char *value) {
}

static void prv_timer(enum pbl_analytics_key key) {
}

static void prv_add(enum pbl_analytics_key key, int32_t amount) {
}

const struct pbl_analytics_backend_ops pbl_analytics__native_ops = {
  .set_signed = prv_set_signed,
  .set_unsigned = prv_set_unsigned,
  .set_string = prv_set_string,
  .timer_start = prv_timer,
  .timer_stop = prv_timer,
  .add = prv_add,
};

static void prv_collect_a(void) {
  s_collect_a_count++;
}
PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_a);

static void prv_collect_b(void) {
  s_collect_b_count++;
}
PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_b);

void test_analytics__initialize(void) {
  s_heartbeat_count = 0;
  s_collect_a_count = 0;
  s_collect_b_count = 0;
  s_collected_before_heartbeat = 0;
  fake_work_cleanup();
  fake_sys_work_callbacks_cleanup();
  pbl_analytics_init();
}

void test_analytics__collectors_run_before_each_heartbeat(void) {
  struct pbl_delayed_work *timer = fake_work_get_next();

  for (int i = 1; i <= 2; i++) {
    fake_work_fire(timer);
    cl_assert_equal_i(s_heartbeat_count, i - 1);

    fake_sys_work_callbacks_invoke_pending();

    cl_assert_equal_i(s_heartbeat_count, i);
    cl_assert_equal_i(s_collect_a_count, i);
    cl_assert_equal_i(s_collect_b_count, i);
    cl_assert_equal_i(s_collected_before_heartbeat, 2 * i);
  }
}
