/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/gatt.h>

#include "comm/ble/gatt_client_accessors.h"
#include "comm/ble/gatt_client_discovery.h"
#include "comm/ble/gatt_service_changed.h"
#include "comm/ble/gap_le_connection.h"

#include "kernel/events.h"

#include "clar.h"

#include <btutil/bt_device.h>
#include <btutil/bt_uuid.h>

// Fakes
///////////////////////////////////////////////////////////

#include "fake_GAPAPI.h"
#include "fake_GATTAPI.h"
#include "fake_GATTAPI_test_vectors.h"
#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_system_task.h"
#include "fake_pbl_malloc.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_bluetopia_interface.h"
#include "stubs_bt_driver_gatt.h"
#include "stubs_bt_lock.h"
#include "stubs_gatt_client_subscriptions.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
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

static void prv_mock_put_service_discovery_events(void) {
  // Simulate discovery of Blood Pressure service:
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_indication_health_thermometer_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_indication_random_128bit_uuid_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
}

// Tests
///////////////////////////////////////////////////////////

void test_gatt_client_accessors__initialize(void) {
  fake_gatt_init();
  fake_event_init();
  gap_le_connection_init();
}

void test_gatt_client_accessors__cleanup(void) {
  gap_le_connection_deinit();
}

void test_gatt_client_accessors__copy_service_refs(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_mock_put_service_discovery_events();

  const Service *bp_service = fake_gatt_get_blood_pressure_service();
  const Service *thermo_service = fake_gatt_get_health_thermometer_service();
  const Service *random_128bit_service = fake_gatt_get_random_128bit_uuid_service();
  const Service *services[] = {
    bp_service,
    thermo_service,
    random_128bit_service,
  };
  const uint8_t num_services = 3;

  // Test gatt_client_copy_service_refs():
  BLEService service_refs[num_services];
  uint8_t num_found_services = gatt_client_copy_service_refs(&device, service_refs, num_services);
  cl_assert_equal_i(num_found_services, num_services);

  for (uint8_t s = 0; s < num_found_services; ++s) {
    BLEService service_ref = service_refs[s];
    const Service *expected_service = services[s];

    // Test gatt_client_service_get_uuid():
    const Uuid uuid = gatt_client_service_get_uuid(service_ref);
    cl_assert(uuid_equal(&uuid, &expected_service->uuid));

    // Test gatt_client_service_get_device():
    BTDeviceInternal returned_device = gatt_client_service_get_device(service_ref);
    cl_assert(bt_device_equal(&returned_device.opaque, &device.opaque));

    // Check Characteristics:
    const uint8_t num_characteristics = expected_service->num_characteristics;
    BLECharacteristic characteristic_refs[num_characteristics];
    // Test gatt_client_service_get_characteristics():
    const uint8_t num_found_characteristics =
                        gatt_client_service_get_characteristics(service_ref, characteristic_refs, num_characteristics);
    cl_assert_equal_i(num_characteristics, num_found_characteristics);

    for (uint8_t c = 0; c < num_found_characteristics; ++c) {
      BLECharacteristic characteristic_ref = characteristic_refs[c];
      const Characteristic *expected_characteristic = &expected_service->characteristics[c];

      // Test gatt_client_characteristic_get_uuid():
      const Uuid uuid = gatt_client_characteristic_get_uuid(characteristic_ref);
      cl_assert(uuid_equal(&uuid, &expected_characteristic->uuid));

      // Test gatt_client_characteristic_get_properties():
      cl_assert_equal_i(gatt_client_characteristic_get_properties(characteristic_ref),
                        expected_characteristic->properties);

      // Test gatt_client_characteristic_get_service():
      cl_assert_equal_i(gatt_client_characteristic_get_service(characteristic_ref), service_ref);

      // Test gatt_client_characteristic_get_device():
      BTDeviceInternal returned_device = gatt_client_characteristic_get_device(characteristic_ref);
      cl_assert(bt_device_equal(&returned_device.opaque, &device.opaque));

      // Test gatt_client_characteristic_get_descriptors():
      const uint8_t num_descriptors = expected_characteristic->num_descriptors;
      BLEDescriptor descriptor_refs[num_descriptors];
      const uint8_t num_found_descriptors =
                           gatt_client_characteristic_get_descriptors(characteristic_ref, descriptor_refs, num_descriptors);
      cl_assert_equal_i(num_descriptors, num_found_descriptors);

      for (uint8_t d = 0; d < num_descriptors; ++d) {
        const Descriptor *expected_descriptor = &expected_characteristic->descriptors[d];
        const BLEDescriptor descriptor_ref = descriptor_refs[d];

        // Test gatt_client_descriptor_get_uuid():
        const Uuid uuid = gatt_client_descriptor_get_uuid(descriptor_ref);
        cl_assert(uuid_equal(&uuid, &expected_descriptor->uuid));

        // Test gatt_client_descriptor_get_characteristic():
        cl_assert_equal_i(gatt_client_descriptor_get_characteristic(descriptor_ref), characteristic_ref);
      }
    }

    // Test gatt_client_service_get_included_services():
    const uint8_t num_inc_services = expected_service->num_included_services;
    BLEService inc_service_refs[num_inc_services];
    const uint8_t num_found_included_services =
                       gatt_client_service_get_included_services(service_ref, inc_service_refs, num_inc_services);
    cl_assert_equal_i(num_inc_services, num_found_included_services);

    for (uint8_t i = 0; i < num_inc_services; ++i) {
      const Service *expected_inc_service = expected_service->included_services[i];
      BLEService inc_service = inc_service_refs[i];

      // Only check the Service UUID:
      const Uuid uuid = gatt_client_service_get_uuid(inc_service);
      cl_assert(uuid_equal(&uuid, &expected_inc_service->uuid));
    }
  }
}

