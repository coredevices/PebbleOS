/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/gap_le_device_name.h>
#include <comm/bt_lock.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <services/common/system_task.h>

#include "nimble_type_conversions.h"

#define GAP_DEVICE_NAME_CHR (0x2A00)

const ble_uuid16_t device_name_chr_uuid = BLE_UUID16_INIT(GAP_DEVICE_NAME_CHR);

static int prv_device_name_read_event_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                         struct ble_gatt_attr *attr, void *arg) {
  if (error->status != 0) {
    if (error->status != BLE_HS_EDONE) {
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "prv_device_name_read_event_cb error=%d",
                error->status);
    }
    return 0;
  }

  char *device_name = kernel_zalloc_check(attr->om->om_len + 1);
  strncpy(device_name, (char *)attr->om->om_data, attr->om->om_len);

  bt_lock();

  GAPLEConnection *connection = (GAPLEConnection *)arg;
  if (connection->device_name) {
    kernel_free(connection->device_name);
  }
  connection->device_name = device_name;

  bt_unlock();

  BTDeviceAddress *addr = kernel_zalloc_check(sizeof(BTDeviceAddress));
  *addr = connection->device.address;
  system_task_add_callback(bt_driver_store_device_name_kernelbg_cb, addr);

  return 0;
}

static void prv_gap_le_device_name_request(GAPLEConnection *connection) {
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_gap_le_device_name_request: Failed to find connection handle");
    return;
  }

  int rc = ble_gattc_read_by_uuid(conn_handle, 1, UINT16_MAX, (ble_uuid_t *)&device_name_chr_uuid,
                                  prv_device_name_read_event_cb, (void *)connection);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_gap_le_device_name_request ble_gattc_read_by_uuid rc=%d", rc);
  }
}

static void prv_request_device_name_cb(GAPLEConnection *connection, void *data) {
    prv_gap_le_device_name_request(connection);
  }

void bt_driver_gap_le_device_name_request(const BTDeviceInternal *device) {
  GAPLEConnection *connection = gap_le_connection_by_device(device);
  if (connection == NULL) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "bt_driver_gap_le_device_name_request gap_le_connection_by_device returned NULL");
    return;
  }
  prv_gap_le_device_name_request(connection);
}

void bt_driver_gap_le_device_name_request_all(void) {
  gap_le_connection_for_each(prv_request_device_name_cb, NULL);
}
