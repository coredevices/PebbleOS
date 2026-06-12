/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bt_driver_gatt.h"

#include <bluetooth/gatt.h>
#include <bluetooth/gatt_discovery.h>
#include "comm/ble/gap_le_connection.h"

#include "kernel/pbl_malloc.h"

#include "clar_asserts.h"

#include <btutil/bt_uuid.h>
#include <util/uuid.h>

#include <string.h>

// 16-bit UUID of the GATT "Service Changed" characteristic (BT assigned numbers).
#define GATT_SERVICE_CHANGED_CHARACTERISTIC_UUID (0x2a05)

// A long-ish nominal watchdog period; the value is irrelevant to the tests,
// which fire the timer explicitly through stub_new_timer_fire.
#define WATCHDOG_TIMEOUT_MS (10000)

// Simulated discovery state, driven through the bt_driver_gatt contract by
// gatt_client_discovery.c.
static bool s_is_discovery_running;
static int s_start_count;
static int s_stop_count;
// Controller error codes (0 == success). Surfaced into the BTErrno space below.
static int s_start_ret_code;
static int s_stop_ret_code;

// The simulated controller's discovery watchdog. Created lazily and re-armed
// each time a discovery is (re)started, so firing it reports a timeout against
// the connection currently being discovered.
static TimerID s_watchdog_timer = TIMER_INVALID_ID;
static GAPLEConnection *s_watchdog_connection;

// Service Changed indications the firmware has pushed to the controller: a count
// plus a faithful copy of the last call's arguments.
static int s_service_changed_indication_count;
static BTDeviceInternal s_service_changed_last_device;
static ATTHandleRange s_service_changed_last_range;

static BTErrno prv_code_to_bterrno(int code) {
  return (code == 0) ? BTErrnoOK : (BTErrno)BTErrnoWithBluetopiaError(code);
}

static void prv_watchdog_timeout_cb(void *data) {
  // The controller reports the in-flight discovery as timed out.
  bt_driver_cb_gatt_client_discovery_complete(s_watchdog_connection,
                                              BTErrnoServiceDiscoveryTimeout);
}

static void prv_arm_watchdog(GAPLEConnection *connection) {
  if (s_watchdog_timer == TIMER_INVALID_ID) {
    s_watchdog_timer = new_timer_create();
  }
  s_watchdog_connection = connection;
  new_timer_start(s_watchdog_timer, WATCHDOG_TIMEOUT_MS, prv_watchdog_timeout_cb, NULL, 0);
}

// Frees the watchdog timer so no allocation outlives the discovery it guards.
static void prv_disarm_watchdog(void) {
  if (s_watchdog_timer != TIMER_INVALID_ID) {
    new_timer_delete(s_watchdog_timer);
    s_watchdog_timer = TIMER_INVALID_ID;
  }
}

// -- bt_driver_gatt discovery contract ----------------------------------------

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection,
                                             const ATTHandleRange *data) {
  ++s_start_count;
  if (s_start_ret_code != 0) {
    return prv_code_to_bterrno(s_start_ret_code);
  }
  s_is_discovery_running = true;
  prv_arm_watchdog((GAPLEConnection *)connection);
  return BTErrnoOK;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  ++s_stop_count;
  if (s_stop_ret_code != 0) {
    return prv_code_to_bterrno(s_stop_ret_code);
  }
  s_is_discovery_running = false;
  prv_disarm_watchdog();
  return BTErrnoOK;
}

void bt_driver_gatt_handle_discovery_abandoned(void) {
  s_is_discovery_running = false;
  prv_disarm_watchdog();
}

void bt_driver_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
  // The response is consumed by the controller; nothing observes it in tests.
}

void bt_driver_gatt_send_changed_indication(const BTDeviceInternal *device,
                                            const ATTHandleRange *data) {
  ++s_service_changed_indication_count;
  s_service_changed_last_device = *device;
  s_service_changed_last_range = *data;
}

TimerID bt_driver_gatt_get_watchdog_timer_id(void) {
  return s_watchdog_timer;
}

int fake_gatt_get_service_changed_indication_count(void) {
  return s_service_changed_indication_count;
}

const BTDeviceInternal *fake_gatt_get_service_changed_last_device(void) {
  return &s_service_changed_last_device;
}

ATTHandleRange fake_gatt_get_service_changed_last_range(void) {
  return s_service_changed_last_range;
}

// -- test accessors -----------------------------------------------------------

bool fake_gatt_is_service_discovery_running(void) {
  return s_is_discovery_running;
}

