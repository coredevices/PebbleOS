/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/logging/logging.h>

#include <bluetooth/services/services.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define TRIGGER_PAIRING_NO_SEC_REQ    (1U << 1U)
#define TRIGGER_PAIRING_FORCE_SEC_REQ (1U << 2U)

static int pebble_pairing_service_get_connectivity_status(
    uint16_t conn_handle, struct pbl_bt_pps_connectivity_status *status) {
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    PBL_LOG_ERR(
        "Failed to find connection descriptor for %d when reading connection status, code: %d",
        conn_handle, rc);
    return -1;
  }

  struct ble_store_key_sec key_sec = {
    .peer_addr = desc.peer_id_addr,
  };
  struct ble_store_value_sec value_sec;
  bool is_bonded = (ble_store_read_peer_sec(&key_sec, &value_sec) == 0);

  int bond_count = 0;
  ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &bond_count);

  memset(status, 0, sizeof(*status));
  status->ble_is_connected = true;
  status->ble_is_bonded = is_bonded;
  status->ble_is_encrypted = desc.sec_state.encrypted;
  status->has_bonded_gateway = (bond_count > 0);
  status->supports_pinning_without_security_request = true;

  return 0;
}

static int prv_notify_connectivity_status(uint16_t conn_handle, uint16_t attr_handle) {
  struct pbl_bt_pps_connectivity_status status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_ERR("pebble_pairing_service_get_connectivity_status failed: %d", rc);
    return rc;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(&status, sizeof(status));
  rc = ble_gatts_notify_custom(conn_handle, attr_handle, om);
  if (rc != 0) {
    PBL_LOG_ERR("ble_gatts_notify_custom failed for attr %d: 0x%04x", attr_handle, (uint16_t)rc);
    return rc;
  }

  return 0;
}

static int prv_access_connection_status(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR)
    return 0;

  struct pbl_bt_pps_connectivity_status status;
  int rc = pebble_pairing_service_get_connectivity_status(conn_handle, &status);
  if (rc != 0) {
    PBL_LOG_ERR("prv_access_connection_status failed: %d", rc);
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

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && !desc.sec_state.encrypted) {
    rc = ble_gap_security_initiate(conn_handle);
    if (rc != 0) {
      return rc;
    }
  } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint8_t flags;

    rc = ble_hs_mbuf_to_flat(ctxt->om, &flags, sizeof(flags), nullptr);
    if (rc != 0) {
      return rc;
    }

    PBL_LOG_DBG("Trigger pairing flags 0x%x", flags);

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
    .uuid = BLE_UUID16_DECLARE(PBL_BT_PPS_UUID_16BIT),
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PPS_CONNECTION_STATUS_UUID)),
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            .access_cb = prv_access_connection_status,
          },
          {
            .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PPS_TRIGGER_PAIRING_UUID)),
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

/* A central that subscribed is told when the link gets encrypted. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  uint16_t attr_handle;

  if (event->type != BLE_GAP_EVENT_ENC_CHANGE || event->enc_change.status != 0) {
    return 0;
  }

  if (ble_gatts_find_chr(pebble_pairing_svc[0].uuid,
                         BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PPS_CONNECTION_STATUS_UUID)),
                         nullptr, &attr_handle) == 0) {
    (void)prv_notify_connectivity_status(event->enc_change.conn_handle, attr_handle);
  }

  return 0;
}

static struct ble_gap_event_listener s_listener;

int pbl_bt__pps_init(void) {
  int rc = ble_gatts_count_cfg(pebble_pairing_svc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to count Pebble Pairing Service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  rc = ble_gatts_add_svcs(pebble_pairing_svc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to add Pebble Pairing Service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  /* Listeners outlive the services, which are added each time the stack starts. */
  rc = ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    return rc;
  }

  return 0;
}
