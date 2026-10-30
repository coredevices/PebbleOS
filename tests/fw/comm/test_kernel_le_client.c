/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/size.h>

#include <clar.h>
#include <comm/ble/gap_le_advert.h>
#include <comm/ble/gatt_client_subscriptions.h>
#include <comm/ble/kernel_le_client/kernel_le_client.h>
#include <comm/ble/kernel_le_client/test/test_definition.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>

// Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <fake_system_task.h>

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

static bool s_has_gateway_bonding;

void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data) {
  const struct pbl_bt_bond_info info = {};

  if (s_has_gateway_bonding) {
    func(&info, user_data);
  }
}

size_t pbl_bt_bond_count(void) {
  return s_has_gateway_bonding ? 1 : 0;
}

void gap_le_advert_unschedule_job_types(GAPLEAdvertisingJobTag *tag_types, size_t num_types) {
}

void gap_le_connect_cancel_all(void) {
}

enum pbl_bt_errno gap_le_connect_cancel_by_bond(const struct pbl_bt_device_internal *addr) {
  return PBL_BT_ERRNO_OK;
}

enum pbl_bt_errno gap_le_connect_connect_by_bond(const struct pbl_bt_device_internal *addr,
                                                 bool auto_reconnect, bool is_pairing_required) {
  return PBL_BT_ERRNO_OK;
}

void gap_le_slave_reconnect_start(void) {
}

static int s_reconnect_stop_count;

void gap_le_slave_reconnect_stop(void) {
  ++s_reconnect_stop_count;
}

enum pbl_bt_errno gatt_client_discovery_discover_all(const struct pbl_bt_device_internal *device) {
  return PBL_BT_ERRNO_OK;
}

static int s_pending_notifications;
static int s_reschedule_count;
static uint32_t s_kernel_events_free;

uint32_t event_kernel_to_kernel_num_free(void) {
  return s_kernel_events_free;
}

uint16_t gatt_client_subscriptions_consume_notification(
    pbl_bt_characteristic_t *characteristic_ref_out, uint8_t *value_out,
    uint16_t *value_length_in_out, bool *has_more_out) {
  --s_pending_notifications;
  *has_more_out = (s_pending_notifications > 0);
  return *has_more_out ? 1 : 0;
}

bool gatt_client_subscriptions_get_notification_header(GATTBufferedNotificationHeader *header_out) {
  if (s_pending_notifications == 0) {
    return false;
  }
  *header_out = (GATTBufferedNotificationHeader){.value_length = 1};
  return true;
}

void gatt_client_subscriptions_reschedule(void) {
  ++s_reschedule_count;
}

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  // Use fake_system_task as mock:
  system_task_add_callback(callback, data);
}

void ppogatt_create(void) {
}

void ppogatt_destroy(void) {
}

void gatt_client_op_cleanup(void) {
}

void ppogatt_reset_disconnect_counter(void) {
}

// Fakes & Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

static const struct pbl_bt_device_internal s_test_device = {
  .address = (const struct pbl_bt_addr){
    .octets = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
  },
};

typedef enum {
  TestServiceInstanceComplete = 1,
  TestServiceInstanceIncomplete = 2,
  TestServiceInstanceUnsupported = 3,
} TestServiceInstance;

