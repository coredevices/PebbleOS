/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pairability_priv.h"

#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/clients/gap.h>
#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/bluetooth/pairing_confirm.h>
#include <pbl/bluetooth/types.h>
#include <pbl/logging/logging.h>

#include <host/ble_gap.h>
#include <host/ble_hs.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

static char s_peer_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
static uint16_t s_pairing_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static struct ble_gap_event_listener s_pairing_listener;

static bool prv_is_bonded(const struct ble_gap_conn_desc *desc) {
  const struct ble_store_key_sec key_sec = {.peer_addr = desc->peer_id_addr};
  struct ble_store_value_sec value_sec;

  return ble_store_read_peer_sec(&key_sec, &value_sec) == 0;
}

static void prv_peer_name_cb(uint16_t conn_handle, const char *name) {
  strncpy(s_peer_name, name, sizeof(s_peer_name) - 1);
}

static void prv_handle_connect(const struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;

  if (event->connect.status != 0 || ble_gap_conn_find(event->connect.conn_handle, &desc) != 0) {
    return;
  }

  /* A peer with no bond is to pair: its name is shown when confirming. */
  memset(s_peer_name, 0, sizeof(s_peer_name));
  if (!prv_is_bonded(&desc)) {
    pbl_bt_gapc_read_name(event->connect.conn_handle, prv_peer_name_cb);
  }
}

static void prv_end_pairing(uint16_t conn_handle, bool success) {
  if (conn_handle != s_pairing_conn_handle) {
    return;
  }

  s_pairing_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  pbl_bt_cb_pairing_confirm_handle_completed(conn_handle, success);
}

/* A link dropped while pairing raises no pairing complete event. */
static int prv_pairing_gap_event(struct ble_gap_event *event, void *arg) {
  if (event->type == BLE_GAP_EVENT_DISCONNECT) {
    prv_end_pairing(event->disconnect.conn.conn_handle, false);
  }

  return 0;
}

static void prv_handle_passkey(const struct ble_gap_event *event) {
  char passkey[7];

  /* Listeners run newest first: the pairing ends before anyone else handles the disconnection. */
  (void)ble_gap_event_listener_unregister(&s_pairing_listener);
  (void)ble_gap_event_listener_register(&s_pairing_listener, prv_pairing_gap_event, nullptr);

  snprintf(
      passkey, sizeof(passkey), "%06" PRIu32,
      (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) ? event->passkey.params.numcmp : 0);
  s_pairing_conn_handle = event->passkey.conn_handle;
  pbl_bt_cb_pairing_confirm_handle_request(
      event->passkey.conn_handle, (s_peer_name[0] != '\0') ? s_peer_name : nullptr, passkey);
}

static int prv_handle_pairing_request(const struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;

  if (pbl_bt__pairability_is_enabled()) {
    return 0;
  }

  if (ble_gap_conn_find(event->pairing_request.conn_handle, &desc) != 0) {
    return BLE_SM_ERR_UNSPECIFIED;
  }

  /* Bonded peers are handled by the repeat pairing policy. */
  if (prv_is_bonded(&desc)) {
    return 0;
  }

  PBL_LOG_INFO("Pairing request rejected: not pairable");
  return BLE_SM_ERR_PAIR_NOT_SUPP;
}

static int prv_handle_repeat_pairing(const struct ble_gap_event *event) {
#if MYNEWT_VAL(BLE_SM_SC_ONLY) && (MYNEWT_VAL(BLE_SM_IO_CAP) == BLE_HS_IO_DISPLAY_YESNO)
  struct ble_gap_conn_desc desc;
  int rc;

  rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
  if (rc != 0) {
    return rc;
  }

  PBL_LOG_INFO("Repeat pairing: deleting stored peer keys and retrying");
  ble_store_util_delete_peer(&desc.peer_id_addr);

  return BLE_GAP_REPEAT_PAIRING_RETRY;
#else
  PBL_LOG_WRN("Repeat pairing ignored");
  return BLE_GAP_REPEAT_PAIRING_IGNORE;
#endif
}

int pbl_bt_gap_event_handler(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      prv_handle_connect(event);
      break;
    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
      *event->conn_update_req.self_params = *event->conn_update_req.peer_params;
      break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      prv_handle_passkey(event);
      break;
    case BLE_GAP_EVENT_PAIRING_COMPLETE:
      PBL_LOG_INFO("Pairing complete: status=0x%04x", (uint16_t)event->pairing_complete.status);
      prv_end_pairing(event->pairing_complete.conn_handle, event->pairing_complete.status == 0);
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      (void)pbl_bt_gattc_notify_rx(event->notify_rx.conn_handle, event->notify_rx.attr_handle,
                                   event->notify_rx.om);
      break;
    case BLE_GAP_EVENT_PAIRING_REQUEST:
      return prv_handle_pairing_request(event);
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      return prv_handle_repeat_pairing(event);
    default:
      break;
  }

  return 0;
}
