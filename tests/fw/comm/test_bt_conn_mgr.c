/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "comm/ble/gap_le_connection.h"
#include "comm/bt_conn_mgr.h"
#include "comm/bt_conn_mgr_impl.h"
#include "services/common/regular_timer.h"

// Fakes
#include "fake_gap_le_connect_params.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_system_task.h"
#include "fake_pbl_malloc.h"

// Stubs
#include "stubs_bluetopia_interface.h"
#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

extern void conn_mgr_handle_desired_state_granted(GAPLEConnection *hdl,
                                                  ResponseTimeState granted_state);

// Stubs
/////
bool gap_le_connection_is_valid(const GAPLEConnection *conn) {
  return (conn != NULL);
}

GAPLEConnection *gap_le_connection_any(void) {
  return NULL;
}

void prv_regular_timer_spend_seconds(uint32_t seconds) {
  for (uint32_t i = 0; i < seconds; ++i) {
    fake_rtc_increment_ticks(RTC_TICKS_HZ);
    regular_timer_fire_seconds(1);

    // bt_conn_mgr offloads the callback to KernelBG
    fake_system_task_callbacks_invoke_pending();
  }
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Tests
///////////////////////////////////////////////////////////

static int s_granted_count;
static GAPLEConnection s_hdl;

void test_bt_conn_mgr__initialize(void) {
  s_granted_count = 0;
  fake_rtc_init(0, 0);
  regular_timer_init();
  fake_gap_le_connect_params_init();
  s_hdl.conn_mgr_info = bt_conn_mgr_info_init();
}

void test_bt_conn_mgr__cleanup(void) {
  regular_timer_deinit();
}

void test_bt_conn_mgr__ble_latency_mgr(void) {
  // 1 consumer at fastest rate should result in fastest rate getting scheduled
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMin, 100);

  uint16_t secs_to_wait;
  ResponseTimeState state;

  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMin);
  cl_assert_equal_i(secs_to_wait, 100);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeMin);

  // another consumer at lower rate should not have any effect
  fake_gap_le_connect_params_reset_last_requested();
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerUnitTests, ResponseTimeMiddle, 30);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMin);
  cl_assert_equal_i(secs_to_wait, 100);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeInvalid);

  // removing the fastest consumer should result in the next fastest being scheduled, but only
  // after an "inactivity timeout":
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMax, 0);

  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMin);
  cl_assert_equal_i(secs_to_wait, BT_CONN_MGR_INACTIVITY_TIMEOUT_SECS);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeInvalid);

  prv_regular_timer_spend_seconds(BT_CONN_MGR_INACTIVITY_TIMEOUT_SECS);

  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMiddle);
  cl_assert_equal_i(secs_to_wait, 30 - BT_CONN_MGR_INACTIVITY_TIMEOUT_SECS);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeMiddle);

  // removing all consumers we should fall back to slowest interval, but only
  // after an "inactivity timeout":
  fake_gap_le_connect_params_reset_last_requested();
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerUnitTests, ResponseTimeMax, 0);

  prv_regular_timer_spend_seconds(BT_CONN_MGR_INACTIVITY_TIMEOUT_SECS);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMax);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeMax);

  // if nothing else is scheduled, middle rate should get picked up right away
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerUnitTests, ResponseTimeMiddle, 30);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMiddle);
  cl_assert_equal_i(secs_to_wait, 30);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeMiddle);

  // higher rate should take over lower rate already scheduled
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMin, 25);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMin);
  cl_assert_equal_i(secs_to_wait, 25);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeMin);

  // two requests at same high rate, longest time should be selected as timeout
  fake_gap_le_connect_params_reset_last_requested();
  conn_mgr_set_ble_conn_response_time(
      &s_hdl, BtConsumerUnitTests, ResponseTimeMin, 250);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMin);
  cl_assert_equal_i(secs_to_wait, 250);
  cl_assert_equal_i(fake_gap_le_connect_params_get_last_requested(), ResponseTimeInvalid);

  bt_conn_mgr_info_deinit(&s_hdl.conn_mgr_info);
}

static void prv_granted_handler(void) {
  ++s_granted_count;
}

void test_bt_conn_mgr__granted_handler_request_max_no_existing_node(void) {
  fake_gap_le_connect_params_set_actual_state(ResponseTimeMax);
  conn_mgr_set_ble_conn_response_time_ext(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMax, 1,
                                          prv_granted_handler);
  // Expect granted handler to be called immediately:
  cl_assert_equal_i(s_granted_count, 1);
}

void test_bt_conn_mgr__granted_handler_request_existing(void) {
  fake_gap_le_connect_params_set_actual_state(ResponseTimeMax);
  conn_mgr_set_ble_conn_response_time_ext(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMin, 1,
                                          prv_granted_handler);
  cl_assert_equal_i(s_granted_count, 0);

  // Simulate that the requested state takes effect:
  fake_gap_le_connect_params_set_actual_state(ResponseTimeMin);
  conn_mgr_handle_desired_state_granted(&s_hdl, ResponseTimeMin);
  cl_assert_equal_i(s_granted_count, 1);

  conn_mgr_set_ble_conn_response_time_ext(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMin, 1,
                                          prv_granted_handler);
  cl_assert_equal_i(s_granted_count, 2);

  conn_mgr_set_ble_conn_response_time_ext(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMiddle,
                                          1, prv_granted_handler);
  cl_assert_equal_i(s_granted_count, 3);

  conn_mgr_set_ble_conn_response_time_ext(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMax,
                                          1, prv_granted_handler);
  cl_assert_equal_i(s_granted_count, 4);
}

void test_bt_conn_mgr__request_max_time_while_no_requests_are_running(void) {
  uint16_t secs_to_wait;
  ResponseTimeState state;

  // Always start off with ResponseTimeMax:
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMax);

  // Requesting ResponseTimeMax should have no effect:
  conn_mgr_set_ble_conn_response_time(&s_hdl, BtConsumerLeServiceDiscovery, ResponseTimeMax, 1);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMax);

  // Not even after waiting 10 seconds:
  prv_regular_timer_spend_seconds(10);
  state = conn_mgr_get_latency_for_le_connection(&s_hdl, &secs_to_wait);
  cl_assert_equal_i(state, ResponseTimeMax);

  bt_conn_mgr_info_deinit(&s_hdl.conn_mgr_info);
}
