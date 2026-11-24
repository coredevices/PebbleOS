/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/responsiveness.h"

#include <pbl/bluetooth/responsiveness.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_work.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>

#define CONN_HANDLE 1

#define FAST 12
#define SLOW 40

static ble_gap_event_fn *s_listener;
static struct ble_gap_conn_desc s_desc;
static struct ble_gap_upd_params s_requested;
static int s_num_requests;
static int s_request_rc;
static struct pbl_bt_responsiveness_req s_req;
static struct pbl_bt_responsiveness_req s_other_req;

int ble_gap_event_listener_register(struct ble_gap_event_listener *listener, ble_gap_event_fn *fn,
                                    void *arg) {
  s_listener = fn;
  return 0;
}

int ble_gap_conn_find(uint16_t handle, struct ble_gap_conn_desc *out_desc) {
  if (handle != CONN_HANDLE) {
    return BLE_HS_ENOTCONN;
  }

  *out_desc = s_desc;
  return 0;
}

int ble_gap_update_params(uint16_t conn_handle, const struct ble_gap_upd_params *params) {
  cl_assert_equal_i(conn_handle, CONN_HANDLE);
  s_requested = *params;
  s_num_requests++;
  return s_request_rc;
}

static void prv_connect(void) {
  struct ble_gap_event event = {
    .type = BLE_GAP_EVENT_CONNECT,
    .connect = {.conn_handle = CONN_HANDLE},
  };

  s_listener(&event, nullptr);
}

/* What the central answers a request with. */
static void prv_update(int status, uint16_t itvl, uint16_t latency) {
  struct ble_gap_event event = {
    .type = BLE_GAP_EVENT_CONN_UPDATE,
    .conn_update = {.status = status, .conn_handle = CONN_HANDLE},
  };

  if (status == 0) {
    s_desc.conn_itvl = itvl;
    s_desc.conn_latency = latency;
  }
  s_listener(&event, nullptr);
}

/* Lets the time pass, running what comes due. */
static void prv_spend_seconds(uint32_t seconds) {
  uint64_t remaining = (uint64_t)seconds * PBL_TICK_HZ;

  while (true) {
    struct pbl_delayed_work *work = fake_work_get_next();

    if (work == nullptr || work->timer.timeout.expires > remaining) {
      if (work != nullptr) {
        work->timer.timeout.expires -= remaining;
      }
      fake_rtc_increment_ticks(remaining);
      return;
    }

    remaining -= work->timer.timeout.expires;
    fake_rtc_increment_ticks(work->timer.timeout.expires);
    fake_work_fire(work);
  }
}

void test_responsiveness__initialize(void) {
  fake_rtc_init(0, 0);
  fake_work_cleanup();
  s_desc = (struct ble_gap_conn_desc){.conn_handle = CONN_HANDLE, .conn_itvl = 24};
  s_num_requests = 0;
  s_request_rc = 0;

  pbl_bt__responsiveness_init();
  prv_connect();
}

void test_responsiveness__cleanup(void) {
  struct ble_gap_event event = {
    .type = BLE_GAP_EVENT_DISCONNECT,
    .disconnect = {.conn = {.conn_handle = CONN_HANDLE}},
  };

  s_listener(&event, nullptr);
}

void test_responsiveness__nothing_is_asked_for_on_its_own(void) {
  prv_spend_seconds(60);
  cl_assert_equal_i(s_num_requests, 0);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MAX);
}

void test_responsiveness__waits_after_connecting(void) {
  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN, 30);
  cl_assert_equal_i(s_num_requests, 0);

  prv_spend_seconds(1);
  cl_assert_equal_i(s_num_requests, 1);
  cl_assert_equal_i(s_requested.itvl_max, FAST);
  cl_assert_equal_i(s_requested.latency, 0);
}

void test_responsiveness__fastest_wins_and_expires(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIDDLE, 30);
  cl_assert_equal_i(s_requested.latency, 4);

  /* The request with the central is answered before the next is made. */
  pbl_bt_responsiveness_request(CONN_HANDLE, &s_other_req, PBL_BT_RESPONSE_TIME_MIN, 10);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIN);
  cl_assert_equal_i(s_num_requests, 1);
  prv_update(0, SLOW, 4);
  cl_assert_equal_i(s_num_requests, 2);
  cl_assert_equal_i(s_requested.itvl_max, FAST);
  prv_update(0, FAST, 0);

  prv_spend_seconds(11);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIDDLE);
  cl_assert_equal_i(s_requested.latency, 4);
  prv_update(0, SLOW, 4);

  prv_spend_seconds(20);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MAX);
  cl_assert_equal_i(s_num_requests, 3);
}

void test_responsiveness__release_is_delayed(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  prv_update(0, FAST, 0);
  prv_spend_seconds(600);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIN);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MAX, 0);
  prv_spend_seconds(1);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIN);
  prv_spend_seconds(2);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MAX);
  cl_assert_equal_i(s_requested.latency, 4);
}

void test_responsiveness__gives_up_after_three_requests(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  prv_spend_seconds(60);
  cl_assert_equal_i(s_num_requests, 3);

  /* Asking for something else starts over. */
  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MAX, 0);
  prv_spend_seconds(3);
  cl_assert_equal_i(s_num_requests, 4);
}

void test_responsiveness__collisions_are_not_attempts(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  for (int i = 0; i < 5; i++) {
    cl_assert_equal_i(s_num_requests, i + 1);
    prv_update(BLE_HS_HCI_ERR(BLE_ERR_LMP_COLLISION), 0, 0);
    prv_spend_seconds(2);
  }

  prv_update(0, FAST, 0);
  prv_spend_seconds(60);
  cl_assert_equal_i(s_num_requests, 6);
}

void test_responsiveness__asks_again_when_the_central_changes_them(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  prv_update(0, FAST, 0);
  cl_assert_equal_i(s_num_requests, 1);

  prv_update(0, SLOW, 0);
  cl_assert_equal_i(s_num_requests, 2);
}

void test_responsiveness__no_connection(void) {
  pbl_bt_responsiveness_request(2, &s_req, PBL_BT_RESPONSE_TIME_MIN, 30);
  pbl_bt_responsiveness_request(BLE_HS_CONN_HANDLE_NONE, &s_req, PBL_BT_RESPONSE_TIME_MIN, 30);
  prv_spend_seconds(5);
  cl_assert_equal_i(s_num_requests, 0);
}

void test_responsiveness__requests_end_with_the_connection(void) {
  prv_spend_seconds(2);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIN);

  test_responsiveness__cleanup();
  prv_connect();
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MAX);

  pbl_bt_responsiveness_request(CONN_HANDLE, &s_req, PBL_BT_RESPONSE_TIME_MIDDLE, 30);
  cl_assert_equal_i(pbl_bt_responsiveness_get(CONN_HANDLE), PBL_BT_RESPONSE_TIME_MIDDLE);
}