int fake_gatt_is_service_discovery_start_count(void) {
  return s_start_count;
}

int fake_gatt_is_service_discovery_stop_count(void) {
  return s_stop_count;
}

void fake_gatt_set_start_return_value(int ret_value) {
  s_start_ret_code = ret_value;
}

void fake_gatt_set_stop_return_value(int ret_value) {
  s_stop_ret_code = ret_value;
}

void fake_gatt_init(void) {
  s_is_discovery_running = false;
  s_start_count = 0;
  s_stop_count = 0;
  s_start_ret_code = 0;
  s_stop_ret_code = 0;
  prv_disarm_watchdog();
  s_watchdog_connection = NULL;
  s_service_changed_indication_count = 0;
  s_service_changed_last_device = (BTDeviceInternal){0};
  s_service_changed_last_range = (ATTHandleRange){0};
}

// -- discovery event injection ------------------------------------------------

// Builds the packed GATTService blob the firmware expects from the high-level
// Service description, mirroring the conversion the real driver performs, then
// pushes it through bt_driver_cb_gatt_client_discovery_handle_indication. The
// firmware takes ownership of the heap blob.
void fake_gatt_put_discovery_indication_service(unsigned int connection_id,
                                                const Service *service) {
  // Once the connection is gone (e.g. a disconnect raced the indication) the
  // driver no longer has a valid GAPLEConnection to hand the firmware, so it
  // drops the late indication rather than delivering a dangling pointer.
  GAPLEConnection *connection = gap_le_connection_by_gatt_id(connection_id);
  if (connection == NULL || !s_is_discovery_running) {
    return;
  }

  uint8_t num_descriptors = 0;
  for (uint8_t c = 0; c < service->num_characteristics; ++c) {
    num_descriptors += service->characteristics[c].num_descriptors;
  }

  const size_t size_bytes = COMPUTE_GATTSERVICE_SIZE_BYTES(
      service->num_characteristics, num_descriptors, service->num_included_services);
  GATTService *blob = kernel_zalloc_check(size_bytes);

  blob->uuid = service->uuid;
  blob->size_bytes = size_bytes;
  blob->att_handle = service->handle;
  blob->num_characteristics = service->num_characteristics;
  blob->num_descriptors = num_descriptors;
  blob->num_att_handles_included_services = service->num_included_services;

  // Characteristics, each immediately followed by its descriptors.
  uint8_t *end_ptr = (uint8_t *)blob->characteristics;
  for (uint8_t c = 0; c < service->num_characteristics; ++c) {
    const Characteristic *src = &service->characteristics[c];
    GATTCharacteristic *dst = (GATTCharacteristic *)end_ptr;
    *dst = (GATTCharacteristic){
        .uuid = src->uuid,
        .att_handle_offset = src->handle - service->handle,
        .properties = src->properties,
        .num_descriptors = src->num_descriptors,
    };
    for (uint8_t d = 0; d < src->num_descriptors; ++d) {
      dst->descriptors[d] = (GATTDescriptor){
          .uuid = src->descriptors[d].uuid,
          .att_handle_offset = src->descriptors[d].handle - service->handle,
      };
    }
    end_ptr += sizeof(GATTCharacteristic) + sizeof(GATTDescriptor) * src->num_descriptors;
  }

  // Included service ATT handles are tacked on after the last characteristic.
  uint16_t *included = (uint16_t *)end_ptr;
  for (uint8_t i = 0; i < service->num_included_services; ++i) {
    included[i] = service->included_services[i]->handle;
  }

  bt_driver_cb_gatt_client_discovery_handle_indication(connection, blob, BTErrnoOK);

  // Mirror the driver: when the discovered service carries the GATT "Service
  // Changed" characteristic, the driver subscribes to it and reports its handle
  // to the firmware so future Service Changed indications can be matched.
  const Uuid service_changed_uuid = bt_uuid_expand_16bit(GATT_SERVICE_CHANGED_CHARACTERISTIC_UUID);
  for (uint8_t c = 0; c < service->num_characteristics; ++c) {
    if (uuid_equal(&service->characteristics[c].uuid, &service_changed_uuid)) {
      bt_driver_cb_gatt_client_discovery_handle_service_changed(
          connection, service->characteristics[c].handle);
    }
  }
}

