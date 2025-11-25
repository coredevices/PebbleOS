/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_client_discovery.h"
#include "comm/ble/gatt_service_changed.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gap_le_task.h"

#include "kernel/events.h"

#include "clar.h"

#include <btutil/bt_device.h>

// Fakes
///////////////////////////////////////////////////////////

#include "fake_GAPAPI.h"
#include "fake_GATTAPI.h"
#include "fake_GATTAPI_test_vectors.h"
#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_system_task.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_bluetopia_interface.h"
#include "stubs_bt_driver_gatt.h"
#include "stubs_bt_lock.h"
#include "stubs_gatt_client_subscriptions.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"

void core_dump_reset(bool is_forced) {
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

uint16_t gaps_get_starting_att_handle(void) {
  return 4;
}

// Helpers
///////////////////////////////////////////////////////////

extern TimerID bt_driver_gatt_get_watchdog_timer_id(void);

#define TEST_GATT_CONNECTION_ID (1234)

static BTDeviceInternal prv_dummy_device(uint8_t octet) {
  BTDeviceAddress address = {
    .octets = {
      [0] = octet,
      [1] = octet,
      [2] = octet,
      [3] = octet,
      [4] = octet,
      [5] = octet,
    },
  };
  BTDevice device = bt_device_init_with_address(address, true /* is_random */);
  return *(BTDeviceInternal *)(&device);
}

static BTDeviceInternal prv_connected_dummy_device(uint8_t octet) {
  BTDeviceInternal device = prv_dummy_device(octet);
  gap_le_connection_add(&device, NULL, true /* local_is_master */);
  GAPLEConnection *connection = gap_le_connection_by_device(&device);
  connection->gatt_connection_id = TEST_GATT_CONNECTION_ID;
  return device;
}

static void prv_assert_no_event(void) {
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_NULL_EVENT);
}

static void prv_assert_event(const BTDeviceInternal *device, BTErrno status) {
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_BLE_GATT_CLIENT_EVENT);
  cl_assert_equal_i(event.bluetooth.le.gatt_client_service.subtype,
                    PebbleBLEGATTClientEventTypeServiceChange);
  cl_assert_equal_i(event.bluetooth.le.gatt_client_service.info->status,
                    status);
  const BTDeviceInternal event_device = event.bluetooth.le.gatt_client_service.info->device;
  const bool equal_devices = bt_device_equal(&device->opaque,
                                 &event_device.opaque);
  cl_assert_equal_b(equal_devices, true);

  // clear the event
  fake_event_clear_last();
  fake_event_reset_count();
}

static void prv_simulate_and_assert_discovery_of_one_service(const BTDeviceInternal *device) {
  // Simulate discovery of Blood Pressure service:
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  prv_assert_event(device, BTErrnoOK);
}

// Tests
///////////////////////////////////////////////////////////

void test_gatt_client_discovery__initialize(void) {
  fake_gatt_init();
  fake_event_init();
  gap_le_connection_init();
}

void test_gatt_client_discovery__cleanup(void) {
  gap_le_connection_deinit();

  // make sure we haven't leaked any memory!
  fake_pbl_malloc_check_net_allocs();
}

// -----------------------------------------------------------------------------
// Edge cases

void test_gatt_client_discovery__not_connected(void) {
  BTDeviceInternal device = prv_dummy_device(1);
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device),
                    BTErrnoInvalidParameter);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);
}

void test_gatt_client_discovery__already_in_progress(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery for device:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), true);

  // Start again (expect to fail):
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device),
                    BTErrnoInvalidState);

  // take down the connection and a disconnection event should be emitted
  gap_le_connection_remove(&device);
  prv_assert_event(&device, BTErrnoServiceDiscoveryDisconnected);
}

void test_gatt_client_discovery__event_is_sent_when_already_discovered(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), true);

  // Simulate discovery of 1 service:
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);
  prv_assert_event(&device, BTErrnoOK);

  fake_event_clear_last();

  // Start discovery again, expect not to run (already discovered):
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);

  // Expect event:
  prv_assert_event(&device, BTErrnoOK);
}

