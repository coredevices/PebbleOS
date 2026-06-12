/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/pebble_pairing_service.h>
#include <comm/ble/gap_le_connection.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>
#include <syscfg/syscfg.h>
#include <system/logging.h>
#include <system/passert.h>

#include "nimble_pebble_pairing_service.h"
#include "nimble_type_conversions.h"

#define TRIGGER_PAIRING_NO_SEC_REQ    (1U << 1U)
#define TRIGGER_PAIRING_FORCE_SEC_REQ (1U << 2U)

typedef struct {
  bool is_used;
  uint16_t conn_handle;
} GatewayCandidate;

static GatewayCandidate s_gateway_candidates[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];

static void prv_note_gateway_candidate(uint16_t conn_handle) {
  GatewayCandidate *free_candidate = NULL;
  for (size_t i = 0; i < MYNEWT_VAL(BLE_MAX_CONNECTIONS); ++i) {
    GatewayCandidate *candidate = &s_gateway_candidates[i];
    if (candidate->is_used && candidate->conn_handle == conn_handle) {
      return;
    }
    if (!candidate->is_used && !free_candidate) {
      free_candidate = candidate;
    }
  }

  if (free_candidate) {
    *free_candidate = (GatewayCandidate){
        .is_used = true,
        .conn_handle = conn_handle,
    };
  }
}

static bool prv_is_gateway_candidate(uint16_t conn_handle) {
  for (size_t i = 0; i < MYNEWT_VAL(BLE_MAX_CONNECTIONS); ++i) {
    const GatewayCandidate *candidate = &s_gateway_candidates[i];
    if (candidate->is_used && candidate->conn_handle == conn_handle) {
      return true;
    }
  }
  return false;
}

void nimble_pebble_pairing_service_handle_disconnect(uint16_t conn_handle) {
  for (size_t i = 0; i < MYNEWT_VAL(BLE_MAX_CONNECTIONS); ++i) {
    GatewayCandidate *candidate = &s_gateway_candidates[i];
    if (candidate->is_used && candidate->conn_handle == conn_handle) {
      *candidate = (GatewayCandidate){};
      return;
    }
  }
}

bool nimble_pebble_pairing_service_peer_is_gateway(const ble_addr_t *peer_addr) {
  struct ble_gap_conn_desc desc;
  const int rc = ble_gap_conn_find_by_addr(peer_addr, &desc);
  return (rc == 0 && prv_is_gateway_candidate(desc.conn_handle));
}

static int pebble_pairing_service_get_connectivity_status(
    uint16_t conn_handle, PebblePairingServiceConnectivityStatus *status) {
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    PBL_LOG_D_ERR(
        LOG_DOMAIN_BT, "Failed to find connection descriptor for %d when reading connection status, code: %d",
        conn_handle, rc);
    return -1;
  }

  struct ble_store_key_sec key_sec = {
    .peer_addr = desc.peer_id_addr,
  };
  struct ble_store_value_sec value_sec;
  bool is_bonded = (ble_store_read_peer_sec(&key_sec, &value_sec) == 0);

  memset(status, 0, sizeof(*status));
  status->ble_is_connected = true;
  status->ble_is_bonded = is_bonded;
  status->ble_is_encrypted = desc.sec_state.encrypted;
  status->has_bonded_gateway = (bt_persistent_storage_has_active_ble_gateway_bonding() ||
                                bt_persistent_storage_has_ble_ancs_bonding());
  status->supports_pinning_without_security_request = true;

  return 0;
}

int pebble_pairing_service_get_connectivity_send_notification(uint16_t conn_handle,
                                                              uint16_t attr_handle) {
  PebblePairingServiceConnectivityStatus status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT, "pebble_pairing_service_get_connectivity_status failed: %d", rc);
    return rc;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(&status, sizeof(status));
  rc = ble_gatts_notify_custom(conn_handle, attr_handle, om);
  if (rc != 0) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT, "ble_gatts_notify_custom failed for attr %d: 0x%04x", attr_handle, (uint16_t)rc);
    return rc;
  }

  return 0;
}

static int prv_access_connection_status(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return 0;

  PebblePairingServiceConnectivityStatus status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT, "prv_access_connection_status failed: %d", rc);
    return 0;
  }

  os_mbuf_append(ctxt->om, &status, sizeof(status));
  return 0;
}

static int prv_access_trigger_pairing(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt, void *arg) {
  int rc;
  struct ble_gap_conn_desc desc;

  rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    return rc;
  }

  prv_note_gateway_candidate(conn_handle);

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && !desc.sec_state.encrypted) {
    rc = ble_gap_security_initiate(conn_handle);
    if (rc != 0) {
      return rc;
    }
  } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint8_t flags;

    rc = ble_hs_mbuf_to_flat(ctxt->om, &flags, sizeof(flags), NULL);
    if (rc != 0) {
      return rc;
    }

    PBL_LOG_D_INFO(LOG_DOMAIN_BT, "Trigger pairing flags 0x%x", flags);

    if ((((flags & TRIGGER_PAIRING_NO_SEC_REQ) == 0U) && !desc.sec_state.encrypted) ||
        ((flags & TRIGGER_PAIRING_FORCE_SEC_REQ) != 0U)) {
      rc = ble_gap_security_initiate(conn_handle);
      if (rc != 0) {
        return rc;
      }
    }
  } else {
    return BLE_ATT_ERR_UNLIKELY;
  }

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

  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT, "prv_notify_chr_updated: failed to find connection handle");
    return;
  }

  uint16_t attr_handle;
  rc = ble_gatts_find_chr(pebble_pairing_svc[0].uuid, chr_uuid, NULL, &attr_handle);
  if (rc != 0) {
    PBL_LOG_D_ERR(LOG_DOMAIN_BT, "prv_notify_chr_updated: failed to find characteristic handle");
    return;
  }
  pebble_pairing_service_get_connectivity_send_notification(conn_handle, attr_handle);
}

void bt_driver_pebble_pairing_service_handle_status_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(
      connection,
      BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PEBBLE_BT_PAIRING_SERVICE_CONNECTION_STATUS_UUID)));
}
