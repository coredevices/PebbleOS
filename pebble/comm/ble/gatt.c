/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>
#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/logging/logging.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_lock.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

void pbl_bt_cb_gatt_handle_connect(const struct pbl_bt_gatt_device_connection_event *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }
    connection->gatt_connection_id = event->connection_id;
    connection->gatt_mtu = event->mtu;
    PBL_LOG_DBG("GATT Connection for " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(event->dev_address));
  }
unlock:
  bt_unlock();
}

void pbl_bt_cb_gatt_handle_disconnect(const struct pbl_bt_gatt_device_disconnection_event *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }
    connection->gatt_connection_id = 0;
    connection->gatt_mtu = 0;
    PBL_LOG_DBG("GATT Disconnection for " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(event->dev_address));
  }
unlock:
  bt_unlock();
}

void pbl_bt_cb_gatt_handle_mtu_update(const struct pbl_bt_gatt_device_mtu_update_event *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }

    PBL_LOG_INFO("Handle MTU change from %d to %d bytes", connection->gatt_mtu, event->mtu);
    connection->gatt_mtu = event->mtu;
  }
unlock:
  bt_unlock();
}
