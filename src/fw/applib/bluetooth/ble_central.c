/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_central.h"

#include "process_state/app_state/app_state.h"

#include "comm/ble/gap_le_connect.h"

static BTErrno prv_bt_errno_for_event(const PebbleBLEConnectionEvent *e) {
  if (e->connected) {
    return BTErrnoConnected;
  }

  // FIXME: PBL-35506 We need to re-evaluate what error code to actually use here
  return e->hci_reason;
}

void ble_central_handle_event(PebbleEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  if (!ble_app_state->connection_handler) {
    return;
  }
  const PebbleBLEConnectionEvent *conn_event = &e->bluetooth.le.connection;
  const BTDeviceInternal device = PebbleEventToBTDeviceInternal(conn_event);
  ble_app_state->connection_handler(device.opaque,
                                    prv_bt_errno_for_event(conn_event));
}

BTErrno ble_central_set_connection_handler(BLEConnectionHandler handler) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  const bool is_subscribed = (ble_app_state->connection_handler != NULL);
  ble_app_state->connection_handler = handler;
  if (handler) {
    if (!is_subscribed) {
      event_service_client_subscribe(&ble_app_state->connection_service_info);
    }
  } else {
    if (is_subscribed) {
      event_service_client_unsubscribe(&ble_app_state->connection_service_info);
    }
  }
  return BTErrnoOK;
}
