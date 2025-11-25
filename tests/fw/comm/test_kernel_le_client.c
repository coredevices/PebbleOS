/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "comm/ble/gap_le_advert.h"
#include "comm/ble/gap_le_task.h"
#include "comm/ble/gatt_client_subscriptions.h"
#include "comm/ble/kernel_le_client/kernel_le_client.h"
#include "comm/ble/kernel_le_client/test/test_definition.h"
#include "kernel/events.h"
#include "util/size.h"

// Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "fake_system_task.h"

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_rand_ptr.h"
#include "stubs_rtc.h"

void ams_create(void) {
}

void ams_destroy(void) {
}

void ancs_create(void) {
}

void ancs_destroy(void) {
}

void app_launch_handle_disconnection(void) {
}

BTBondingID bt_persistent_storage_get_ble_ancs_bonding(void) {
  return 1;
}

bool bt_persistent_storage_is_ble_ancs_bonding(BTBondingID bonding) {
  return true;
}

void gap_le_advert_unschedule_job_types(GAPLEAdvertisingJobTag *tag_types, size_t num_types) {
}

void gap_le_connect_cancel_all(GAPLEClient client) {
}

BTErrno gap_le_connect_cancel_by_bonding(BTBondingID bonding_id, GAPLEClient client) {
  return BTErrnoOK;
}

BTErrno gap_le_connect_connect_by_bonding(BTBondingID bonding_id, bool auto_reconnect,
                                          bool is_pairing_required, GAPLEClient client) {
  return BTErrnoOK;
}

void gap_le_slave_reconnect_start(void) {
}

void gap_le_slave_reconnect_stop(void) {
}

BTErrno gatt_client_discovery_discover_all(const BTDeviceInternal *device) {
  return BTErrnoOK;
}

uint16_t gatt_client_subscriptions_consume_notification(BLECharacteristic *characteristic_ref_out,
                                                        uint8_t *value_out,
                                                        uint16_t *value_length_in_out,
                                                        GAPLEClient client, bool *has_more_out) {
  return 0;
}

bool gatt_client_subscriptions_get_notification_header(GAPLEClient client,
                                                       GATTBufferedNotificationHeader *header_out) {
  return false;
}

void gatt_client_subscriptions_reschedule(GAPLEClient c) {
}

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  // Use fake_system_task as mock:
  system_task_add_callback(callback, data);
}

void ppogatt_create(void) {
}

void ppogatt_destroy(void) {
}

void ppogatt_handle_buffer_empty(void) {
}

void bt_driver_reconnect_try_now(bool ignore_paused) {
}

void gatt_client_op_cleanup(GAPLEClient client) {
}

void ppogatt_reset_disconnect_counter(void) {
}

// Fakes & Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

static const BTDeviceInternal s_test_device = {
  .address = (const BTDeviceAddress) {
    .octets = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
  },
};

typedef enum {
  TestServiceInstanceComplete = 1,
  TestServiceInstanceIncomplete = 2,
  TestServiceInstanceUnsupported = 3,
} TestServiceInstance;

static BLEService s_service_handles[] = {
  TestServiceInstanceComplete,
  TestServiceInstanceIncomplete,
  TestServiceInstanceUnsupported,
};

typedef enum {
  TestCharacteristicInstanceCompleteOne = 11,
  TestCharacteristicInstanceCompleteTwo = 12,
  TestCharacteristicInstanceIncompleteOne = 21,
  TestCharacteristicInstanceUnsupported = 33,
} TestCharacteristicInstance;

Uuid gatt_client_service_get_uuid(BLEService service_ref) {
  switch (service_ref) {
    case TestServiceInstanceComplete:
    case TestServiceInstanceIncomplete:
      return s_test_service_uuid;

    case TestServiceInstanceUnsupported:
    default:
      return UUID_INVALID;
  }
}

