/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/pp/transport.h>
#include <pbl/pp/transports/gatt.h>

#include <clar.h>
#include <host/ble_gap.h>
#include <pp/transports/gatt/ppogatt_internal.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_gatt_client_operations.h>
#include <fake_gatt_client_subscriptions.h>
#include <fake_pbl_malloc.h>
#include <fake_session.h>
#include <fake_sys_work.h>
#include <fake_work.h>

#define CONN_HANDLE (1)

static pbl_bt_characteristic_t s_characteristics[PBL_PP_GATT_CHARACTERISTIC_NUM] = {
  [PBL_PP_GATT_CHARACTERISTIC_DATA] = 01,
  [PBL_PP_GATT_CHARACTERISTIC_META] = 02,
};
#define SERIAL_NUMBER "XXXXXXXXXXXX"

uint16_t ble_att_mtu(uint16_t conn_handle) {
  return 158;
}

int ble_gap_terminate(uint16_t conn_handle, uint8_t hci_reason) {
  return 0;
}

int ble_gap_conn_find(uint16_t handle, struct ble_gap_conn_desc *out_desc) {
  if (out_desc != nullptr) {
    *out_desc = (struct ble_gap_conn_desc){.sec_state.encrypted = 1};
  }
  return 0;
}

const char *pbl_pp_gatt_cb_get_serial_number(void) {
  return SERIAL_NUMBER;
}

void pbl_pp_gatt_cb_set_responsiveness(uint16_t conn_handle, struct pbl_bt_responsiveness_req *req,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs) {
}

bool pbl_pp_main_is_current(void) {
  return true;
}

int pbl_bt_gattc_service_start(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                               const struct pbl_bt_gattc_service_cb *cb) {
  return 0;
}

int pbl_bt_gattc_service_rediscover(struct pbl_bt_gattc_service *svc) {
  return 0;
}

void pbl_bt_gattc_service_stop(struct pbl_bt_gattc_service *svc) {
}

int pbl_bt_gattc_service_subscribe(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable) {
  return 0;
}

int pbl_bt_gattc_service_read(struct pbl_bt_gattc_service *svc, uint8_t chr) {
  return (gatt_client_op_read(s_characteristics[chr]) == PBL_BT_ERRNO_OK) ? 0 : -EIO;
}

int pbl_bt_gattc_service_write_no_rsp(struct pbl_bt_gattc_service *svc, uint8_t chr,
                                      const void *data, uint16_t len) {
  const enum pbl_bt_errno e =
      gatt_client_op_write_without_response(s_characteristics[chr], data, len);
  return (e == PBL_BT_ERRNO_OK) ? 0 : -EIO;
}

bool pbl_bt_gattc_service_notify_rx(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                                    uint16_t attr_handle, struct os_mbuf *om) {
  return false;
}

enum pbl_bt_errno pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf,
                                              uint16_t len) {
  return PBL_BT_ERRNO_OK;
}

void pbl_pp_main_call(void (*callback)(void *data), void *data) {
  callback(data);
}

// Helpers
///////////////////////////////////////////////////////////

extern uint32_t ppogatt_client_count(void);
extern void ppogatt_reversed_handle_subscribed(uint16_t conn_handle);
extern void ppogatt_reversed_handle_unsubscribed(uint16_t conn_handle);
extern void ppogatt_reversed_handle_data(uint16_t conn_handle, const uint8_t *buf, size_t len);

static void prv_phone_sends_reversed_reset_request(void) {
  const PPoGATTPacket reset_request = {
    .type = PPoGATTPacketTypeResetRequest,
  };
  ppogatt_reversed_handle_data(CONN_HANDLE, (const uint8_t *)&reset_request, sizeof(reset_request));
}

static void prv_phone_sends_reversed_reset_complete(void) {
  uint8_t reset_complete[sizeof(PPoGATTPacket) + sizeof(PPoGATTResetCompleteClientIDPayloadV1)] =
      {};
  PPoGATTPacket *packet = (PPoGATTPacket *)reset_complete;
  packet->type = PPoGATTPacketTypeResetComplete;
  *(PPoGATTResetCompleteClientIDPayloadV1 *)packet->payload =
      (PPoGATTResetCompleteClientIDPayloadV1){
        .ppogatt_max_rx_window = 25,
        .ppogatt_max_tx_window = 25
  };
  ppogatt_reversed_handle_data(CONN_HANDLE, reset_complete, sizeof(reset_complete));
}

// Tests
///////////////////////////////////////////////////////////

void test_ppogatt_reversed__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  fake_gatt_client_op_init();
  fake_gatt_client_subscriptions_init();
  fake_comm_session_init();
  pbl_pp_gatt_create();
  pbl_pp_gatt_discover(CONN_HANDLE);
}

void test_ppogatt_reversed__cleanup(void) {
  pbl_pp_gatt_destroy();
  cl_assert_equal_i(ppogatt_client_count(), 0);
  fake_wallclock_work_cleanup();
  fake_gatt_client_op_deinit();
  fake_gatt_client_subscriptions_deinit();
  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
  fake_comm_session_cleanup();
  fake_work_cleanup();
}

// A bonded phone's subscription is restored when it reconnects, while the
// phone may host forward PPoG: the forward handshake must still run.
void test_ppogatt_reversed__subscription_does_not_block_forward(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  cl_assert_equal_i(ppogatt_client_count(), 1);

  pbl_pp_gatt_handle_service_discovered(0);
  fake_gatt_client_op_assert_read(s_characteristics[PBL_PP_GATT_CHARACTERISTIC_META]);
  cl_assert_equal_i(ppogatt_client_count(), 2);
}

// Once the phone starts the reversed handshake, reversed replaces forward.
void test_ppogatt_reversed__reset_request_takes_over_forward(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  pbl_pp_gatt_handle_service_discovered(0);
  cl_assert_equal_i(ppogatt_client_count(), 2);

  prv_phone_sends_reversed_reset_request();
  cl_assert_equal_i(ppogatt_client_count(), 1);

  // Forward PPoG yields to the reversed session from now on.
  pbl_pp_gatt_handle_service_discovered(0);
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__reset_request_without_forward(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  prv_phone_sends_reversed_reset_request();
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__unsubscribe_deletes_reversed(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  pbl_pp_gatt_handle_service_discovered(0);
  ppogatt_reversed_handle_unsubscribed(CONN_HANDLE);
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__invalidate_all_keeps_reversed(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  pbl_pp_gatt_handle_service_discovered(0);
  cl_assert_equal_i(ppogatt_client_count(), 2);

  pbl_pp_gatt_invalidate_all_references();
  cl_assert_equal_i(ppogatt_client_count(), 1);

  prv_phone_sends_reversed_reset_request();
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__invalidate_all_keeps_open_reversed_session(void) {
  ppogatt_reversed_handle_subscribed(CONN_HANDLE);
  prv_phone_sends_reversed_reset_request();
  prv_phone_sends_reversed_reset_complete();
  cl_assert_equal_i(fake_comm_session_open_call_count(), 1);

  pbl_pp_gatt_invalidate_all_references();
  pbl_pp_gatt_handle_service_discovered(0);
  cl_assert_equal_i(ppogatt_client_count(), 1);
  cl_assert_equal_i(fake_comm_session_close_call_count(), 0);
}
