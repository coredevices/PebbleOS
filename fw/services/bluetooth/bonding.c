/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bond.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/bluetooth/pairability.h>

#include <comm/ble/gap_le_connect.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/ble/gap_le_device_name.h>
#include <comm/ble/kernel_le_client/kernel_le_client.h>
#include <comm/bt_lock.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

void pbl_bt_cb_bond_added(const struct pbl_bt_bond_info *info) {
  // In practice only a single bond is kept, so a new one means a new phone.
  bt_settings_set_unfaithful(true);

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  bt_lock();
  GAPLEConnection *connection = gap_le_connection_by_addr(&info->addr.address);
  if (connection) {
    connection->is_bonded = true;
    connection->identity = info->addr;
    connection->is_gateway = true;
    gap_le_device_name_request(&connection->device);
  } else {
    PBL_LOG_ERR("Couldn't find connection for bond!");
  }
  bt_unlock();

  kernel_le_client_handle_bond_added(info);
  bt_pairability_update_due_to_bonding_change();
}

void pbl_bt_cb_bond_deleted(const struct pbl_bt_device_internal *addr) {
  bt_settings_delete_peer_name(addr);

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  gap_le_connection_handle_bond_deleted(addr);
  gap_le_connect_handle_bond_deleted(addr);
  kernel_le_client_handle_bond_deleted(addr);
  bt_pairability_update_due_to_bonding_change();
}
