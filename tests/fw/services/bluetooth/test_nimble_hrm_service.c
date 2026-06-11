/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/hrm_service.h>

#include <host/ble_att.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <util/size.h>

#include "clar.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_logging.h"
#include "stubs_passert.h"

#define HRM_SERVICE_UUID (0x180D)
#define HRM_MEASUREMENT_UUID (0x2A37)
#define HRM_BODY_SENSOR_LOCATION_UUID (0x2A38)
#define HRM_BODY_SENSOR_LOCATION_WRIST (0x02)

#define TEST_MEASUREMENT_HANDLE (0x0201)
#define TEST_CONN_HANDLE_A (0x1001)
#define TEST_CONN_HANDLE_B (0x1002)

typedef struct {
  uint16_t conn_handle;
  uint16_t attr_handle;
  uint8_t payload[3];
  uint16_t payload_length;
} NotifyCall;

static const struct ble_gatt_svc_def *s_registered_services;
static int s_count_cfg_call_count;
static int s_add_svcs_call_count;
static int s_notify_call_count;
static NotifyCall s_notify_calls[4];
static BTDeviceInternal s_subscription_device;
static bool s_subscription_state;
static int s_subscription_call_count;
static ble_addr_t s_peer_addr_a;
static int s_gap_conn_find_rc;

static uint16_t prv_uuid16(const ble_uuid_t *uuid) {
  cl_assert(uuid);
  cl_assert_equal_i(uuid->type, BLE_UUID_TYPE_16);
  return ((const ble_uuid16_t *)uuid)->value;
}

static const struct ble_gatt_chr_def *prv_find_chr(uint16_t uuid) {
  const struct ble_gatt_chr_def *chr = s_registered_services[0].characteristics;
  while (chr && chr->uuid) {
    if (prv_uuid16(chr->uuid) == uuid) {
      return chr;
    }
    ++chr;
  }
  return NULL;
}

static struct os_mbuf *prv_mbuf_alloc(uint16_t size) {
  struct os_mbuf *om = calloc(1, sizeof(*om) + size);
  cl_assert(om);
  om->om_data = om->om_databuf;
  return om;
}

int os_mbuf_append(struct os_mbuf *om, const void *data, uint16_t len) {
  memcpy(om->om_data + om->om_len, data, len);
  om->om_len += len;
  return 0;
}

struct os_mbuf *ble_hs_mbuf_from_flat(const void *buf, uint16_t len) {
  struct os_mbuf *om = prv_mbuf_alloc(len);
  os_mbuf_append(om, buf, len);
  return om;
}

int ble_gatts_count_cfg(const struct ble_gatt_svc_def *defs) {
  ++s_count_cfg_call_count;
  cl_assert(defs);
  return 0;
}

int ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs) {
  ++s_add_svcs_call_count;
  s_registered_services = svcs;

  const struct ble_gatt_chr_def *measurement_chr = prv_find_chr(HRM_MEASUREMENT_UUID);
  cl_assert(measurement_chr);
  cl_assert(measurement_chr->val_handle);
  *measurement_chr->val_handle = TEST_MEASUREMENT_HANDLE;

  return 0;
}

int ble_gatts_notify_custom(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) {
  cl_assert(s_notify_call_count < (int)ARRAY_LENGTH(s_notify_calls));

  NotifyCall *call = &s_notify_calls[s_notify_call_count++];
  call->conn_handle = conn_handle;
  call->attr_handle = attr_handle;
  call->payload_length = om->om_len;
  memcpy(call->payload, om->om_data, om->om_len);

  free(om);
  return 0;
}

bool pebble_device_to_nimble_conn_handle(const BTDeviceInternal *device, uint16_t *handle) {
  switch (device->address.octets[0]) {
    case 0xA1:
      *handle = TEST_CONN_HANDLE_A;
      return true;
    case 0xB1:
      *handle = TEST_CONN_HANDLE_B;
      return true;
    default:
      return false;
  }
}

void nimble_addr_to_pebble_device(const ble_addr_t *stack_addr, BTDeviceInternal *host_addr) {
  memcpy(host_addr->address.octets, stack_addr->val, sizeof(host_addr->address.octets));
  host_addr->is_random_address = stack_addr->type == BLE_ADDR_RANDOM;
  host_addr->is_classic = false;
}