void test_gatt_client_discovery__disconnected_during_discovery(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  // Simulate disconnection:
  gap_le_connection_remove(&device);
  // Process racing discovery indication:
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  // Bluetopia's GATT module does *NOT* emit a service discovery completion event for disconnections

  // Test that our API *does* emit an service discovery event with "disconnected" reason:
  prv_assert_event(&device, BTErrnoServiceDiscoveryDisconnected);
}

void test_gatt_client_discovery__complete_error(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  // Simulate getting one service indication...
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  // ... then a failure:
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_RESPONSE_TIMEOUT,
                                         TEST_GATT_CONNECTION_ID);
  // Expect event with error status and 0 services:
  prv_assert_event(&device,
                   BTErrnoWithBluetopiaError(GATT_SERVICE_DISCOVERY_STATUS_RESPONSE_TIMEOUT));
  // Expect service discovery to be stopped:
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);
}

// -------------------------------------------------------------------------------------------------
// Watchdog timeout tests

static void prv_fire_watchdog_timeouts(const BTDeviceInternal *device, int retries) {
  for (int i = 0; i <= retries; ++i) {
    const int start_count = fake_gatt_is_service_discovery_start_count();
    const int stop_count = fake_gatt_is_service_discovery_stop_count();

    // Fire the watchdog timer:
    const TimerID watchdog_timer = bt_driver_gatt_get_watchdog_timer_id();
    stub_new_timer_fire(watchdog_timer);

    // Check whether GATT_Stop_Service_Discovery has been called:
    cl_assert_equal_i(stop_count + 1, fake_gatt_is_service_discovery_stop_count());

    if (i < GATT_CLIENT_DISCOVERY_MAX_RETRY) {
      // Check whether GATT_Start_Service_Discovery has been called, except for the last iteration:
      cl_assert_equal_i(start_count + 1, fake_gatt_is_service_discovery_start_count());
      // No client event:
      prv_assert_no_event();
    } else {
      // Last iteration: expect event with error status and 0 services:
      prv_assert_event(device, BTErrnoServiceDiscoveryTimeout);
    }
  }
}

void test_gatt_client_discovery__watchdog_error_out_after_max_retries(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_fire_watchdog_timeouts(&device, GATT_CLIENT_DISCOVERY_MAX_RETRY);
}

void test_gatt_client_discovery__watchdog_retry_counter_not_affecting_successive_process(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_fire_watchdog_timeouts(&device, GATT_CLIENT_DISCOVERY_MAX_RETRY);

  // Make sure the previous retry counter doesn't affect any new discovery process:
  fake_event_clear_last();
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);

  // Fire watchdog one less time than the maximum:
  prv_fire_watchdog_timeouts(&device, GATT_CLIENT_DISCOVERY_MAX_RETRY - 1);

  // Finally make the Bluetopia discovery events come in:
  prv_simulate_and_assert_discovery_of_one_service(&device);
}

void test_gatt_client_discovery__watchdog_race_with_stopping(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);

  // Make Bluetopia's GATT_Stop_Service_Discovery fail:
  // (Service discovery has finished in the mean time, disconnected, ...)
  fake_gatt_set_stop_return_value(BTGATT_ERROR_INVALID_PARAMETER);

  // Fire the watchdog timer:
  const TimerID watchdog_timer = bt_driver_gatt_get_watchdog_timer_id();
  stub_new_timer_fire(watchdog_timer);

  // No event should be generated, because the finishing / disconnecting / ... should cause
  // Bluetopia to call back to prv_handle_bluetopia_discovery_event() and therefore the normal
  // path will be taken.
  prv_assert_no_event();

  // take down the connection and a disconnection event should be emitted
  gap_le_connection_remove(&device);
  prv_assert_event(&device, BTErrnoServiceDiscoveryDisconnected);
}

