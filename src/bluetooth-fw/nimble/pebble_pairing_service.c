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

#include <bluetooth/pebble_pairing_service.h>
#include <system/logging.h>
#include <system/passert.h>

#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>

static int prv_access_connection_status(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(conn_handle, &desc) != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to find connection descriptor when reading connection status");
    return -1;
  }

  PebblePairingServiceConnectivityStatus status = {
      .is_reversed_ppogatt_enabled = false,
      .ble_is_connected = true,
      .supports_pinning_without_security_request = false,
      .ble_is_bonded = desc.sec_state.bonded,
      .ble_is_encrypted = desc.sec_state.encrypted,
  };
  os_mbuf_append(ctxt->om, &status, sizeof(status));
  return 0;
}

static int prv_access_trigger_pairing(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt, void *arg) {
  int rc = 0;
  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
      break;
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
      rc = ble_gap_security_initiate(conn_handle);
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "security_init rc=%d", rc);
      break;
  }
  return rc;
}

static int prv_access_gatt_mtu(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // TODO: implement
  return 0;
}

static int prv_access_connection_params(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // TODO: implement
  return 0;
}

static const struct ble_gatt_svc_def pebble_pairing_svc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(PEBBLE_BT_PAIRING_SERVICE_UUID_16BIT),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_STATUS_UUID),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .access_cb = prv_access_connection_status,
                },
                {
                    .uuid = BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_TRIGGER_PAIRING_UUID),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                    .access_cb = prv_access_trigger_pairing,
                },
                {
                    .uuid = BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_GATT_MTU_UUID),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .access_cb = prv_access_gatt_mtu,
                },
                {
                    .uuid =
                        BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_PARAMETERS_UUID),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .access_cb = prv_access_connection_params,
                },
                {
                    0, /* No more characteristics in this service */
                },
            },
    },
    {
        0, /* No more services */
    },
};

void pebble_pairing_service_init(void) {
  int rc;

  rc = ble_gatts_count_cfg(pebble_pairing_svc);
  PBL_ASSERTN(rc == 0);
  rc = ble_gatts_add_svcs(pebble_pairing_svc);
  PBL_ASSERTN(rc == 0);
}

void prv_notify_chr_updated(const ble_uuid_t *chr_uuid) {
  uint16_t chr_val_handle;
  int rc = ble_gatts_find_chr(pebble_pairing_svc[0].uuid, chr_uuid, NULL, &chr_val_handle);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "prv_notify_chr_updated: Failed to find characteristic handle");
    return;
  }
  ble_gatts_chr_updated(chr_val_handle);
}

void bt_driver_pebble_pairing_service_handle_status_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_STATUS_UUID));
}

void bt_driver_pebble_pairing_service_handle_gatt_mtu_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(BLE_UUID128_DECLARE(PEBBLE_BT_PAIRING_SERVICE_GATT_MTU_UUID));
}
