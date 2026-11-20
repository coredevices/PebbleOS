/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_connect.h"

#include "gap_le_connect_params.h"
#include "gap_le_connection.h"
#include "gap_le_device_name.h"

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>

#include <comm/bluetooth_analytics.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <kernel/events.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

static bool s_is_connected_as_slave;

// Only used by the Settings/Bluetooth UI to refresh the list.
static void prv_put_legacy_connection_event(const struct pbl_bt_device_internal *device,
                                            bool is_connected) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .bluetooth = {
      .connection = {
        .is_ble = true,
        .device = *device,
        .state = is_connected ? PebbleBluetoothConnectionEventStateConnected
                              : PebbleBluetoothConnectionEventStateDisconnected,
      },
    },
  };
  event_put(&event);
}

static void prv_put_gateway_event(const struct pbl_bt_device_internal *device, uint8_t hci_reason,
                                  bool connected) {
  PebbleEvent pebble_event = {
    .type = PEBBLE_BLE_CONNECTION_EVENT,
    .task_mask = (PebbleTaskBitset) ~(1u << PebbleTask_KernelMain),
    .bluetooth = {
      .le = {
        .connection = {
          .bt_device_bits = device->opaque.opaque_64,
          .hci_reason = hci_reason,
          .connected = connected,
        },
      },
    },
  };
  event_put(&pebble_event);
}

// The gateway is the bonded phone, once its link is encrypted: only then can its services be used.
// bt_lock() must be held.
static void prv_update_gateway(GAPLEConnection *connection) {
  if (connection->is_gateway || !connection->is_bonded || !connection->is_encrypted) {
    return;
  }

  connection->is_gateway = true;
  prv_put_gateway_event(&connection->device, PBL_BT_HCI_STATUS_SUCCESS, true /* connected */);
}

void pbl_bt_handle_le_connection_handle_update_address(const struct pbl_bt_addr_change *e) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(&e->device);
    if (!connection) {
      PBL_LOG_ERR(
          "Got address update for non-existent connection. "
          "Old addr:" PBL_BT_ADDR_FMT,
          PBL_BT_ADDR_XPLODE(e->device.address));
      goto unlock;
    }

    connection->device = e->new_device;
    PBL_LOG_INFO("Updated address to " PBL_BT_ADDR_FMT,
                 PBL_BT_ADDR_XPLODE(connection->device.address));
  }
unlock:
  bt_unlock();
}

void pbl_bt_handle_le_connection_handle_update_irk(const struct pbl_bt_irk_change *e) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(&e->device);
    if (!connection) {
      PBL_LOG_ERR("Got IRK update for non-existent connection");
      goto unlock;
    }

    if (connection->irk) {
      PBL_LOG_WRN("Connection already has IRK!?");
    }

    gap_le_connection_set_irk(connection, e->irk_valid ? &e->irk : nullptr);
  }
unlock:
  bt_unlock();
}

void pbl_bt_handle_peer_version_info_event(
    const struct pbl_bt_remote_version_info_received_event *e) {
  bt_lock();

  GAPLEConnection *connection = gap_le_connection_by_device(&e->peer_address);
  if (connection) {
    const struct pbl_bt_remote_version_info *info = &e->remote_version_info;
    connection->remote_version_info = *info;
    PBL_LOG_DBG("Remote Vers Info: VersNr: %d, CompId: 0x%x, SubVersNr: 0x%x",
                (int)info->version_number, (int)info->company_identifier,
                (int)info->subversion_number);
  }

  bt_unlock();
}

//! bt_lock is assumed to be taken before calling this function.

void pbl_bt_handle_le_connection_complete_event(const struct pbl_bt_conn_complete_event *event) {
  bt_lock();
  const struct pbl_bt_conn_params *params = &event->conn_params;
  PBL_LOG_INFO("LE Conn Compl: addr=" PBL_BT_ADDR_FMT ", is_random_addr=%u,",
               PBL_BT_ADDR_XPLODE(event->peer_address.address),
               event->peer_address.is_random_address);
  PBL_LOG_INFO(
      "               hdl=%u, status=0x%02x, master=%u, %u, slave lat=%u, "
      "supervision timeout=%u, is_resolved=%c",
      event->handle, event->status, event->is_master, params->conn_interval_1_25ms,
      params->slave_latency_events, params->supervision_timeout_10ms,
      event->is_resolved ? 'Y' : 'N');

  if (event->status != PBL_BT_HCI_STATUS_SUCCESS) {
    PBL_LOG_ERR("Connection Complete Event status: 0x%x", event->status);
    goto unlock;
  }

  s_is_connected_as_slave = true;
  bt_advertising_handle_connect();
  prv_put_legacy_connection_event(&event->peer_address, true /* connected */);

  if (gap_le_connection_is_connected(&event->peer_address)) {
    PBL_LOG_ERR("Not adding connection for device. It is already connected .. disconnecting");
    pbl_bt_gap_le_disconnect(&event->peer_address);
    goto unlock;
  }

  const struct pbl_bt_sm_key *remote_irk = event->is_resolved ? &event->irk : nullptr;
  GAPLEConnection *connection =
      gap_le_connection_add(&event->peer_address, remote_irk, event->is_master);
  connection->conn_handle = event->handle;
  connection->conn_params = *params;
  connection->gatt_mtu = event->mtu;

  bluetooth_analytics_handle_connect(&event->peer_address, &event->conn_params);

  // The stack reports a bonded peer by its identity address.
  if (pbl_bt_bond_get(&event->peer_address, nullptr) == 0) {
    connection->is_bonded = true;
    connection->identity = event->peer_address;
  } else {
    PBL_LOG_WRN("No bond for connection");
    bluetooth_analytics_handle_no_intent_for_connection();
  }

#ifdef CONFIG_RECOVERY_FW
  conn_mgr_set_ble_conn_response_time(connection, PBL_BT_CONSUMER_PRF, PBL_BT_RESPONSE_TIME_MIN,
                                      MAX_PERIOD_RUN_FOREVER);
#endif

unlock:
  bt_unlock();
}

