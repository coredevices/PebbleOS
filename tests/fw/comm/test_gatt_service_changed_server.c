/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>
#include <pbl/btutil/bt_device.h>

#include <clar.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/ble/gatt_service_changed.h>
#include <kernel/events.h>

extern void gatt_service_changed_server_init(void);

// Fakes
///////////////////////////////////////////////////////////

#include <fake_bt_gatt.h>
#include <fake_pbl_malloc.h>
#include <fake_rtc.h>
#include <fake_sys_work.h>
#include <fake_work.h>

// Stubs
///////////////////////////////////////////////////////////

uint16_t gaps_get_starting_att_handle(void) {
  return 4;
}

pbl_bt_service_t gatt_client_att_handle_get_service(GAPLEConnection *connection,
                                                    uint16_t att_handle,
                                                    const GATTServiceNode **service_node_out) {
  return 0;
}

uint8_t gatt_client_copy_service_refs_by_discovery_generation(
    const struct pbl_bt_device_internal *device, pbl_bt_service_t services_out[],
    uint8_t num_services, uint8_t discovery_gen) {
  return 0;
}

void gatt_client_service_get_all_characteristics_and_descriptors(
    GAPLEConnection *connection, struct pbl_bt_gatt_service *service,
    pbl_bt_characteristic_t *characteristic_hdls_out, pbl_bt_descriptor_t *descriptor_hdls_out) {
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Helpers
///////////////////////////////////////////////////////////

static const struct pbl_bt_device_internal s_device = {
  .address = {
    .octets = {
      1,
      2,
      3,
      4,
      5,
      6,
    },
  },
};

static uint32_t s_connection_id = 1;

static GAPLEConnection *s_connection;

static void prv_cccd_write(bool is_subscribing) {
  struct pbl_bt_gatt_server_subscribe_event event = {
    .connection_id = s_connection_id,
    .dev_address = s_device.address,
    .is_subscribing = is_subscribing,
  };
  pbl_bt_cb_gatt_service_changed_server_subscribe(&event);
}

static void prv_process_pending_callbacks(GAPLEConnection *connection) {
  if (connection && fake_work_is_scheduled(&connection->gatt_service_changed_indication_work)) {
    fake_work_fire(&connection->gatt_service_changed_indication_work);
  }
  fake_sys_work_callbacks_invoke_pending();
}

#define prv_expect_service_changed_indication_api_call_count(expected_count)             \
  {                                                                                      \
    prv_process_pending_callbacks(s_connection);                                         \
    cl_assert_equal_i(fake_gatt_get_service_changed_indication_count(), expected_count); \
  }

// Tests
///////////////////////////////////////////////////////////

void test_gatt_service_changed_server__initialize(void) {
  gatt_service_changed_server_init();
  fake_gatt_init();
  gap_le_connection_init();
  gap_le_connection_add(&s_device, nullptr, false /* local_is_master */);
  s_connection = gap_le_connection_by_device(&s_device);
  cl_assert(s_connection);
  s_connection->gatt_connection_id = s_connection_id;
}

void test_gatt_service_changed_server__cleanup(void) {
  if (s_connection) {
    gap_le_connection_remove(&s_device);
    s_connection = nullptr;
  }
  gap_le_connection_deinit();
  fake_work_cleanup();
}

void test_gatt_service_changed_server__unsubscribe(void) {
  prv_cccd_write(false /* is_subscribing */);

  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__subscribe_event_but_no_connection(void) {
  gap_le_connection_remove(&s_device);
  s_connection = nullptr;

  prv_cccd_write(true /* is_subscribing */);

  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__subscribe_fw_not_updated(void) {
  prv_cccd_write(true /* is_subscribing */);
  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__resubscribe_indication_already_pending(void) {
  gatt_service_changed_server_handle_fw_update();

  prv_cccd_write(true /* is_subscribing */);
  prv_cccd_write(true /* is_subscribing */);

  prv_expect_service_changed_indication_api_call_count(1);
}

void test_gatt_service_changed_server__reconnect_resubscribe_stop_sending_after_n_times(void) {
  gatt_service_changed_server_handle_fw_update();

  gap_le_connection_remove(&s_device);
  s_connection = nullptr;

  static const int max_times = 5;

  for (int i = 0; i < max_times + 1; ++i) {
    gap_le_connection_add(&s_device, nullptr, false /* local_is_master */);
    s_connection = gap_le_connection_by_device(&s_device);
    cl_assert(s_connection);
    s_connection->gatt_connection_id = s_connection_id;

    prv_cccd_write(true /* is_subscribing */);
    if (i < max_times) {
      prv_expect_service_changed_indication_api_call_count(i + 1);
    } else {
      prv_expect_service_changed_indication_api_call_count(max_times);
    }

    gap_le_connection_remove(&s_device);
    s_connection = nullptr;
  }
}

void test_gatt_service_changed_server__disconnect_during_delay(void) {
  gatt_service_changed_server_handle_fw_update();
  prv_cccd_write(true /* is_subscribing */);

  cl_assert(fake_work_get_next() != nullptr);

  // Simulate disconnection:
  gap_le_connection_remove(&s_device);
  s_connection = nullptr;

  // The delay must not outlive the connection:
  cl_assert(fake_work_get_next() == nullptr);

  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__disconnect_after_delay(void) {
  gatt_service_changed_server_handle_fw_update();
  prv_cccd_write(true /* is_subscribing */);

  // The delay passes, which queues the indication on the background work queue:
  fake_work_fire(&s_connection->gatt_service_changed_indication_work);

  // Simulate disconnection before that runs:
  gap_le_connection_remove(&s_device);
  s_connection = nullptr;

  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__indication_carries_connection_and_full_range(void) {
  gatt_service_changed_server_handle_fw_update();
  prv_cccd_write(true /* is_subscribing */);

  // Fire the delayed indication and let the system-task callback run.
  prv_expect_service_changed_indication_api_call_count(1);

  // The service layer must hand the driver the connection currently subscribed,
  // and a "rediscover everything" range (0x0001 - 0xFFFF) so the remote cache is
  // fully invalidated after the firmware update.
  cl_assert(
      bt_device_internal_equal(fake_gatt_get_service_changed_last_device(), &s_connection->device));
  const struct pbl_bt_att_handle_range range = fake_gatt_get_service_changed_last_range();
  cl_assert_equal_i(range.start, 0x0001);
  cl_assert_equal_i(range.end, 0xFFFF);
}