uint8_t gatt_client_service_get_characteristics_matching_uuids(BLEService service_ref,
                                                         BLECharacteristic characteristics_out[],
                                                         const Uuid matching_characteristic_uuids[],
                                                         uint8_t num_characteristics) {
  cl_assert_equal_i(num_characteristics, TestCharacteristicCount);
  switch (service_ref) {
    case TestServiceInstanceComplete:
      characteristics_out[0] = TestCharacteristicInstanceCompleteOne;
      characteristics_out[1] = TestCharacteristicInstanceCompleteTwo;
      return 2;
    case TestServiceInstanceIncomplete:
      characteristics_out[0] = TestCharacteristicInstanceIncompleteOne;
      return 1;
    case TestCharacteristicInstanceUnsupported:
      characteristics_out[0] = TestCharacteristicInstanceUnsupported;
      return 1;
    default:
      return 0;
  }
}

static int s_read_responses_consumed_count;
void gatt_client_consume_read_response(uintptr_t object_ref,
                                       uint8_t value_out[],
                                       uint16_t value_length,
                                       GAPLEClient client) {
  ++s_read_responses_consumed_count;
}

static int s_services_discovered_count;
void test_client_handle_service_discovered(BLECharacteristic *characteristics) {
  ++s_services_discovered_count;
}

void test_client_invalidate_all_references(void) {

}

void test_client_handle_service_removed(BLECharacteristic *characteristics,
                                        uint8_t num_characteristics) {

}

static bool s_can_handle_characteristic;
bool test_client_can_handle_characteristic(BLECharacteristic characteristic) {
  return s_can_handle_characteristic;
}

void test_client_handle_write_response(BLECharacteristic characteristic, BLEGATTError error) {

}

void test_client_handle_subscribe(BLECharacteristic characteristic,
                                  BLESubscription subscription_type, BLEGATTError error) {

}

void test_client_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                             size_t value_length, BLEGATTError error) {

}


// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_kernel_le_client__initialize(void) {
  s_services_discovered_count = 0;
  s_read_responses_consumed_count = 0;
  s_can_handle_characteristic = false;
  kernel_le_client_init();
}

void test_kernel_le_client__cleanup(void) {
  kernel_le_client_deinit();
  fake_system_task_callbacks_cleanup();
}

void test_kernel_le_client__read_response_consumed_even_if_client_is_gone(void) {
  // Simulate the client goes away:
  s_can_handle_characteristic = false;

  PebbleEvent e = (PebbleEvent) {
    .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
    .bluetooth.le.gatt_client = {
      .object_ref = TestCharacteristicInstanceCompleteOne,
      .value_length = 1,
      .gatt_error = BLEGATTErrorSuccess,
      .subtype = PebbleBLEGATTClientEventTypeCharacteristicRead,
    },
  };

  kernel_le_client_handle_event(&e);

  cl_assert_equal_i(s_read_responses_consumed_count, 1);

  // When value_length is zero, the read response should NOT be consumed:
  e.bluetooth.le.gatt_client.value_length = 0;
  s_read_responses_consumed_count = 0;
  kernel_le_client_handle_event(&e);

  cl_assert_equal_i(s_read_responses_consumed_count, 0);
}

void test_kernel_le_client__service_added(void) {
  uint8_t num_services_added = ARRAY_LENGTH(s_service_handles);
  PebbleBLEGATTClientServiceEventInfo *info =
      kernel_malloc(sizeof(PebbleBLEGATTClientServiceEventInfo) +
                    (num_services_added * sizeof(BLEService)));

  *info = (PebbleBLEGATTClientServiceEventInfo) {
    .status = BTErrnoOK,
    .type = PebbleServicesAdded,
    .device = s_test_device,
  };
  info->services_added_data.num_services_added = num_services_added;
  memcpy(info->services_added_data.services, s_service_handles, sizeof(s_service_handles));

  PebbleEvent e = (PebbleEvent) {
    .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
    .bluetooth.le.gatt_client_service = {
      .info = info,
      .subtype = PebbleBLEGATTClientEventTypeServiceChange,
    },
  };

  kernel_le_client_handle_event(&e);

  // Found one complete service instance:
  cl_assert_equal_i(s_services_discovered_count, 1);

  kernel_free(info);
}

// FIXME: PBL-27751: Improve test coverage of kernel_le_client.c
