/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/hrm_service.h>

#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <system/logging.h>
#include <system/passert.h>

#include "nimble_type_conversions.h"

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define HRM_SERVICE_UUID (0x180D)
#define HRM_MEASUREMENT_UUID (0x2A37)
#define HRM_BODY_SENSOR_LOCATION_UUID (0x2A38)

#define HRM_MEASUREMENT_FLAG_UINT16_BPM (1 << 0)
#define HRM_MEASUREMENT_FLAG_SENSOR_CONTACT_DETECTED (1 << 1)
#define HRM_MEASUREMENT_FLAG_SENSOR_CONTACT_SUPPORTED (1 << 2)

// Bluetooth SIG Body Sensor Location: Wrist.
#define HRM_BODY_SENSOR_LOCATION_WRIST (0x02)

static uint16_t s_hrm_measurement_handle;
static bool s_hrm_service_enabled;

static int prv_access_measurement(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static int prv_access_body_sensor_location(uint16_t conn_handle, uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
  const uint8_t body_sensor_location = HRM_BODY_SENSOR_LOCATION_WRIST;
  const int rc = os_mbuf_append(ctxt->om, &body_sensor_location, sizeof(body_sensor_location));
  return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def s_hrm_service[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID16_DECLARE(HRM_SERVICE_UUID),
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
        .uuid = BLE_UUID16_DECLARE(HRM_MEASUREMENT_UUID),
        .access_cb = prv_access_measurement,
        .val_handle = &s_hrm_measurement_handle,
        .flags = BLE_GATT_CHR_F_NOTIFY,
      },
      {
        .uuid = BLE_UUID16_DECLARE(HRM_BODY_SENSOR_LOCATION_UUID),
        .access_cb = prv_access_body_sensor_location,
        .flags = BLE_GATT_CHR_F_READ,
      },
      {
        0,
      },
    },
  },
  {
    0,
  },
};

static size_t prv_encode_measurement(const BleHrmServiceMeasurement *measurement,
                                     uint8_t *buffer, size_t buffer_size) {
  PBL_ASSERTN(buffer_size >= 3);

  uint8_t flags = HRM_MEASUREMENT_FLAG_SENSOR_CONTACT_SUPPORTED;
  if (measurement->is_on_wrist) {
    flags |= HRM_MEASUREMENT_FLAG_SENSOR_CONTACT_DETECTED;
  }

  size_t length = 0;
  buffer[length++] = flags;

  if (measurement->bpm > UINT8_MAX) {
    flags |= HRM_MEASUREMENT_FLAG_UINT16_BPM;
    buffer[0] = flags;
    buffer[length++] = measurement->bpm & 0xff;
    buffer[length++] = measurement->bpm >> 8;
  } else {
    buffer[length++] = (uint8_t)measurement->bpm;
  }

  return length;
}

bool bt_driver_is_hrm_service_supported(void) {
  return true;
}

void bt_driver_hrm_service_init(void) {
  const int count_rc = ble_gatts_count_cfg(s_hrm_service);
  PBL_ASSERTN(count_rc == 0);

  const int add_rc = ble_gatts_add_svcs(s_hrm_service);
  PBL_ASSERTN(add_rc == 0);
}

void bt_driver_hrm_service_enable(bool enable) {
  s_hrm_service_enabled = enable;
}

void bt_driver_hrm_service_handle_measurement(const BleHrmServiceMeasurement *measurement,
                                              const BTDeviceInternal *permitted_devices,
                                              size_t num_permitted_devices) {
  if (!s_hrm_service_enabled || !measurement) {
    return;
  }

  uint8_t payload[3];
  const size_t payload_size = prv_encode_measurement(measurement, payload, sizeof(payload));

  for (size_t i = 0; i < num_permitted_devices; ++i) {
    uint16_t conn_handle;
    if (!pebble_device_to_nimble_conn_handle(&permitted_devices[i], &conn_handle)) {
      continue;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, payload_size);
    if (!om) {
      PBL_LOG_ERR("Failed to allocate HRM measurement notification");
      return;
    }

    const int rc = ble_gatts_notify_custom(conn_handle, s_hrm_measurement_handle, om);
    if (rc != 0) {
      PBL_LOG_ERR("Failed to notify HRM measurement: 0x%04x", (uint16_t)rc);
    }
  }
}

void bt_driver_hrm_service_handle_subscription(uint16_t conn_handle, uint16_t attr_handle,
                                               bool is_subscribed) {
  if (attr_handle != s_hrm_measurement_handle) {
    return;
  }

  struct ble_gap_conn_desc desc;
  const int rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to find HRM subscriber connection: 0x%04x",
                  (uint16_t)rc);
    return;
  }

  BTDeviceInternal device;
  nimble_addr_to_pebble_device(&desc.peer_id_addr, &device);
  bt_driver_cb_hrm_service_update_subscription(&device, is_subscribed);
}
