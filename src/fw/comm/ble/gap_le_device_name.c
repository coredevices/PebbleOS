/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_device_name.h"
#include "bluetooth/gap_le_device_name.h"

#include "comm/bt_lock.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"

BTBondingID prv_get_bonding_id_and_name_from_address_safe(void *ctx, char* device_name) {
  BTBondingID bonding_id = BT_BONDING_ID_INVALID;
  BTDeviceAddress *addr = (BTDeviceAddress *)ctx;
  GAPLEConnection *connection = gap_le_connection_by_addr(addr);

  bt_lock();
  if (!gap_le_connection_is_valid(connection)) {
    goto unlock;
  }

  bonding_id = connection->bonding_id;

  if (device_name) {
    strncpy(device_name, connection->device_name, BT_DEVICE_NAME_BUFFER_SIZE);
    device_name[BT_DEVICE_NAME_BUFFER_SIZE - 1] = '\0';
  }

unlock:
  bt_unlock();
  return bonding_id;
}

void bt_driver_store_device_name_kernelbg_cb(void *ctx) {
  char device_name[BT_DEVICE_NAME_BUFFER_SIZE];
  BTBondingID bonding_id = prv_get_bonding_id_and_name_from_address_safe(ctx, device_name);
  kernel_free(ctx);

  if (bonding_id == BT_BONDING_ID_INVALID) {
    return;
  }

  // Can't access flash when bt_lock() is held...
  if (!bt_persistent_storage_update_ble_device_name(bonding_id, device_name)) {
    return;
  }

  PebbleEvent event = {
    .type = PEBBLE_BLE_DEVICE_NAME_UPDATED_EVENT,
  };
  event_put(&event);
}

void gap_le_device_name_request_all(void) {
  bt_lock();
  bt_driver_gap_le_device_name_request_all();
  bt_unlock();
}

void gap_le_device_name_request(const BTDeviceInternal *address) {
  bt_lock();
  bt_driver_gap_le_device_name_request(address);
  bt_unlock();
}
