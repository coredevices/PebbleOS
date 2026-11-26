/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <fake_rtc.h>
#include <fake_work.h>
#include <nimble/nimble_npl.h>

static struct ble_npl_callout s_co;
static int s_num_events;

static void prv_event(struct ble_npl_event *ev) {
  s_num_events++;
}

void test_nimble_port__initialize(void) {
  fake_rtc_init(0, 0);
  fake_work_cleanup();
  s_num_events = 0;

  ble_npl_callout_init(&s_co, nullptr, prv_event, nullptr);
}

void test_nimble_port__cleanup(void) {
  ble_npl_callout_stop(&s_co);
}

void test_nimble_port__callout_runs_its_event(void) {
  cl_assert(!ble_npl_callout_is_active(&s_co));

  cl_assert_equal_i(ble_npl_callout_reset(&s_co, 300), BLE_NPL_OK);
  cl_assert(ble_npl_callout_is_active(&s_co));
  cl_assert_equal_i(s_num_events, 0);

  fake_work_fire(fake_work_get_next());
  cl_assert_equal_i(s_num_events, 1);
  cl_assert(!ble_npl_callout_is_active(&s_co));
}

void test_nimble_port__stopped_callout_does_not_run(void) {
  cl_assert_equal_i(ble_npl_callout_reset(&s_co, 300), BLE_NPL_OK);
  ble_npl_callout_stop(&s_co);

  cl_assert(!ble_npl_callout_is_active(&s_co));
  cl_assert(fake_work_get_next() == nullptr);
}

/* The host pulls its timer in when a timeout is due before the time this reports. */
void test_nimble_port__callout_reports_when_it_expires(void) {
  fake_rtc_increment_ticks(5000);

  cl_assert_equal_i(ble_npl_callout_reset(&s_co, 300), BLE_NPL_OK);
  cl_assert_equal_i(ble_npl_callout_get_ticks(&s_co), ble_npl_time_get() + 300);

  fake_rtc_increment_ticks(100);
  cl_assert((ble_npl_stime_t)(ble_npl_time_get() + 100 - ble_npl_callout_get_ticks(&s_co)) < 0);
  cl_assert((ble_npl_stime_t)(ble_npl_time_get() + 250 - ble_npl_callout_get_ticks(&s_co)) > 0);
}
