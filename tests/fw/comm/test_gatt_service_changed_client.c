/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_service_changed.h"
#include "comm/ble/gap_le_connection.h"

#include "kernel/events.h"

#include "clar.h"

#include <btutil/bt_device.h>

// Fakes
///////////////////////////////////////////////////////////

#include "fake_bt_driver_gatt.h"
#include "fake_pbl_malloc.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_system_task.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_bt_lock.h"
#include "stubs_events.h"
#include "stubs_gatt_client_subscriptions.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"

void core_dump_reset(bool is_forced) {
}

static GAPLEConnection s_connection;

GAPLEConnection *gap_le_connection_by_device(const BTDeviceInternal *addr) {
  return &s_connection;
}

GAPLEConnection *gap_le_connection_by_addr(const BTDeviceAddress *addr) {
  return &s_connection;
}

GAPLEConnection *gap_le_connection_by_gatt_id(unsigned int connection_id) {
  return &s_connection;
}

bool gap_le_connection_is_valid(const GAPLEConnection *conn) {
  return true;
}

GAPLEConnection *gap_le_connection_any(void) {
  return NULL;
}

uint16_t gaps_get_starting_att_handle(void) {
  return 4;
}

GAPLEConnection *gatt_client_characteristic_get_connection(BLECharacteristic characteristic_ref) {
  return NULL;
}

BLEService gatt_client_att_handle_get_service(
    GAPLEConnection *connection, uint16_t att_handle, const GATTServiceNode **service_node_out) {
  return 0;
}

uint8_t gatt_client_copy_service_refs_by_discovery_generation(
    const BTDeviceInternal *device, BLEService services_out[],
    uint8_t num_services, uint8_t discovery_gen) { return 0;}

void gatt_client_service_get_all_characteristics_and_descriptors(
    GAPLEConnection *connection, GATTService *service,
    BLECharacteristic *characteristic_hdls_out,
    BLEDescriptor *descriptor_hdls_out) { }

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// FIXME: PBL-23945
void fake_kernel_malloc_mark(void) { }
void fake_kernel_malloc_mark_assert_equal(void) { }

// Helpers
///////////////////////////////////////////////////////////

#define TEST_GATT_CONNECTION_ID (1234)

static const BTDeviceInternal s_device = {
  .address = { .octets = { 1, 2, 3, 4, 5, 6 } },
};

// Tests
///////////////////////////////////////////////////////////

void test_gatt_service_changed_client__initialize(void) {
  fake_gatt_init();
  s_connection = (GAPLEConnection) {
    .device = s_device,
    .gatt_connection_id = TEST_GATT_CONNECTION_ID,
    .gatt_service_changed_att_handle = 0,
  };
  // Kick off a discovery so the fake driver is in the running state and will
  // deliver the service indications the tests inject below.
  cl_assert_equal_i(gatt_client_discovery_discover_all(&s_device), BTErrnoOK);
}

void test_gatt_service_changed_client__cleanup(void) {
}

// Discovery
///////////////////////////////////////////////////////////

void test_gatt_service_changed_client__handle_non_gatt_profile_service(void) {
  fake_gatt_put_discovery_indication_blood_pressure_service(TEST_GATT_CONNECTION_ID);
  // A service without the Service Changed characteristic leaves the recorded
  // handle untouched.
  cl_assert_equal_i(s_connection.gatt_service_changed_att_handle, 0);
}

void test_gatt_service_changed_client__handle_gatt_profile_service(void) {
  fake_gatt_put_discovery_indication_gatt_profile_service(TEST_GATT_CONNECTION_ID,
                                                     true /* has_service_changed_characteristic */);
  // The driver reports the Service Changed characteristic's handle, which the
  // firmware records so it can match future Service Changed indications. (The
  // CCCD subscription that the old Bluetopia client performed now lives in the
  // driver; see commit 3b9276848 onward and src/bluetooth-fw/nimble.)
  cl_assert_equal_i(s_connection.gatt_service_changed_att_handle,
                    fake_gatt_gatt_profile_service_service_changed_att_handle());
}

void test_gatt_service_changed_client__handle_gatt_profile_service_missing_service_changed(void) {
  fake_gatt_put_discovery_indication_gatt_profile_service(TEST_GATT_CONNECTION_ID,
                                                    false /* has_service_changed_characteristic */);
  // No Service Changed characteristic in the profile service: nothing recorded.
  cl_assert_equal_i(s_connection.gatt_service_changed_att_handle, 0);
}

// Characteristic Value Indications
///////////////////////////////////////////////////////////

void test_gatt_service_changed_client__handle_indication_non_service_changed(void) {
  fake_gatt_put_discovery_indication_gatt_profile_service(TEST_GATT_CONNECTION_ID,
                                                     true /* has_service_changed_characteristic */);
  const uint8_t value;
  const bool handled = gatt_service_changed_client_handle_indication(&s_connection, 0xfffe,
                                                                     &value, sizeof(value));
  cl_assert_equal_b(handled, false);
}

void test_gatt_service_changed_client__handle_indication_service_changed(void) {
  fake_gatt_put_discovery_indication_gatt_profile_service(TEST_GATT_CONNECTION_ID,
                                                     true /* has_service_changed_characteristic */);
  // Finish the initial discovery so the indication-triggered rediscovery can
  // start a fresh one.
  fake_gatt_put_discovery_complete_event(GATT_SERVICE_DISCOVERY_STATUS_SUCCESS,
                                         TEST_GATT_CONNECTION_ID);
  const uint16_t att_handle = fake_gatt_gatt_profile_service_service_changed_att_handle();

  fake_kernel_malloc_mark();

  const int start_count_before_indication = fake_gatt_is_service_discovery_start_count();

  const uint16_t handle_range[2] = {
    [0] = 0x1,
    [1] = 0xfffe,
  };
  const bool handled = gatt_service_changed_client_handle_indication(&s_connection, att_handle,
                                              (const uint8_t *) handle_range, sizeof(uint16_t) * 2);
  // Re-discovery is trigger on KernelBG:
  fake_system_task_callbacks_invoke_pending();

  // The KernelBG trip uses kernel_malloc, make sure it's cleaning up properly:
  fake_kernel_malloc_mark_assert_equal();
  cl_assert_equal_b(handled, true);

  // Expect service discovery to be started once more:
  cl_assert_equal_i(start_count_before_indication + 1,
                    fake_gatt_is_service_discovery_start_count());
}

void test_gatt_service_changed_client__handle_indication_service_changed_malformatted(void) {
  fake_gatt_put_discovery_indication_gatt_profile_service(TEST_GATT_CONNECTION_ID,
                                                     true /* has_service_changed_characteristic */);
  const uint16_t att_handle = fake_gatt_gatt_profile_service_service_changed_att_handle();

  const uint16_t handle_range[1] = {
    [0] = 0x1,
  };
  const bool handled = gatt_service_changed_client_handle_indication(&s_connection, att_handle,
                                              (const uint8_t *) handle_range, sizeof(uint16_t) * 1);
  cl_assert_equal_b(handled, true);
}
