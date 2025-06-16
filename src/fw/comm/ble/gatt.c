/*
 * Copyright 2024 Google LLC
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

#include <bluetooth/gatt.h>

#include "comm/ble/ble_log.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gatt_service_changed.h"
#include "comm/bt_lock.h"
#include "kernel/events.h"

#include <bluetooth/pebble_pairing_service.h>

//! @see comment in gatt_client_subscriptions.c
extern void gatt_client_subscriptions_handle_server_notification(GAPLEConnection *connection,
                                                                 uint16_t att_handle,
                                                                 const uint8_t *att_value,
                                                                 uint16_t att_length);

extern PebbleTaskBitset gap_le_connect_task_mask_for_connection(const GAPLEConnection *connection);

void bt_driver_cb_gatt_handle_connect(const GattDeviceConnectionEvent *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }
    connection->gatt_connection_id = event->connection_id;
    connection->gatt_mtu = event->mtu;
    BLE_LOG_DEBUG("GATT Connection for " BT_DEVICE_ADDRESS_FMT,
                  BT_DEVICE_ADDRESS_XPLODE(event->dev_address));
  }
unlock:
  bt_unlock();
}

void bt_driver_cb_gatt_handle_disconnect(const GattDeviceDisconnectionEvent *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }
    connection->gatt_connection_id = 0;
    connection->gatt_mtu = 0;
    BLE_LOG_DEBUG("GATT Disconnection for " BT_DEVICE_ADDRESS_FMT,
                  BT_DEVICE_ADDRESS_XPLODE(event->dev_address));
  }
unlock:
  bt_unlock();
}

void bt_driver_cb_gatt_handle_mtu_update(const GattDeviceMtuUpdateEvent *event) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }
    
    PBL_LOG(LOG_LEVEL_INFO, "Handle MTU change from %d to %d bytes",
            connection->gatt_mtu, event->mtu);
    connection->gatt_mtu = event->mtu;
  }
unlock:
  bt_unlock();
}

void bt_driver_cb_gatt_handle_notification(const GattServerNotifIndicEvent *event) {
  GAPLEConnection *connection = NULL;
  bt_lock();
  {
    connection = gap_le_connection_by_addr(&event->dev_address);
  }
  bt_unlock();

  if (connection == NULL) {
    return;
  }

  gatt_client_subscriptions_handle_server_notification(connection,
                                                       event->attr_handle,
                                                       event->attr_val,
                                                       event->attr_val_len);
  BLE_LOG_DEBUG("GATT Server Notification for handle %u " BT_DEVICE_ADDRESS_FMT,
                event->attr_handle, BT_DEVICE_ADDRESS_XPLODE(event->dev_address));
}

void bt_driver_cb_gatt_handle_indication(const GattServerNotifIndicEvent *event) {
  GAPLEConnection *connection = NULL;
  bool done = false;
  bt_lock();
  {
    connection = gap_le_connection_by_addr(&event->dev_address);

    BLE_LOG_DEBUG("GATT Server Indication for handle %u " BT_DEVICE_ADDRESS_FMT,
                  event->attr_handle,
                  BT_DEVICE_ADDRESS_XPLODE(event->dev_address));

    // We are done if we got disconnected in the meantime or if this is a Service Changed indication
    // consumed by gatt_service_changed.c
    done = (connection == NULL) || gatt_service_changed_client_handle_indication(
        connection, event->attr_handle, event->attr_val, event->attr_val_len);
  }
  bt_unlock();

  if (done) {
    return;
  }

  gatt_client_subscriptions_handle_server_notification(
      connection, event->attr_handle, event->attr_val, event->attr_val_len);
}

void bt_driver_cb_gatt_handle_buffer_empty(const GattDeviceBufferEmptyEvent *event) {
  bt_lock();
  {
    const GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
    if (!connection) {
      goto unlock;
    }

    PebbleTaskBitset task_mask = gap_le_connect_task_mask_for_connection(connection);

    PebbleEvent e = {
      .type = PEBBLE_BLE_GATT_CLIENT_EVENT,
      .task_mask = task_mask,
      .bluetooth = {
        .le = {
          .gatt_client = {
            .subtype = PebbleBLEGATTClientEventTypeBufferEmpty,
          },
        },
      },
    };
    event_put(&e);
  }
unlock:
  bt_unlock();
}
