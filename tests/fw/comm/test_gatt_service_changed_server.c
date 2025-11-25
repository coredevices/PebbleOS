/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_service_changed.h"
#include "comm/ble/gap_le_connection.h"
#include "bluetopia_interface.h"

#include "kernel/events.h"

#include "clar.h"

#include <btutil/bt_device.h>

extern void gatt_service_changed_server_init(void);

// Fakes
///////////////////////////////////////////////////////////

#include "fake_GAPAPI.h"
#include "fake_GATTAPI.h"
#include "fake_GATTAPI_test_vectors.h"
#include "fake_pbl_malloc.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_system_task.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_bluetopia_interface.h"
#include "stubs_bt_driver_gatt.h"
#include "stubs_bt_driver_gatt_client_discovery.h"
#include "stubs_bt_lock.h"
#include "stubs_events.h"
#include "stubs_gatt_client_subscriptions.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"

uint16_t gaps_get_starting_att_handle(void) {
  return 4;
}

BLEService gatt_client_att_handle_get_service(
      GAPLEConnection *connection, uint16_t att_handle, const GATTServiceNode **service_node_out) {
  return 0;
}

uint8_t gatt_client_copy_service_refs_by_discovery_generation(
                                    const BTDeviceInternal *device, BLEService services_out[],
                                    uint8_t num_services, uint8_t discovery_gen) {
  return 0;
}

void gatt_client_service_get_all_characteristics_and_descriptors(
                                     GAPLEConnection *connection, GATTService *service,
                                     BLECharacteristic *characteristic_hdls_out,
                                     BLEDescriptor *descriptor_hdls_out) {
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Helpers
///////////////////////////////////////////////////////////

static const BTDeviceInternal s_device = {
  .address = {
    .octets = {
      1, 2, 3, 4, 5, 6,
    },
  },
};

static uint32_t s_connection_id = 1;

static GAPLEConnection *s_connection;

static void prv_cccd_write(bool is_subscribing) {
  GattServerSubscribeEvent event = {
    .connection_id = s_connection_id,
    .dev_address = s_device.address,
    .is_subscribing = is_subscribing,
  };
  bt_driver_cb_gatt_service_changed_server_subscribe(&event);
}

static void prv_process_pending_callbacks(GAPLEConnection *connection) {
  if (connection && connection->gatt_service_changed_indication_timer) {
    stub_new_timer_fire(connection->gatt_service_changed_indication_timer);
  }
  fake_system_task_callbacks_invoke_pending();
}

#define prv_expect_service_changed_indication_api_call_count(expected_count) \
{ \
  prv_process_pending_callbacks(s_connection); \
  cl_assert_equal_i(fake_gatt_get_service_changed_indication_count(), expected_count); \
}

// Tests
///////////////////////////////////////////////////////////


void test_gatt_service_changed_server__initialize(void) {
  gatt_service_changed_server_init();
  fake_gatt_init();
  gap_le_connection_init();
  gap_le_connection_add(&s_device, NULL, false /* local_is_master */);
  s_connection = gap_le_connection_by_device(&s_device);
  cl_assert(s_connection);
  s_connection->gatt_connection_id = s_connection_id;
}

void test_gatt_service_changed_server__cleanup(void) {
  if (s_connection) {
    gap_le_connection_remove(&s_device);
    s_connection = NULL;
  }
  gap_le_connection_deinit();
  stub_new_timer_cleanup();
}

void test_gatt_service_changed_server__unsubscribe(void) {
  prv_cccd_write(false /* is_subscribing */);

  prv_expect_service_changed_indication_api_call_count(0);
}

void test_gatt_service_changed_server__subscribe_event_but_no_connection(void) {
  gap_le_connection_remove(&s_device);
  s_connection = NULL;

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
  s_connection = NULL;

  static const int max_times = 5;

  for (int i = 0; i < max_times + 1; ++i) {
    gap_le_connection_add(&s_device, NULL, false /* local_is_master */);
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
    s_connection = NULL;
  }
}

void test_gatt_service_changed_server__disconnect_during_delay(void) {
  gatt_service_changed_server_handle_fw_update();
  prv_cccd_write(true /* is_subscribing */);

  // Grab the timer ID:
  TimerID t = s_connection->gatt_service_changed_indication_timer;
  s_connection->gatt_service_changed_indication_timer = TIMER_INVALID_ID;

  // Simulate disconnection:
  gap_le_connection_remove(&s_device);
  s_connection = NULL;

  // Timer fires:
  stub_new_timer_fire(t);

  prv_expect_service_changed_indication_api_call_count(0);
}

