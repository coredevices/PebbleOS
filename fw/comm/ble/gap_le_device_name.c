/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_device_name.h"

#include <pbl/bluetooth/gap_le_device_name.h>
#include <pbl/services/bluetooth/bt_settings.h>

#include <comm/bt_lock.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>

static bool prv_get_identity_and_name_from_address_safe(void *ctx,
                                                        struct pbl_bt_device_internal *identity,
                                                        char *device_name) {
  bool is_bonded = false;
  struct pbl_bt_addr *addr = (struct pbl_bt_addr *)ctx;
  GAPLEConnection *connection = gap_le_connection_by_addr(addr);

  bt_lock();
  if (!gap_le_connection_is_valid(connection) || !connection->is_bonded) {
    goto unlock;
  }

  is_bonded = true;
  *identity = connection->identity;
  strncpy(device_name, connection->device_name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
  device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE - 1] = '\0';

unlock:
  bt_unlock();
  return is_bonded;
}

void pbl_bt_store_device_name_kernelbg_cb(void *ctx) {
  struct pbl_bt_device_internal identity;
  char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  const bool is_bonded = prv_get_identity_and_name_from_address_safe(ctx, &identity, device_name);
  kernel_free(ctx);

  if (!is_bonded) {
    return;
  }

  // Can't access flash when bt_lock() is held...
  bt_settings_set_peer_name(&identity, device_name);

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