void test_gatt_client_accessors__copy_service_refs_matching(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_mock_put_service_discovery_events();

  const uint8_t num_services = 1;

  // Test gatt_client_copy_service_refs():
  BLEService service_refs[num_services];
  const Service *bp_service = fake_gatt_get_blood_pressure_service();
  uint8_t num_found_services = gatt_client_copy_service_refs_matching_uuid(&device, service_refs,
                                                                           num_services,
                                                                           &bp_service->uuid);
  cl_assert_equal_i(num_found_services, num_services);

  // Test that the UUID matches the Blood Pressure UUID:
  const Uuid uuid = gatt_client_service_get_uuid(service_refs[0]);
  cl_assert(uuid_equal(&uuid, &bp_service->uuid));
}

void test_gatt_client_accessors__get_characteristics_matching_uuids(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  // Start discovery:
  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  prv_mock_put_service_discovery_events();

  const Service *bp_service = fake_gatt_get_blood_pressure_service();

  // Get the reference to the Blood Pressure service:
  const uint8_t num_services = 1;
  BLEService service_refs[num_services];
  gatt_client_copy_service_refs_matching_uuid(&device, service_refs,
                                              num_services, &bp_service->uuid);

  Uuid matching_uuids[3];
  matching_uuids[0] = bp_service->characteristics[1].uuid;
  matching_uuids[1] = bt_uuid_expand_16bit(0xffff); // not expected to match
  matching_uuids[2] = bp_service->characteristics[0].uuid;

  BLECharacteristic characteristics[3];
  const uint8_t found =
        gatt_client_service_get_characteristics_matching_uuids(service_refs[0], characteristics,
                                                               matching_uuids, 3);
  cl_assert_equal_i(found, 2);

  // Expect the order of the matching_uuids array is preserved:
  cl_assert(uuid_equal(&matching_uuids[0], &bp_service->characteristics[1].uuid));
  cl_assert(uuid_equal(&matching_uuids[2], &bp_service->characteristics[0].uuid));

  // Expect the 0xffff UUID to return "no match":
  cl_assert_equal_i(characteristics[1], BLE_CHARACTERISTIC_INVALID);
}

extern uint8_t gatt_client_copy_service_refs_by_discovery_generation(
    const BTDeviceInternal *device, BLEService services_out[],
    uint8_t num_services, uint8_t discovery_gen);
extern void gatt_client_discovery_discover_range(GAPLEConnection *connection,
                                                 ATTHandleRange *hdl_range);

void test_gatt_client_accessors__get_service_refs_by_discovery_gen(void) {
  BTDeviceInternal device = prv_connected_dummy_device(1);

  cl_assert_equal_i(gatt_client_discovery_discover_all(&device), BTErrnoOK);
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS, TEST_GATT_CONNECTION_ID);

  ATTHandleRange range = {
    .start = 0x1,
    .end = 0xC000,
  };

  gatt_client_discovery_discover_range(gap_le_connection_by_device(&device), &range);
  fake_gatt_put_discovery_indication_health_thermometer_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_indication_random_128bit_uuid_service(TEST_GATT_CONNECTION_ID);
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS, TEST_GATT_CONNECTION_ID);


  const Service *bp_service = fake_gatt_get_blood_pressure_service();
  const Service *thermo_service = fake_gatt_get_health_thermometer_service();
  const Service *random_128bit_service = fake_gatt_get_random_128bit_uuid_service();

  BLEService service_refs_out[3];

  // Only the BP service should be part of the first generation
  uint8_t refs_out =  gatt_client_copy_service_refs_by_discovery_generation(
      &device, service_refs_out, 3, 0);
  cl_assert_equal_i(1, refs_out);
  const Uuid uuid = gatt_client_service_get_uuid(service_refs_out[0]);
  cl_assert(uuid_equal(&uuid, &bp_service->uuid));

  // Thermo & Random 128 bit service should be part of the second gen
  refs_out =  gatt_client_copy_service_refs_by_discovery_generation(
      &device, service_refs_out, 3, 1);
  cl_assert_equal_i(2, refs_out);
  const Uuid uuid1 = gatt_client_service_get_uuid(service_refs_out[0]);
  const Uuid uuid2 = gatt_client_service_get_uuid(service_refs_out[0]);
  cl_assert(uuid_equal(&uuid1, &thermo_service->uuid) ||
            uuid_equal(&uuid1, &random_128bit_service->uuid));
  cl_assert(uuid_equal(&uuid2, &thermo_service->uuid) ||
            uuid_equal(&uuid2, &random_128bit_service->uuid));
}
