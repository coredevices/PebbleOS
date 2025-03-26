/*
 * Copyright 2025 Google LLC
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

#include <bluetooth/bonding_sync.h>
#include <bluetooth/bt_driver_advert.h>
#include <bluetooth/gatt.h>
#include <bluetooth/pairing_confirm.h>
#include <comm/bt_lock.h>
#include <host/ble_gap.h>
#include <host/ble_hs_hci.h>
#include <system/logging.h>
#include <system/passert.h>

#include "nimble_type_conversions.h"

void bt_driver_advert_advertising_disable(void) {
  int rc;

  if (ble_gap_adv_active() == 0) {
    return;
  }

  rc = ble_gap_adv_stop();
  PBL_ASSERT(rc == 0, "Failed to stop advertising (%d)", rc);
}

// no impl needed for nimble, buggy stack workaround
bool bt_driver_advert_is_connectable(void) { return true; }

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) { return false; }

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
  int rc;

  rc = ble_gap_adv_set_data((uint8_t *)&ad_data->data, ad_data->ad_data_length);
  PBL_ASSERT(rc == 0, "Failed to set advertising data (%d)", rc);

  rc = ble_gap_adv_rsp_set_data((uint8_t *)&ad_data->data[ad_data->ad_data_length],
                                ad_data->scan_resp_data_length);
  PBL_ASSERT(rc == 0, "Failed to set scan response data (%d)", rc);
}

static void prv_handle_connection_event(struct ble_gap_event *event) {
  // we only want to notify on a successful connection
  if (event->connect.status != 0) return;

  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->connect.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_connection_event: Failed to find connection descriptor");
    return;
  }

  struct BleConnectionCompleteEvent complete_event = {
      .handle = event->connect.conn_handle,
      .is_master = desc.role == BLE_GAP_ROLE_MASTER,
      .status = HciStatusCode_Success,
  };

  // If OTA address != ID address, then the address must be resolved.
  // This happens for an already paired devices.
  complete_event.is_resolved = ble_addr_cmp(&desc.peer_id_addr, &desc.peer_ota_addr) != 0;

  nimble_conn_params_to_pebble(&desc, &complete_event.conn_params);
  nimble_addr_to_pebble_device(&desc.peer_id_addr, &complete_event.peer_address);
  bt_driver_handle_le_connection_complete_event(&complete_event);
}

static void prv_handle_disconnection_event(struct ble_gap_event *event) {
  GattDeviceDisconnectionEvent gatt_event;
  nimble_addr_to_pebble_addr(&event->disconnect.conn.peer_id_addr, &gatt_event.dev_address);
  bt_driver_cb_gatt_handle_disconnect(&gatt_event);

  struct BleDisconnectionCompleteEvent disconnection_event = {
      .handle = event->disconnect.conn.conn_handle,
      .reason = event->disconnect.reason,
      .status = HciStatusCode_Success,
  };
  nimble_addr_to_pebble_device(&event->disconnect.conn.peer_id_addr,
                               &disconnection_event.peer_address);
  bt_driver_handle_le_disconnection_complete_event(&disconnection_event);
}

static void prv_handle_enc_change_event(struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_enc_change_event: Failed to find connection descriptor");
    return;
  }

  struct BleEncryptionChange enc_change_event = {
      .encryption_enabled = desc.sec_state.encrypted,
      .status =
          event->enc_change.status,  // doesn't technically match but only logged so this is fine
  };
  nimble_addr_to_pebble_addr(&desc.peer_id_addr, &enc_change_event.dev_address);
  bt_driver_handle_le_encryption_change_event(&enc_change_event);

  BleBonding bonding = {
      .is_gateway = true,
      .should_pin_address = false,
      .pairing_info =
          {
              .is_local_encryption_info_valid = false,
              .is_remote_encryption_info_valid = false,
              .is_remote_identity_info_valid = true,
              .is_remote_signing_info_valid = false,
              .is_mitm_protection_enabled = false,
          },
  };
  nimble_addr_to_pebble_device(&desc.peer_id_addr, &bonding.pairing_info.identity);
  bt_driver_cb_handle_create_bonding(&bonding, &enc_change_event.dev_address);
}

static void prv_handle_conn_params_updated_event(struct ble_gap_event *event) {
  if (event->conn_update.status != 0) return;

  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_conn_params_updated_event: Failed to find connection descriptor");
    return;
  }

  struct BleConnectionUpdateCompleteEvent conn_params_update_event = {
      .status = HciStatusCode_Success,
  };
  nimble_conn_params_to_pebble(&desc, &conn_params_update_event.conn_params);
  nimble_addr_to_pebble_addr(&desc.peer_id_addr, &conn_params_update_event.dev_address);
  bt_driver_handle_le_conn_params_update_event(&conn_params_update_event);
}

static void prv_handle_passkey_event(struct ble_gap_event *event) {
  char passkey_str[7];
  uint32_t passkey = 0;
  PairingUserConfirmationCtx *ctx =
      (PairingUserConfirmationCtx *)((uintptr_t)event->passkey.conn_handle);

  if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
    passkey = event->passkey.params.numcmp;
  }

  snprintf(passkey_str, sizeof(passkey_str), "%lu", passkey);
  // TODO: get device name
  bt_driver_cb_pairing_confirm_handle_request(ctx, passkey_str, NULL);
}

static void prv_handle_pairing_complete_event(struct ble_gap_event *event) {
  PairingUserConfirmationCtx *ctx =
      (PairingUserConfirmationCtx *)((uintptr_t)event->pairing_complete.conn_handle);
  bt_driver_cb_pairing_confirm_handle_completed(ctx, event->pairing_complete.status == 0);
}

static void prv_handle_identity_resolved_event(struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->identity_resolved.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_identity_resolved_event: Failed to find connection descriptor");
    return;
  }

  BleAddressChange addr_change_event;
  nimble_addr_to_pebble_device(&desc.peer_ota_addr, &addr_change_event.device);
  nimble_addr_to_pebble_device(&desc.peer_id_addr, &addr_change_event.new_device);
  bt_driver_handle_le_connection_handle_update_address(&addr_change_event);
}

static void prv_handle_mtu_change_event(struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->mtu.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_mtu_change_event: Failed to find connection descriptor");
    return;
  }

  GattDeviceMtuUpdateEvent mtu_update_event = {.mtu = event->mtu.value};
  nimble_addr_to_pebble_addr(&desc.peer_id_addr, &mtu_update_event.dev_address);
  bt_driver_cb_gatt_handle_mtu_update(&mtu_update_event);
}

extern int pebble_pairing_service_get_connectivity_send_notification(uint16_t conn_handle,
                                                                     uint16_t attr_handle);
static void prv_handle_subscription_event(struct ble_gap_event *event) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG,
            "prv_handle_subscription_event: connhandle: %d attr:%d notify:%d/%d indicate:%d/%d",
            event->subscribe.conn_handle, event->subscribe.attr_handle,
            event->subscribe.prev_notify, event->subscribe.cur_notify,
            event->subscribe.prev_indicate, event->subscribe.cur_indicate);

  int rc = pebble_pairing_service_get_connectivity_send_notification(event->subscribe.conn_handle,
                                                                     event->subscribe.attr_handle);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "pebble_pairing_service_get_connectivity_send_notification rc=%d", rc);
  }
}

static void prv_handle_notification_rx_event(struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(event->notify_rx.conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_handle_notification_rx_event: Failed to find connection descriptor");
    return;
  }

  GattServerNotifIndicEvent notification_event = {
      .attr_handle = event->notify_rx.attr_handle,
      .attr_val = event->notify_rx.om->om_data,
      .attr_val_len = event->notify_rx.om->om_len,
  };
  nimble_addr_to_pebble_addr(&desc.peer_id_addr, &notification_event.dev_address);

  if (event->notify_rx.indication == 1) {
    bt_driver_cb_gatt_handle_indication(&notification_event);
  } else {
    bt_driver_cb_gatt_handle_notification(&notification_event);
  }
}

static int prv_handle_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_CONNECT");
      prv_handle_connection_event(event);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_DISCONNECT");
      prv_handle_disconnection_event(event);
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_ENC_CHANGE");
      prv_handle_enc_change_event(event);
      break;
    case BLE_GAP_EVENT_CONN_UPDATE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_CONN_UPDATE");
      prv_handle_conn_params_updated_event(event);
      break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_PASSKEY_ACTION");
      prv_handle_passkey_event(event);
      break;
    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_IDENTITY_RESOLVED");
      prv_handle_identity_resolved_event(event);
      break;
    case BLE_GAP_EVENT_PAIRING_COMPLETE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_PAIRING_COMPLETE");
      prv_handle_pairing_complete_event(event);
      break;
    case BLE_GAP_EVENT_MTU:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_MTU");
      prv_handle_mtu_change_event(event);
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "BLE_GAP_EVENT_SUBSCRIBE");
      prv_handle_subscription_event(event);
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      // no log here because it's incredibly noisy
      prv_handle_notification_rx_event(event);
      break;
    default:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_WARNING, "Unhandled GAP event: %d", event->type);
      break;
  }
  return 0;
}

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                         bool enable_scan_resp) {
  int rc;
  uint8_t own_addr_type;
  struct ble_gap_adv_params advp = {
      .conn_mode = enable_scan_resp ? BLE_GAP_CONN_MODE_UND : BLE_GAP_DISC_MODE_NON,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
      .itvl_min = BLE_GAP_CONN_ITVL_MS(min_interval_ms),
      .itvl_max = BLE_GAP_CONN_ITVL_MS(max_interval_ms),
  };

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to infer own address type (%d)", rc);
    return false;
  }

  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &advp, prv_handle_gap_event, NULL);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to start advertising (%d)", rc);
    return false;
  }

  return true;
}

// no impl needed for nimble, buggy stack workaround
bool bt_driver_advert_client_has_cycled(void) { return false; }

// no impl needed for nimble, buggy stack workaround
void bt_driver_advert_client_set_cycled(bool has_cycled) {}

// no impl needed for nimble, buggy stack workaround
bool bt_driver_advert_should_not_cycle(void) { return false; }