void test_gatt_client_discovery__watchdog_race_with_restarting(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);

  // Make Bluetopia's GATT_Start_Service_Discovery fail:
  // (Disconnected in the mean time, ...)
  fake_gatt_set_start_return_value(BTGATT_ERROR_INVALID_PARAMETER);

  // Fire the watchdog timer:
  const TimerID watchdog_timer = bt_driver_gatt_get_watchdog_timer_id();
  stub_new_timer_fire(watchdog_timer);

  // Stopping did not fail, but restarting did. In this case we need to generate an event that
  // the discovery process failed. The error from GATT_Start_Service_Discovery is expected to be
  // passed in the event.
  prv_assert_event(&device, BTErrnoWithBluetopiaError(BTGATT_ERROR_INVALID_PARAMETER));
}

// -------------------------------------------------------------------------------------------------
// Re-discovery

extern BTErrno gatt_client_discovery_rediscover_all(const BTDeviceInternal *device);

void test_gatt_client_discovery__rediscover_not_already_running(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_simulate_and_assert_discovery_of_one_service(&device);

  GAPLEConnection *connection = gap_le_connection_by_gatt_id(TEST_GATT_CONNECTION_ID);
  // Expect one service, Blood Pressure:
  cl_assert_equal_i(list_count(&connection->gatt_remote_services->node), 1);

  fake_event_clear_last();

  // Re-discovery:
  cl_assert_equal_i(gatt_client_discovery_rediscover_all(&device), BTErrnoOK);

  // Expect "Database Changed" event:
  prv_assert_event(&device, BTErrnoServiceDiscoveryDatabaseChanged);
  // Expect all services nodes to be cleaned up:
  cl_assert_equal_i(list_count(&connection->gatt_remote_services->node), 0);

  // Put one, expect one:
  prv_simulate_and_assert_discovery_of_one_service(&device);
  cl_assert_equal_i(list_count(&connection->gatt_remote_services->node), 1);
}

void test_gatt_client_discovery__rediscover_already_running(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  // Put one service, but do not finish...
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  prv_assert_no_event();
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), true);
  const int stop_count_before_rediscovery = fake_gatt_is_service_discovery_stop_count();

  // Re-discovery:
  cl_assert_equal_i(gatt_client_discovery_rediscover_all(&device), BTErrnoOK);

  // Assert the previous process has been stopped:
  cl_assert_equal_i(stop_count_before_rediscovery + 1, fake_gatt_is_service_discovery_stop_count());

  // Expect "Database Changed" event:
  prv_assert_event(&device, BTErrnoServiceDiscoveryDatabaseChanged);

  // Put one, expect one:
  prv_simulate_and_assert_discovery_of_one_service(&device);
}

extern void gatt_client_discovery_discover_range(GAPLEConnection *connection,
                                                 ATTHandleRange *hdl_range);

void test_gatt_client_discovery__multiple_jobs_pending(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);
  GAPLEConnection *connection = gap_le_connection_by_device(&device);

  ATTHandleRange range = {
    .start = 0x1,
    .end = 0x3000
  };
  ATTHandleRange range_alt = {
    .start = 0x3001,
    .end = 0x4000
  };

  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);

  // start a discovery job, pretend nothing is found
  gatt_client_discovery_discover_range(connection, &range);
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), true);
  // pend up another service discovery job
  gatt_client_discovery_discover_range(connection, &range);
  cl_assert_equal_i(1, fake_gatt_is_service_discovery_start_count());

  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  // Nothing was found so we should just have a completion event
  cl_assert_equal_i(1, fake_event_get_count());

  prv_assert_event(&device, BTErrnoOK);
  cl_assert_equal_i(2, fake_gatt_is_service_discovery_start_count());

  // next job should be in progress
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), true);
  // BP service was discovered
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  // start another job, should be pended
  gatt_client_discovery_discover_range(connection, &range);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);

  // Since we are restarting discovery over the same range and discovered one
  // service two events should be generated, one about the discovery complete
  // and one invalidating the just discovered service
  cl_assert_equal_i(2, fake_event_get_count());
  prv_assert_event(&device, BTErrnoOK);
  cl_assert_equal_i(3, fake_gatt_is_service_discovery_start_count());

  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);

  gatt_client_discovery_discover_range(connection, &range_alt);

  // BP service should have been discovered
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  // Only one event should have been pended since we were not rediscovering the
  // same handle range
  cl_assert_equal_i(1, fake_event_get_count());
  prv_assert_event(&device, BTErrnoOK);

  // Nothing discovered for final query
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  prv_assert_event(&device, BTErrnoOK);
  
  cl_assert_equal_i(4, fake_gatt_is_service_discovery_start_count());
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);
}

