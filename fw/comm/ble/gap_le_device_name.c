/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_device_name.h"
#include "pbl/bluetooth/gap_le_device_name.h"

#include "comm/bt_lock.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"

pbl_bt_bonding_id_t prv_get_bonding_id_and_name_from_address_safe(void *ctx, char *device_name) {
  pbl_bt_bonding_id_t bonding_id = PBL_BT_BONDING_ID_INVALID;
  struct pbl_bt_addr *addr = (struct pbl_bt_addr *)ctx;
  GAPLEConnection *connection = gap_le_connection_by_addr(addr);

  bt_lock();
  if (!gap_le_connection_is_valid(connection)) {
    goto unlock;
  }

  bonding_id = connection->bonding_id;

  if (device_name) {
    strncpy(device_name, connection->device_name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
    device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE - 1] = '\0';
  }

unlock:
  bt_unlock();
  return bonding_id;
}

void pbl_bt_store_device_name_kernelbg_cb(void *ctx) {
  char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  pbl_bt_bonding_id_t bonding_id = prv_get_bonding_id_and_name_from_address_safe(ctx, device_name);
  kernel_free(ctx);

  if (bonding_id == PBL_BT_BONDING_ID_INVALID) {
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
  pbl_bt_gap_le_device_name_request_all();
  bt_unlock();
}

void gap_le_device_name_request(const struct pbl_bt_device_internal *address) {
  bt_lock();
  pbl_bt_gap_le_device_name_request(address);
  bt_unlock();
}
