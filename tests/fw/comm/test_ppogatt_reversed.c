/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/comm_session/session_transport.h>
#include <pbl/services/regular_timer.h>

#include <clar.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/ble/gatt_client_operations.h>
#include <comm/ble/kernel_le_client/ppogatt/ppogatt.h>
#include <comm/ble/kernel_le_client/ppogatt/ppogatt_internal.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_gatt_client_operations.h>
#include <fake_gatt_client_subscriptions.h>
#include <fake_new_timer.h>
#include <fake_pbl_malloc.h>
#include <fake_session.h>
#include <fake_system_task.h>

#define CONN_HANDLE (1)

static GAPLEConnection s_connection = {
  .is_encrypted = true,
};
static struct pbl_bt_device_internal s_device = {};
static pbl_bt_characteristic_t s_write_characteristic;

int pbl_bt_gap_le_disconnect(const struct pbl_bt_device_internal *peer_address) {
  return 0;
}

uint16_t gap_le_connection_get_gatt_mtu(const struct pbl_bt_device_internal *device) {
  return 158;
}

GAPLEConnection *gap_le_connection_get_gateway(void) {
  return nullptr;
}

GAPLEConnection *gap_le_connection_by_device(const struct pbl_bt_device_internal *device) {
  return &s_connection;
}

GAPLEConnection *gatt_client_characteristic_get_connection(
    pbl_bt_characteristic_t characteristic_ref) {
  return &s_connection;
}

uint16_t gatt_client_characteristic_get_handle_and_connection(
    pbl_bt_characteristic_t characteristic_ref, GAPLEConnection **connection_out) {
  s_write_characteristic = characteristic_ref;
  *connection_out = &s_connection;
  return 1;
}

enum pbl_bt_errno pbl_bt_gatt_write_without_response(GAPLEConnection *connection,
                                                     const uint8_t *value, size_t value_length,
                                                     uint16_t att_handle) {
  return gatt_client_op_write_without_response(s_write_characteristic, value, value_length);
}

enum pbl_bt_errno pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf,
                                              uint16_t len) {
  return PBL_BT_ERRNO_OK;
}

enum pbl_bt_errno gatt_client_discovery_rediscover_all(
    const struct pbl_bt_device_internal *device) {
  return PBL_BT_ERRNO_OK;
}

struct pbl_bt_device_internal gatt_client_characteristic_get_device(
    pbl_bt_characteristic_t characteristic_ref) {
  return s_device;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Helpers
///////////////////////////////////////////////////////////

extern uint32_t ppogatt_client_count(void);
extern void ppogatt_reversed_handle_subscribed(const struct pbl_bt_device_internal *device,
                                               uint16_t conn_handle);
extern void ppogatt_reversed_handle_unsubscribed(uint16_t conn_handle);
extern void ppogatt_reversed_handle_data(uint16_t conn_handle, const uint8_t *buf, size_t len);

static pbl_bt_characteristic_t s_characteristics[PPoGATTCharacteristicNum] = {
  [PPoGATTCharacteristicData] = 01,
  [PPoGATTCharacteristicMeta] = 02,
};

static void prv_phone_sends_reversed_reset_request(void) {
  const PPoGATTPacket reset_request = {
    .type = PPoGATTPacketTypeResetRequest,
  };
  ppogatt_reversed_handle_data(CONN_HANDLE, (const uint8_t *)&reset_request, sizeof(reset_request));
}

// Tests
///////////////////////////////////////////////////////////

void test_ppogatt_reversed__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  fake_gatt_client_op_init();
  fake_gatt_client_subscriptions_init();
  regular_timer_init();
  fake_comm_session_init();
  ppogatt_create();
}

void test_ppogatt_reversed__cleanup(void) {
  ppogatt_destroy();
  cl_assert_equal_i(ppogatt_client_count(), 0);
  regular_timer_deinit();
  fake_gatt_client_op_deinit();
  fake_gatt_client_subscriptions_deinit();
  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
  fake_comm_session_cleanup();
}

// A bonded phone's subscription is restored when it reconnects, while the
// phone may host forward PPoG: the forward handshake must still run.
void test_ppogatt_reversed__subscription_does_not_block_forward(void) {
  ppogatt_reversed_handle_subscribed(&s_device, CONN_HANDLE);
  cl_assert_equal_i(ppogatt_client_count(), 1);

  ppogatt_handle_service_discovered(s_characteristics);
  fake_gatt_client_op_assert_read(s_characteristics[PPoGATTCharacteristicMeta]);
  cl_assert_equal_i(ppogatt_client_count(), 2);
}

// Once the phone starts the reversed handshake, reversed replaces forward.
void test_ppogatt_reversed__reset_request_takes_over_forward(void) {
  ppogatt_reversed_handle_subscribed(&s_device, CONN_HANDLE);
  ppogatt_handle_service_discovered(s_characteristics);
  cl_assert_equal_i(ppogatt_client_count(), 2);

  prv_phone_sends_reversed_reset_request();
  cl_assert_equal_i(ppogatt_client_count(), 1);

  // Forward PPoG yields to the reversed session from now on.
  ppogatt_handle_service_discovered(s_characteristics);
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__reset_request_without_forward(void) {
  ppogatt_reversed_handle_subscribed(&s_device, CONN_HANDLE);
  prv_phone_sends_reversed_reset_request();
  cl_assert_equal_i(ppogatt_client_count(), 1);
}

void test_ppogatt_reversed__unsubscribe_deletes_reversed(void) {
  ppogatt_reversed_handle_subscribed(&s_device, CONN_HANDLE);
  ppogatt_handle_service_discovered(s_characteristics);
  ppogatt_reversed_handle_unsubscribed(CONN_HANDLE);
  cl_assert_equal_i(ppogatt_client_count(), 1);
}