void pbl_bt_handle_le_disconnection_complete_event(
    const struct pbl_bt_disconn_complete_event *event) {
  bt_lock();

  if (event->status != PBL_BT_HCI_STATUS_SUCCESS) {
    PBL_LOG_ERR("Disconnection Complete Event status: 0x%x", event->status);
    goto unlock;
  }

  GAPLEConnection *connection = gap_le_connection_by_device(&event->peer_address);
  if (!connection) {
    PBL_LOG_DBG("LE Disconn: no connection for hdl=%u", event->handle);
    goto unlock;
  }

  PBL_LOG_INFO("LE Disconn: addr=" PBL_BT_ADDR_FMT ", is_random_addr=%u,",
               PBL_BT_ADDR_XPLODE(event->peer_address.address),
               event->peer_address.is_random_address);
  PBL_LOG_INFO("            hdl=%u, status=0x%02x, reason=0x%02x", event->handle, event->status,
               event->reason);

  bluetooth_analytics_handle_disconnect(connection->local_is_master);
  bluetooth_analytics_handle_connection_disconnection_event(event->reason,
                                                            &connection->remote_version_info);

  s_is_connected_as_slave = false;
  bt_advertising_handle_disconnect();
  prv_put_legacy_connection_event(&event->peer_address, false /* disconnected */);

  if (connection->is_gateway) {
    prv_put_gateway_event(&connection->device, event->reason, false /* connected */);
  }

  gap_le_connection_remove(&event->peer_address);

unlock:
  bt_unlock();
}

void pbl_bt_handle_le_encryption_change_event(const struct pbl_bt_encryption_change *event) {
  bt_lock();
  if (!event->encryption_enabled) {
    PBL_LOG_ERR("LE encryption change: failed (%u)", event->status);
    goto unlock;
  }

  GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
  if (!connection) {
    goto unlock;
  }
  if (connection->is_encrypted) {
    PBL_LOG_INFO("LE encryption change: refreshed");
    goto unlock;
  }

  PBL_LOG_DBG("LE encryption change: encrypted");
  connection->is_encrypted = true;
  bluetooth_analytics_handle_encryption_change();
#ifdef CONFIG_BT_SERVICE_PPS
  pbl_bt_pps_handle_status_change(connection);
#endif

  prv_update_gateway(connection);
unlock:
  bt_unlock();
}

void gap_le_connect_handle_bond_added(const struct pbl_bt_bond_info *info) {
  bt_lock();
  GAPLEConnection *connection = gap_le_connection_by_addr(&info->addr.address);
  if (connection == nullptr && info->irk_valid) {
    connection = gap_le_connection_find_by_irk(&info->irk);
  }
  if (connection != nullptr) {
    connection->is_bonded = true;
    connection->identity = info->addr;
    gap_le_device_name_request(&connection->device);
    prv_update_gateway(connection);
  } else {
    PBL_LOG_ERR("Couldn't find connection for bond!");
  }
  bt_unlock();
}

static bool prv_is_gateway_with_identity(GAPLEConnection *connection, void *data) {
  const struct pbl_bt_device_internal *addr = data;
  return connection->is_gateway && bt_device_internal_equal(&connection->identity, addr);
}

void gap_le_connect_handle_bond_deleted(const struct pbl_bt_device_internal *addr) {
  bt_lock();
  GAPLEConnection *connection = gap_le_connection_find(prv_is_gateway_with_identity, (void *)addr);
  if (connection) {
    const int result = pbl_bt_gap_le_disconnect(&connection->device);
    if (result != 0) {
      PBL_LOG_ERR("Ble disconnect failed: %d", result);
    }

    // The disconnection that follows is no longer the gateway's.
    connection->is_gateway = false;
    connection->is_bonded = false;
    prv_put_gateway_event(&connection->device, GAPLEConnectHCIReasonExtensionUserRemovedBonding,
                          false /* connected */);
  }
  bt_unlock();
}

bool gap_le_connect_is_connected_as_slave(void) {
  bool connected;
  bt_lock();
  {
    connected = s_is_connected_as_slave;
  }
  bt_unlock();
  return connected;
}

static void prv_disconnect_gateway(GAPLEConnection *connection, void *data) {
  if (connection->is_gateway) {
    connection->is_gateway = false;
    pbl_bt_gap_le_disconnect(&connection->device);
  }
}

void gap_le_connect_deinit(void) {
  bt_lock();
  {
    // The clients of the gateway are torn down by their owner: no event.
    gap_le_connection_for_each(prv_disconnect_gateway, nullptr);

    if (s_is_connected_as_slave) {
      bluetooth_analytics_handle_disconnect(false);
      s_is_connected_as_slave = false;
    }
  }
  bt_unlock();
}
