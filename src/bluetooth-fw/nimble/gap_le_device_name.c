/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/gap_le_device_name.h>
#include <comm/bt_lock.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <kernel/pbl_malloc.h>
#include <os/os_mbuf.h>
#include <pbl/services/system_task.h>
#include <system/logging.h>

#include "nimble_gattc_op_queue.h"
#include "nimble_type_conversions.h"

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define GAP_DEVICE_NAME_CHR (0x2A00)

const ble_uuid16_t device_name_chr_uuid = BLE_UUID16_INIT(GAP_DEVICE_NAME_CHR);

static int prv_device_name_read_event_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                         struct ble_gatt_attr *attr, void *arg) {
  const BTDeviceInternal *device = arg;

  if (error->status != 0) {
    if (error->status != BLE_HS_EDONE) {
      PBL_LOG_ERR("prv_device_name_read_event_cb error=%d",
                error->status);
    }
    // Frees arg (the op context); must be the last use of it
    nimble_gattc_op_queue_complete();
    return 0;
  }

  const uint16_t name_len = OS_MBUF_PKTLEN(attr->om);

  PBL_LOG_DBG("Device name read cb: conn=%u handle=%u len=%u", conn_handle, attr->handle,
              name_len);

  char *device_name = kernel_zalloc_check(name_len + 1);
  if (ble_hs_mbuf_to_flat(attr->om, device_name, name_len, NULL) != 0) {
    kernel_free(device_name);
    return 0;
  }

  bool changed;
  BTDeviceAddress addr_copy;

  bt_lock();

  GAPLEConnection *connection = gap_le_connection_by_device(device);
  if (connection == NULL) {
    bt_unlock();
    kernel_free(device_name);
    return 0;
  }

  changed = (connection->device_name == NULL) || strcmp(connection->device_name, device_name) != 0;
  if (changed) {
    if (connection->device_name) {
      kernel_free(connection->device_name);
    }
    connection->device_name = device_name;
  }
  addr_copy = connection->device.address;

  bt_unlock();

  if (!changed) {
    // Same name as before: don't re-persist it (each store rewrites the
    // pairing storage and fires bonding change handlers).
    kernel_free(device_name);
    return 0;
  }

  BTDeviceAddress *addr = kernel_zalloc_check(sizeof(BTDeviceAddress));
  *addr = addr_copy;
  system_task_add_callback(bt_driver_store_device_name_kernelbg_cb, addr);

  return 0;
}

static int prv_device_name_read_op_start(void *ctx) {
  const BTDeviceInternal *device = ctx;
  uint16_t conn_handle;

  if (!pebble_device_to_nimble_conn_handle(device, &conn_handle)) {
    PBL_LOG_WRN("Device name read: connection is gone");
    return BLE_HS_ENOTCONN;
  }

  PBL_LOG_DBG("Device name read request: conn=%u", conn_handle);

  int rc = ble_gattc_read_by_uuid(conn_handle, 1, UINT16_MAX, (ble_uuid_t *)&device_name_chr_uuid,
                                  prv_device_name_read_event_cb, ctx);
  if (rc != 0) {
    PBL_LOG_ERR("prv_device_name_read_op_start ble_gattc_read_by_uuid rc=0x%04x", (uint16_t)rc);
  }

  return rc;
}

void bt_driver_gap_le_device_name_request(const BTDeviceInternal *device) {
  BTDeviceInternal *ctx = kernel_zalloc_check(sizeof(*ctx));
  *ctx = *device;
  nimble_gattc_op_queue_push(prv_device_name_read_op_start, ctx);
}

static void prv_request_device_name_cb(GAPLEConnection *connection, void *data) {
  bt_driver_gap_le_device_name_request(&connection->device);
}

void bt_driver_gap_le_device_name_request_all(void) {
  bt_lock();
  gap_le_connection_for_each(prv_request_device_name_cb, NULL);
  bt_unlock();
}