static pbl_bt_service_t s_service_handles[] = {
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

Uuid gatt_client_service_get_uuid(pbl_bt_service_t service_ref) {
  switch (service_ref) {
    case TestServiceInstanceComplete:
    case TestServiceInstanceIncomplete:
      return s_test_service_uuid;

    case TestServiceInstanceUnsupported:
    default:
      return UUID_INVALID;
  }
}

uint8_t gatt_client_service_get_characteristics_matching_uuids(
    pbl_bt_service_t service_ref, pbl_bt_characteristic_t characteristics_out[],
    const Uuid matching_characteristic_uuids[], uint8_t num_characteristics) {
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
bool gatt_client_consume_read_response(uintptr_t object_ref, uint8_t value_out[],
                                       uint16_t value_length) {
  ++s_read_responses_consumed_count;
  return true;
}

static int s_services_discovered_count;
void test_client_handle_service_discovered(pbl_bt_characteristic_t *characteristics) {
  ++s_services_discovered_count;
}

void test_client_invalidate_all_references(void) {
}

void test_client_handle_service_removed(pbl_bt_characteristic_t *characteristics,
                                        uint8_t num_characteristics) {
}

static bool s_can_handle_characteristic;
bool test_client_can_handle_characteristic(pbl_bt_characteristic_t characteristic) {
  return s_can_handle_characteristic;
}

void test_client_handle_write_response(pbl_bt_characteristic_t characteristic,
                                       enum pbl_bt_gatt_error error) {
}

void test_client_handle_subscribe(pbl_bt_characteristic_t characteristic,
                                  BLESubscription subscription_type, enum pbl_bt_gatt_error error) {
}

static int s_notifications_handled;
void test_client_handle_read_or_notification(pbl_bt_characteristic_t characteristic,
                                             const uint8_t *value, size_t value_length,
                                             enum pbl_bt_gatt_error error) {
  ++s_notifications_handled;
  --s_kernel_events_free;
}

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_kernel_le_client__initialize(void) {
  s_services_discovered_count = 0;
  s_read_responses_consumed_count = 0;
  s_can_handle_characteristic = false;
  s_pending_notifications = 0;
  s_reschedule_count = 0;
  s_kernel_events_free = 14;
  s_notifications_handled = 0;
  s_has_gateway_bonding = true;
  kernel_le_client_init();
  s_reconnect_stop_count = 0;
}

void test_kernel_le_client__cleanup(void) {
  kernel_le_client_deinit();
  fake_system_task_callbacks_cleanup();
}

void test_kernel_le_client__read_response_consumed_even_if_client_is_gone(void) {
  // Simulate the client goes away:
  s_can_handle_characteristic = false;

  PebbleEvent e = (PebbleEvent){
    .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
    .bluetooth.le.gatt_client = {
      .object_ref = TestCharacteristicInstanceCompleteOne,
      .value_length = 1,
      .gatt_error = PBL_BT_GATT_ERROR_SUCCESS,
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

void test_kernel_le_client__notifications_yield_before_kernel_queue_fills(void) {
  s_can_handle_characteristic = true;
  s_pending_notifications = 50;

  PebbleEvent e = (PebbleEvent){
    .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
    .bluetooth.le.gatt_client.subtype = PebbleBLEGATTClientEventTypeNotification,
  };
  kernel_le_client_handle_event(&e);

  cl_assert_equal_i(s_notifications_handled, 11);
  cl_assert_equal_i(s_reschedule_count, 1);
  cl_assert_equal_i(s_pending_notifications, 39);

  // KernelMain drained its queue before taking the rescheduled event:
  s_kernel_events_free = 14;
  s_notifications_handled = 0;
  kernel_le_client_handle_event(&e);
  cl_assert_equal_i(s_notifications_handled, 11);
  cl_assert_equal_i(s_reschedule_count, 2);
}

void test_kernel_le_client__notifications_drained_with_room_in_kernel_queue(void) {
  s_can_handle_characteristic = true;
  s_pending_notifications = 5;

  PebbleEvent e = (PebbleEvent){
    .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
    .bluetooth.le.gatt_client.subtype = PebbleBLEGATTClientEventTypeNotification,
  };
  kernel_le_client_handle_event(&e);

  cl_assert_equal_i(s_notifications_handled, 5);
  cl_assert_equal_i(s_pending_notifications, 0);
  cl_assert_equal_i(s_reschedule_count, 0);
}

void test_kernel_le_client__service_added(void) {
  uint8_t num_services_added = ARRAY_LENGTH(s_service_handles);
  PebbleBLEGATTClientServiceEventInfo *info =
      kernel_malloc(sizeof(PebbleBLEGATTClientServiceEventInfo) +
                    (num_services_added * sizeof(pbl_bt_service_t)));

  *info = (PebbleBLEGATTClientServiceEventInfo){
    .status = PBL_BT_ERRNO_OK,
    .type = PebbleServicesAdded,
    .device = s_test_device,
  };
  info->services_added_data.num_services_added = num_services_added;
  memcpy(info->services_added_data.services, s_service_handles, sizeof(s_service_handles));

  PebbleEvent e = (PebbleEvent){
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

void test_kernel_le_client__deleting_last_bond_stops_reconnecting(void) {
  const struct pbl_bt_device_internal addr = {};

  s_has_gateway_bonding = false;
  kernel_le_client_handle_bond_deleted(&addr);
  cl_assert_equal_i(s_reconnect_stop_count, 1);
}

void test_kernel_le_client__deleting_other_bond_keeps_reconnecting(void) {
  const struct pbl_bt_device_internal addr = {};

  kernel_le_client_handle_bond_deleted(&addr);
  cl_assert_equal_i(s_reconnect_stop_count, 0);
}