void fake_gatt_put_discovery_complete_event(uint8_t status, unsigned int connection_id) {
  cl_assert_equal_b(s_is_discovery_running, true);
  s_is_discovery_running = false;
  prv_disarm_watchdog();

  GAPLEConnection *connection = gap_le_connection_by_gatt_id(connection_id);
  cl_assert(connection != NULL);

  // A non-success status is surfaced as a discovery error in the BTErrno space,
  // which the firmware forwards verbatim into the client event.
  const BTErrno errno = prv_code_to_bterrno(status);
  bt_driver_cb_gatt_client_discovery_complete(connection, errno);
}

// -- reference service descriptions -------------------------------------------

static Service s_health_thermometer_service;

const Service *fake_gatt_get_health_thermometer_service(void) {
  s_health_thermometer_service = (const Service){
    .uuid = bt_uuid_expand_16bit(0x1809),
    .handle = 0x11,
    .num_characteristics = 1,
    .characteristics = {
      [0] = {
        .uuid = bt_uuid_expand_16bit(0x2a1c),
        .properties = 0x02,
        .handle = 0x13,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = 0x15,
          },
        },
      },
    },
  };
  return &s_health_thermometer_service;
}

void fake_gatt_put_discovery_indication_health_thermometer_service(unsigned int connection_id) {
  fake_gatt_put_discovery_indication_service(connection_id,
                                             fake_gatt_get_health_thermometer_service());
}

static Service s_blood_pressure_service;
#define BP_START_ATT_HANDLE 0x1
#define BP_END_ATT_HANDLE 0x9

const Service *fake_gatt_get_blood_pressure_service(void) {
  // Ensure the included Health Thermometer reference is populated.
  fake_gatt_get_health_thermometer_service();
  s_blood_pressure_service = (const Service){
    .uuid = bt_uuid_expand_16bit(0x1810),
    .handle = BP_START_ATT_HANDLE,
    .num_characteristics = 2,
    .characteristics = {
      [0] = {
        .uuid = bt_uuid_expand_16bit(0x2a35),
        .properties = 0x20,  // Indicatable
        .handle = 0x3,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = 0x05,
          },
        },
      },
      [1] = {
        .uuid = bt_uuid_expand_16bit(0x2a49),
        .properties = 0x02,
        .handle = 0x7,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = BP_END_ATT_HANDLE,
          },
        },
      },
    },
    .num_included_services = 1,
    .included_services = {
      [0] = &s_health_thermometer_service,
    },
  };
  return &s_blood_pressure_service;
}

void fake_gatt_put_discovery_indication_blood_pressure_service(unsigned int connection_id) {
  fake_gatt_put_discovery_indication_service(connection_id,
                                             fake_gatt_get_blood_pressure_service());
}

void fake_gatt_get_bp_att_handle_range(uint16_t *start, uint16_t *end) {
  *start = BP_START_ATT_HANDLE;
  *end = BP_END_ATT_HANDLE;
}

static Service s_random_128bit_service;

const Service *fake_gatt_get_random_128bit_uuid_service(void) {
  s_random_128bit_service = (const Service){
    .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                     0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB0),
    .handle = 0x17,
    .num_characteristics = 2,
    .characteristics = {
      [0] = {
        .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                         0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB1),
        .properties = 0x02,
        .handle = 0x19,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                             0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB2),
            .handle = 0x21,
          },
        },
      },
      [1] = {
        .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                         0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB3),
        .properties = 0x02,
        .handle = 0x23,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                             0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB4),
            .handle = 0x25,
          },
        },
      },
    },
  };
  return &s_random_128bit_service;
}

void fake_gatt_put_discovery_indication_random_128bit_uuid_service(unsigned int connection_id) {
  fake_gatt_put_discovery_indication_service(connection_id,
                                             fake_gatt_get_random_128bit_uuid_service());
}

static Service s_gatt_profile_service;

void fake_gatt_put_discovery_indication_gatt_profile_service(
    unsigned int connection_id, bool has_service_changed_characteristic) {
  s_gatt_profile_service = (const Service){
    .uuid = bt_uuid_expand_16bit(0x1801),
    .handle = 0x1,
    .num_characteristics = has_service_changed_characteristic ? 1 : 0,
    .characteristics = {
      [0] = {
        .uuid = bt_uuid_expand_16bit(0x2a05),
        .properties = 0x20,
        .handle = 0x3,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = 0x05,
          },
        },
      },
    },
  };
  fake_gatt_put_discovery_indication_service(connection_id, &s_gatt_profile_service);
}

uint16_t fake_gatt_gatt_profile_service_service_changed_att_handle(void) {
  return 3;  // .handle = 0x3
}

uint16_t fake_gatt_gatt_profile_service_service_changed_cccd_att_handle(void) {
  return 5;  // descriptor .handle = 0x05
}
