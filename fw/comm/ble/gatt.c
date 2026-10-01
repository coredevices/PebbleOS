/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/gatt.h>

#include <pbl/logging/logging.h>
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gatt_service_changed.h"
#include "comm/bt_lock.h"
#include "kernel/events.h"

#include <pbl/bluetooth/pebble_pairing_service.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

//! @see comment in gatt_client_subscriptions.c
extern void gatt_client_subscriptions_handle_server_notification(GAPLEConnection *connection,
                                                                 uint16_t att_handle,
                                                                 const uint8_t *att_value,
                                                                 uint16_t att_length);

extern PebbleTaskBitset gap_le_connect_task_mask_for_connection(const GAPLEConnection *connection);

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

void pbl_bt_cb_gatt_handle_notification(const struct pbl_bt_gatt_server_notif_indic_event *event) {
  GAPLEConnection *connection = NULL;
  bt_lock();
  {
    connection = gap_le_connection_by_addr(&event->dev_address);
  }
  bt_unlock();

  if (connection == NULL) {
    return;
  }

  gatt_client_subscriptions_handle_server_notification(connection, event->attr_handle,
                                                       event->attr_val, event->attr_val_len);
  PBL_LOG_VERBOSE("GATT Server Notification for handle %u " PBL_BT_ADDR_FMT, event->attr_handle,
                  PBL_BT_ADDR_XPLODE(event->dev_address));
}

void pbl_bt_cb_gatt_handle_indication(const struct pbl_bt_gatt_server_notif_indic_event *event) {
  GAPLEConnection *connection = NULL;
  bool done = false;
  bt_lock();
  {
    connection = gap_le_connection_by_addr(&event->dev_address);

    PBL_LOG_VERBOSE("GATT Server Indication for handle %u " PBL_BT_ADDR_FMT, event->attr_handle,
                    PBL_BT_ADDR_XPLODE(event->dev_address));

    // We are done if we got disconnected in the meantime or if this is a Service Changed indication
    // consumed by gatt_service_changed.c
    done = (connection == NULL) ||
           gatt_service_changed_client_handle_indication(connection, event->attr_handle,
                                                         event->attr_val, event->attr_val_len);
  }
  bt_unlock();

  if (done) {
    return;
  }

  gatt_client_subscriptions_handle_server_notification(connection, event->attr_handle,
                                                       event->attr_val, event->attr_val_len);
}

void pbl_bt_cb_gatt_handle_buffer_empty(const struct pbl_bt_gatt_device_buffer_empty_event *event) {
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
