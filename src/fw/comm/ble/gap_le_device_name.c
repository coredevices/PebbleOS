/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_device_name.h"
#include "bluetooth/gap_le_device_name.h"

#include "comm/bt_lock.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"

static bool prv_get_bonding_id_and_name_from_address_safe(void *ctx, char *device_name,
                                                          BTBondingID *bonding_id_out) {
  bool found = false;
  BTDeviceAddress *addr = (BTDeviceAddress *)ctx;
  GAPLEConnection *connection = gap_le_connection_by_addr(addr);

  bt_lock();
  if (!gap_le_connection_is_valid(connection) || (connection->device_name == NULL)) {
    goto unlock;
  }

  found = true;
  *bonding_id_out = connection->bonding_id;

  if (device_name) {
    strncpy(device_name, connection->device_name, BT_DEVICE_NAME_BUFFER_SIZE);
    device_name[BT_DEVICE_NAME_BUFFER_SIZE - 1] = '\0';
  }

unlock:
  bt_unlock();
  return found;
}

void bt_driver_store_device_name_kernelbg_cb(void *ctx) {
  char device_name[BT_DEVICE_NAME_BUFFER_SIZE];
  BTBondingID bonding_id = BT_BONDING_ID_INVALID;
  bool found = prv_get_bonding_id_and_name_from_address_safe(ctx, device_name, &bonding_id);
  kernel_free(ctx);

  if (!found) {
    return;
  }

  if (bonding_id != BT_BONDING_ID_INVALID) {
    // Can't access flash when bt_lock() is held...
    bt_persistent_storage_update_ble_device_name(bonding_id, device_name);
  }

  // Notify listeners even if the name couldn't be persisted (e.g. not bonded
  // yet), so the UI can pick up the freshly read name.
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
