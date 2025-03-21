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
#include <comm/ble/gap_le_connection.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <system/logging.h>
#include <system/passert.h>

#include "nimble_type_conversions.h"

static int pebble_pairing_service_get_connectivity_status(
    uint16_t conn_handle, PebblePairingServiceConnectivityStatus *status) {
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    PBL_LOG_D(
        LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
        "Failed to find connection descriptor for %d when reading connection status, code: %d",
        conn_handle, rc);
    return -1;
  }

  memset(status, 0, sizeof(*status));
  status->ble_is_connected = true;
  status->ble_is_bonded = desc.sec_state.bonded;
  status->ble_is_encrypted = desc.sec_state.encrypted;

  return 0;
}

int pebble_pairing_service_get_connectivity_send_notification(uint16_t conn_handle,
                                                              uint16_t attr_handle) {
  PebblePairingServiceConnectivityStatus status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "pebble_pairing_service_get_connectivity_status failed: %d", rc);
    return rc;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(&status, sizeof(status));
  rc = ble_gatts_notify_custom(conn_handle, attr_handle, om);
  PBL_LOG(LOG_LEVEL_INFO, "ble_gatts_notify for attr %d returned %d", attr_handle, rc);
  return rc;
}

static int prv_access_connection_status(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return 0;

  PebblePairingServiceConnectivityStatus status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "prv_access_connection_status failed: %d", rc);
    return 0;
  }

  os_mbuf_append(ctxt->om, &status, sizeof(status));
  return 0;
}

static int prv_access_trigger_pairing(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt, void *arg) {
  int rc = ble_gap_security_initiate(conn_handle);
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "ble_gap_security_initiate rc=%d", rc);
  return rc;
}

static int prv_access_gatt_mtu(uint16_t conn_handle, uint16_t attr_handle,
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
                    .uuid = BLE_UUID128_DECLARE(
                        BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_STATUS_UUID)),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .access_cb = prv_access_connection_status,
                },
                {
                    .uuid = BLE_UUID128_DECLARE(
                        BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_TRIGGER_PAIRING_UUID)),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                    .access_cb = prv_access_trigger_pairing,
                },
                {
                    .uuid = BLE_UUID128_DECLARE(
                        BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_GATT_MTU_UUID)),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .access_cb = prv_access_gatt_mtu,
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

void prv_notify_chr_updated(const GAPLEConnection *connection, const ble_uuid_t *chr_uuid) {
  int rc;

  uint16_t conn_handle;
  rc = pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_notify_chr_updated: failed to find connection handle");
    return;
  }

  uint16_t attr_handle;
  rc = ble_gatts_find_chr(pebble_pairing_svc[0].uuid, chr_uuid, NULL, &attr_handle);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR,
              "prv_notify_chr_updated: failed to find characteristic handle");
    return;
  }
  pebble_pairing_service_get_connectivity_send_notification(conn_handle, attr_handle);
}

void bt_driver_pebble_pairing_service_handle_status_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(
      connection,
      BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_STATUS_UUID)));
}

void bt_driver_pebble_pairing_service_handle_gatt_mtu_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(
      connection, BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_GATT_MTU_UUID)));
}
