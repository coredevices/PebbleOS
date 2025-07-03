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

#include "nimble_type_conversions.h"

#include <bluetooth/gatt.h>

#include <host/ble_gatt.h>

static int prv_gatt_write_event_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr, void *arg) {
  GattClientOpWriteReponse resp = {
      .hdr = {
          .type = GattClientOpResponseWrite,
          .error_code = error->status == 0 ? 0 : BTErrnoInternalErrorBegin + error->status,
          .context = arg,
      }};
  bt_driver_cb_gatt_client_operations_handle_response(&resp.hdr);
  return 0;
}

static int prv_gatt_read_event_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                  struct ble_gatt_attr *attr, void *arg) {
  GattClientOpReadReponse resp = {
      .hdr =
          {
              .type = GattClientOpResponseRead,
              .error_code = error->status == 0 ? 0 : BTErrnoInternalErrorBegin + error->status,
              .context = arg,
          },
      .value = attr->om->om_data,
      .value_length = attr->om->om_len,
  };
  bt_driver_cb_gatt_client_operations_handle_response(&resp.hdr);
  return 0;
}

BTErrno bt_driver_gatt_write_without_response(GAPLEConnection *connection, const uint8_t *value,
                                              size_t value_length, uint16_t att_handle) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG_VERBOSE, "bt_driver_gatt_write_without_response: %d",
            att_handle);
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    return BTErrnoInvalidState;
  }

  int rc = ble_gattc_write_no_rsp_flat(conn_handle, att_handle, value, value_length);
  return rc == 0 ? BTErrnoOK : BTErrnoInternalErrorBegin + rc;
}

BTErrno bt_driver_gatt_write(GAPLEConnection *connection, const uint8_t *value, size_t value_length,
                             uint16_t att_handle, void *context) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG_VERBOSE, "bt_driver_gatt_write: %d", att_handle);
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    return BTErrnoInvalidState;
  }

  int rc = ble_gattc_write_flat(conn_handle, att_handle, value, value_length,
                                prv_gatt_write_event_cb, context);
  return rc == 0 ? BTErrnoOK : BTErrnoInternalErrorBegin + rc;
}

BTErrno bt_driver_gatt_read(GAPLEConnection *connection, uint16_t att_handle, void *context) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG_VERBOSE, "bt_driver_gatt_read: %d", att_handle);
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    return BTErrnoInvalidState;
  }

  int rc = ble_gattc_read(conn_handle, att_handle, prv_gatt_read_event_cb, context);
  return rc == 0 ? BTErrnoOK : BTErrnoInternalErrorBegin + rc;
}