int ble_gap_conn_find(uint16_t handle, struct ble_gap_conn_desc *out_desc) {
  if (s_gap_conn_find_rc != 0 || handle != TEST_CONN_HANDLE_A) {
    return -1;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->conn_handle = handle;
  out_desc->peer_id_addr = s_peer_addr_a;
  return 0;
}

void bt_driver_cb_hrm_service_update_subscription(const BTDeviceInternal *device,
                                                  bool is_subscribed) {
  ++s_subscription_call_count;
  s_subscription_device = *device;
  s_subscription_state = is_subscribed;
}

static void prv_assert_payload(const NotifyCall *call, const uint8_t *payload, size_t length) {
  cl_assert_equal_i(call->attr_handle, TEST_MEASUREMENT_HANDLE);
  cl_assert_equal_i(call->payload_length, length);
  cl_assert_equal_m(call->payload, payload, length);
}

// Tests
///////////////////////////////////////////////////////////

void test_nimble_hrm_service__initialize(void) {
  s_registered_services = NULL;
  s_count_cfg_call_count = 0;
  s_add_svcs_call_count = 0;
  s_notify_call_count = 0;
  s_subscription_device = (BTDeviceInternal){};
  s_subscription_state = false;
  s_subscription_call_count = 0;
  s_gap_conn_find_rc = 0;
  memset(s_notify_calls, 0, sizeof(s_notify_calls));

  s_peer_addr_a = (ble_addr_t){
      .type = BLE_ADDR_RANDOM,
      .val = {1, 2, 3, 4, 5, 6},
  };

  bt_driver_hrm_service_enable(false);
  bt_driver_hrm_service_init();
}

void test_nimble_hrm_service__registers_standard_heart_rate_service(void) {
  cl_assert_equal_b(true, bt_driver_is_hrm_service_supported());
  cl_assert_equal_i(1, s_count_cfg_call_count);
  cl_assert_equal_i(1, s_add_svcs_call_count);

  cl_assert(s_registered_services);
  cl_assert_equal_i(s_registered_services[0].type, BLE_GATT_SVC_TYPE_PRIMARY);
  cl_assert_equal_i(prv_uuid16(s_registered_services[0].uuid), HRM_SERVICE_UUID);

  const struct ble_gatt_chr_def *measurement_chr = prv_find_chr(HRM_MEASUREMENT_UUID);
  cl_assert(measurement_chr);
  cl_assert_equal_i(measurement_chr->flags, BLE_GATT_CHR_F_NOTIFY);

  struct ble_gatt_access_ctxt ctxt = {};
  cl_assert_equal_i(
      measurement_chr->access_cb(TEST_CONN_HANDLE_A, TEST_MEASUREMENT_HANDLE, &ctxt, NULL),
      BLE_ATT_ERR_READ_NOT_PERMITTED);

  const struct ble_gatt_chr_def *body_location_chr = prv_find_chr(HRM_BODY_SENSOR_LOCATION_UUID);
  cl_assert(body_location_chr);
  cl_assert_equal_i(body_location_chr->flags, BLE_GATT_CHR_F_READ);

  struct os_mbuf *om = prv_mbuf_alloc(1);
  ctxt.om = om;
  cl_assert_equal_i(body_location_chr->access_cb(TEST_CONN_HANDLE_A, 0, &ctxt, NULL), 0);
  cl_assert_equal_i(om->om_len, 1);
  cl_assert_equal_i(om->om_data[0], HRM_BODY_SENSOR_LOCATION_WRIST);
  free(om);
}

void test_nimble_hrm_service__does_not_notify_when_disabled(void) {
  const BTDeviceInternal device = {
      .address.octets = {0xA1},
  };
  const BleHrmServiceMeasurement measurement = {
      .bpm = 80,
      .is_on_wrist = true,
  };

  bt_driver_hrm_service_handle_measurement(&measurement, &device, 1);

  cl_assert_equal_i(0, s_notify_call_count);
}

void test_nimble_hrm_service__notifies_standard_heart_rate_measurements(void) {
  const BTDeviceInternal devices[] = {
      {.address.octets = {0xA1}},
      {.address.octets = {0xB1}},
      {.address.octets = {0xC1}},
  };

  bt_driver_hrm_service_enable(true);

  const BleHrmServiceMeasurement measurement = {
      .bpm = 80,
      .is_on_wrist = true,
  };
  bt_driver_hrm_service_handle_measurement(&measurement, devices, ARRAY_LENGTH(devices));

  const uint8_t expected_8bit_payload[] = {0x06, 80};
  cl_assert_equal_i(2, s_notify_call_count);
  cl_assert_equal_i(s_notify_calls[0].conn_handle, TEST_CONN_HANDLE_A);
  prv_assert_payload(&s_notify_calls[0], expected_8bit_payload, sizeof(expected_8bit_payload));
  cl_assert_equal_i(s_notify_calls[1].conn_handle, TEST_CONN_HANDLE_B);
  prv_assert_payload(&s_notify_calls[1], expected_8bit_payload, sizeof(expected_8bit_payload));

  const BleHrmServiceMeasurement high_measurement = {
      .bpm = 300,
      .is_on_wrist = false,
  };
  bt_driver_hrm_service_handle_measurement(&high_measurement, devices, 1);

  const uint8_t expected_16bit_payload[] = {0x05, 0x2c, 0x01};
  cl_assert_equal_i(3, s_notify_call_count);
  prv_assert_payload(&s_notify_calls[2], expected_16bit_payload, sizeof(expected_16bit_payload));
}

void test_nimble_hrm_service__subscription_reports_peer_device(void) {
  bt_driver_hrm_service_handle_subscription(TEST_CONN_HANDLE_A, TEST_MEASUREMENT_HANDLE, true);

  cl_assert_equal_i(1, s_subscription_call_count);
  cl_assert_equal_b(true, s_subscription_state);
  cl_assert_equal_m(s_subscription_device.address.octets, s_peer_addr_a.val,
                    sizeof(s_subscription_device.address.octets));
  cl_assert_equal_b(true, s_subscription_device.is_random_address);

  bt_driver_hrm_service_handle_subscription(TEST_CONN_HANDLE_A, TEST_MEASUREMENT_HANDLE + 1, false);
  cl_assert_equal_i(1, s_subscription_call_count);

  s_gap_conn_find_rc = -1;
  bt_driver_hrm_service_handle_subscription(TEST_CONN_HANDLE_A, TEST_MEASUREMENT_HANDLE, false);
  cl_assert_equal_i(1, s_subscription_call_count);
}