void test_gatt_client_discovery__partial_and_full_discovery_jobs_intermixed(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);
  GAPLEConnection *connection = gap_le_connection_by_device(&device);

  // queue up a few jobs - note only one should be running at any time
  ATTHandleRange range = {
    .start = 0x1,
    .end = 0x3000
  };
  for (int i = 0; i < 10; i++) {
    gatt_client_discovery_discover_range(connection, &range);
  }

  // kick off a full discovery
  cl_assert_equal_i(gatt_client_discovery_rediscover_all(&device), BTErrnoOK);

  // Assert the previous process has been stopped:
  cl_assert_equal_i(1, fake_gatt_is_service_discovery_stop_count());

  // Expect "Database Changed" event:
  prv_assert_event(&device, BTErrnoServiceDiscoveryDatabaseChanged);

  prv_simulate_and_assert_discovery_of_one_service(&device);

  // None of the batched up jobs should have run
  cl_assert_equal_i(2, fake_gatt_is_service_discovery_start_count());

  // nothing should still be running
  cl_assert_equal_b(fake_gatt_is_service_discovery_running(), false);
}

// -------------------------------------------------------------------------------------------------
// Test vectors

static void prv_assert_blood_pressure_service(const GATTService *service) {
  const Service *bp_service = fake_gatt_get_blood_pressure_service();

  const uint16_t service_handle = bp_service->handle;
  cl_assert_equal_i(service->att_handle, service_handle);
  cl_assert_equal_b(uuid_equal(&service->uuid, &bp_service->uuid), true);
  cl_assert_equal_i(service->num_att_handles_included_services, bp_service->num_included_services);
  cl_assert_equal_i(service->num_characteristics, bp_service->num_characteristics);

  const GATTCharacteristic *characteristic_one = service->characteristics;
  const Characteristic *expected_characteristic1 = &bp_service->characteristics[0];
  cl_assert_equal_i(characteristic_one->att_handle_offset, expected_characteristic1->handle - service_handle);
  cl_assert_equal_i(characteristic_one->num_descriptors, expected_characteristic1->num_descriptors);
  cl_assert_equal_i(characteristic_one->descriptors[0].att_handle_offset,
                    expected_characteristic1->descriptors[0].handle - service_handle);
  cl_assert_equal_i(characteristic_one->properties, expected_characteristic1->properties);
  cl_assert_equal_b(uuid_equal(&characteristic_one->uuid, &expected_characteristic1->uuid), true);

  // Second characteristic is tacked right after the first one:
  const GATTCharacteristic *characteristic_two =
                  (const GATTCharacteristic *) &characteristic_one->descriptors[1];
  const Characteristic *expected_characteristic2 = &bp_service->characteristics[1];
  cl_assert_equal_i(characteristic_two->att_handle_offset, expected_characteristic2->handle - service_handle);
  cl_assert_equal_i(characteristic_two->num_descriptors, expected_characteristic2->num_descriptors);
  cl_assert_equal_i(characteristic_two->descriptors[0].att_handle_offset,
                    expected_characteristic2->descriptors[0].handle - service_handle);
  cl_assert_equal_i(characteristic_two->properties, expected_characteristic2->properties);
  cl_assert_equal_b(uuid_equal(&characteristic_two->uuid, &expected_characteristic2->uuid), true);
}

void test_gatt_client_discovery__single_blood_pressure_service(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);

  prv_simulate_and_assert_discovery_of_one_service(&device);

  GAPLEConnection *connection = gap_le_connection_by_gatt_id(TEST_GATT_CONNECTION_ID);
  // Expect one service, Blood Pressure:
  cl_assert_equal_i(list_count(&connection->gatt_remote_services->node), 1);
  const GATTService *service = connection->gatt_remote_services->service;
  prv_assert_blood_pressure_service(service);
}
