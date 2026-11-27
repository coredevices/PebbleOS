/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gatts.h>
#include <pbl/bluetooth/pps.h>
#include <pbl/logging/logging.h>

#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

static const ble_uuid128_t s_status_uuid =
    PBL_BT_PPS_UUID128_INIT(PBL_BT_PPS_CONNECTION_STATUS_UUID_32BIT);
static const ble_uuid128_t s_trigger_uuid =
    PBL_BT_PPS_UUID128_INIT(PBL_BT_PPS_TRIGGER_PAIRING_UUID_32BIT);

static int prv_get_status(uint16_t conn_handle, uint8_t *status) {
  struct ble_gap_conn_desc desc;
  int rc;

  rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    return rc;
  }

  memset(status, 0, PBL_BT_PPS_CONNECTIVITY_STATUS_SIZE);
  status[0] = PBL_BT_PPS_STATUS_CONNECTED | PBL_BT_PPS_STATUS_NO_SECURITY_REQUEST_SUPPORTED;
  if (pbl_bt_bond_get(&desc.peer_id_addr, nullptr) == 0) {
    status[0] |= PBL_BT_PPS_STATUS_BONDED;
  }
  if (desc.sec_state.encrypted) {
    status[0] |= PBL_BT_PPS_STATUS_ENCRYPTED;
  }
  if (pbl_bt_bond_count() > 0U) {
    status[0] |= PBL_BT_PPS_STATUS_HAS_BONDED_GATEWAY;
  }

  return 0;
}

static void prv_notify_status(uint16_t conn_handle, uint16_t attr_handle) {
  uint8_t status[PBL_BT_PPS_CONNECTIVITY_STATUS_SIZE];
  int ret;

  if (prv_get_status(conn_handle, status) != 0) {
    return;
  }

  ret = pbl_bt_gatts_notify(conn_handle, attr_handle, status, sizeof(status));
  if (ret != 0) {
    PBL_LOG_ERR("Connectivity status notification failed: %d", ret);
  }
}

static int prv_access_status(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
  uint8_t status[PBL_BT_PPS_CONNECTIVITY_STATUS_SIZE];

  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR || prv_get_status(conn_handle, status) != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  if (os_mbuf_append(ctxt->om, status, sizeof(status)) != 0) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  return 0;
}

// What is returned is an ATT error, not one of the host.
static int prv_ask_security(uint16_t conn_handle) {
  int rc = ble_gap_security_initiate(conn_handle);

  // A pairing that is already running is what was asked for.
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    PBL_LOG_ERR("Security request failed: %d", rc);
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}

static int prv_access_trigger(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
  struct ble_gap_conn_desc desc;
  uint8_t flags;

  if (ble_gap_conn_find(conn_handle, &desc) != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    return desc.sec_state.encrypted ? 0 : prv_ask_security(conn_handle);
  }

  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  // Only the first byte of a longer write is looked at.
  if (OS_MBUF_PKTLEN(ctxt->om) == 0U) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  if (ble_hs_mbuf_to_flat(ctxt->om, &flags, sizeof(flags), nullptr) != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  PBL_LOG_DBG("Trigger pairing flags 0x%x", flags);

  if ((((flags & PBL_BT_PPS_TRIGGER_NO_SECURITY_REQUEST) == 0U) && !desc.sec_state.encrypted) ||
      ((flags & PBL_BT_PPS_TRIGGER_FORCE_SECURITY_REQUEST) != 0U)) {
    return prv_ask_security(conn_handle);
  }

  return 0;
}

static const struct ble_gatt_svc_def s_pps_svc[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID16_DECLARE(PBL_BT_PPS_UUID_16BIT),
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = &s_status_uuid.u,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            .access_cb = prv_access_status,
          },
          {
            .uuid = &s_trigger_uuid.u,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            .access_cb = prv_access_trigger,
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

/* The central is told when its link gets encrypted, subscribed or not. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  uint16_t attr_handle;

  if (event->type != BLE_GAP_EVENT_ENC_CHANGE || event->enc_change.status != 0) {
    return 0;
  }

  if (ble_gatts_find_chr(s_pps_svc[0].uuid, &s_status_uuid.u, nullptr, &attr_handle) == 0) {
    prv_notify_status(event->enc_change.conn_handle, attr_handle);
  }

  return 0;
}

static struct ble_gap_event_listener s_listener;

static int prv_add(void) {
  int rc;

  rc = pbl_bt_gatts_add(s_pps_svc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to add PPS: %d", rc);
    return rc;
  }

  /* Listeners outlive the services, which are added each time the stack starts. */
  rc = ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    return -EIO;
  }

  return 0;
}

PBL_BT_GATTS_SERVICE_DEFINE(pps, 20, prv_add);
