/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/gatt.h>

#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <services/gatt/ble_svc_gatt.h>
#include <system/logging.h>

#include "nimble_type_conversions.h"

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

void bt_driver_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {}

void bt_driver_gatt_send_changed_indication(const BTDeviceInternal *device,
                                            const ATTHandleRange *data) {
  // Resolve the NimBLE connection handle through the NimBLE connection table
  // (ble_gap_conn_find_by_addr). No bt_lock needed — the device address is a
  // value copy from the service layer, which held bt_lock when it took the copy.
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(device, &conn_handle)) {
    PBL_LOG_ERR("Service Changed: no connection handle for device");
    return;
  }

  // The Service Changed characteristic lives in the GATT profile service (0x1801),
  // registered by ble_svc_gatt_init(). Look up its value handle.
  const ble_uuid16_t svc_uuid = BLE_UUID16_INIT(GATT_SERVICE_UUID);
  const ble_uuid16_t chr_uuid = BLE_UUID16_INIT(BLE_SVC_GATT_CHR_SERVICE_CHANGED_UUID16);
  uint16_t val_handle;
  int rc = ble_gatts_find_chr(&svc_uuid.u, &chr_uuid.u, NULL, &val_handle);
  if (rc != 0) {
    PBL_LOG_ERR("Service Changed: find_chr failed: 0x%04x", (uint16_t)rc);
    return;
  }

  // The indication value is the changed ATT handle range (start/end, little-endian).
  struct os_mbuf *om = ble_hs_mbuf_from_flat(data, sizeof(*data));
  if (!om) {
    PBL_LOG_ERR("Service Changed: failed to allocate mbuf");
    return;
  }

  rc = ble_gatts_indicate_custom(conn_handle, val_handle, om);
  if (rc != 0) {
    PBL_LOG_ERR("Service Changed: indicate failed: 0x%04x", (uint16_t)rc);
  }
}
