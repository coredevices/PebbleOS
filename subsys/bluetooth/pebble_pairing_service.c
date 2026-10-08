/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/pebble_pairing_service.h>
#include <comm/ble/gap_le_connection.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>
#include <system/passert.h>

#include "nimble_type_conversions.h"

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define TRIGGER_PAIRING_NO_SEC_REQ    (1U << 1U)
#define TRIGGER_PAIRING_FORCE_SEC_REQ (1U << 2U)

//! Bounds from BT Core v4.2, Vol 6, Part B, 4.5.1 / Vol 2, Part E, 7.8.18 (connection interval in
//! 1.25 ms units, supervision timeout in 10 ms units).
#define CONN_INTERVAL_MIN_1_25MS     (6U)
#define CONN_INTERVAL_MAX_1_25MS     (3200U)
#define SUPERVISION_TIMEOUT_MIN_10MS (10U)

static uint16_t s_conn_params_val_handle;

//! Connections that enabled notifications on the Connection Parameters characteristic.
static uint16_t s_conn_params_subscribers[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];

static struct ble_gap_event_listener s_gap_event_listener;

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

int pebble_pairing_service_get_connectivity_send_notification(uint16_t conn_handle,
                                                              uint16_t attr_handle) {
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

    rc = ble_hs_mbuf_to_flat(ctxt->om, &flags, sizeof(flags), NULL);
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

static int prv_get_conn_params(uint16_t conn_handle,
                               struct pbl_bt_pps_conn_params_read_notif *params) {
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    return rc;
  }

  memset(params, 0, sizeof(*params));
  params->current_interval_1_25ms = desc.conn_itvl;
  params->current_slave_latency_events = desc.conn_latency;
  params->current_supervision_timeout_10ms = desc.supervision_timeout;

  return 0;
}

static int prv_validate_conn_param_set(const struct pbl_bt_pps_conn_param_set *set) {
  const uint32_t interval_min = set->interval_min_1_25ms;
  const uint32_t interval_max = interval_min + set->interval_max_delta_1_25ms;
  const uint32_t supervision_timeout_10ms = set->supervision_timeout_30ms * 3U;

  if (interval_min < CONN_INTERVAL_MIN_1_25MS) {
    return PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_SMALL;
  }
  if (interval_min > CONN_INTERVAL_MAX_1_25MS) {
    return PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_LARGE;
  }
  if (interval_max > CONN_INTERVAL_MAX_1_25MS) {
    return PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MAX_SLOTS_TOO_LARGE;
  }
  // The supervision timeout must exceed (1 + latency) * interval_max * 2:
  // timeout_10ms * 10 > (1 + latency) * interval_max_1_25ms * 1.25 * 2
  if (supervision_timeout_10ms < SUPERVISION_TIMEOUT_MIN_10MS ||
      supervision_timeout_10ms * 4U <= (1U + set->slave_latency_events) * interval_max) {
    return PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_SUPERVISION_TIMEOUT_TOO_SMALL;
  }

  return 0;
}

//! Validates a write to the Connection Parameters characteristic. Returns 0 if the write should be
//! passed on to the firmware, or the ATT error to respond with.
static int prv_validate_conn_params_write(const uint8_t *buf, uint16_t len) {
  if (len < 1U) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  const struct pbl_bt_pps_conn_params_write *write =
      (const struct pbl_bt_pps_conn_params_write *)buf;
  switch (write->cmd) {
    case PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS: {
      const size_t settings_len =
          len - offsetof(struct pbl_bt_pps_conn_params_write, remote_param_mgmt_settings);
      // The parameter sets are optional, but if present, all of them must be:
      if (settings_len == sizeof(struct pbl_bt_pps_remote_param_mgmt_settings)) {
        return 0;
      }
      if (settings_len != PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      for (int i = 0; i < PBL_BT_RESPONSE_TIME_NUM; ++i) {
        int rc = prv_validate_conn_param_set(
            &write->remote_param_mgmt_settings.connection_parameter_sets[i]);
        if (rc != 0) {
          return rc;
        }
      }
      return 0;
    }

    case PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE:
      if (len != offsetof(struct pbl_bt_pps_conn_params_write, remote_desired_state) +
                     sizeof(struct pbl_bt_pps_remote_desired_state)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      if (write->remote_desired_state.state >= PBL_BT_RESPONSE_TIME_NUM) {
        return PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_INVALID_REMOTE_DESIRED_STATE;
      }
      return 0;

    case PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION:
      return PBL_BT_PPS_GATT_ERROR_DEVICE_DOES_NOT_SUPPORT_PLE;

    default:
      return PBL_BT_PPS_GATT_ERROR_UNKNOWN_COMMAND_ID;
  }
}

static int prv_access_connection_parameters(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
  int rc;

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    struct pbl_bt_pps_conn_params_read_notif params;
    rc = prv_get_conn_params(conn_handle, &params);
    if (rc != 0) {
      return BLE_ATT_ERR_UNLIKELY;
    }
    rc = os_mbuf_append(ctxt->om, &params, sizeof(params));
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  uint8_t buf[PBL_BT_PPS_CONN_PARAMS_WRITE_SIZE_WITH_PARAM_SETS];
  const uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
  if (len > sizeof(buf)) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }
  rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);
  if (rc != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  rc = prv_validate_conn_params_write(buf, len);
  if (rc != 0) {
    PBL_LOG_WRN("Rejected connection parameters write (cmd %u, %u bytes): 0x%02x",
                (len > 0U) ? buf[0] : 0U, len, rc);
    return rc;
  }

  struct ble_gap_conn_desc desc;
  rc = ble_gap_conn_find(conn_handle, &desc);
  if (rc != 0) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  struct pbl_bt_device_internal device;
  nimble_addr_to_pebble_device(&desc.peer_id_addr, &device);
  pbl_bt_cb_pps_handle_connection_parameter_write(
      &device, (const struct pbl_bt_pps_conn_params_write *)buf, len);

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
            .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PPS_CONNECTION_PARAMETERS_UUID)),
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            .access_cb = prv_access_connection_parameters,
            .val_handle = &s_conn_params_val_handle,
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

static void prv_set_conn_params_subscriber(uint16_t conn_handle, bool subscribed) {
  uint16_t *free_slot = NULL;
  for (size_t i = 0; i < ARRAY_LENGTH(s_conn_params_subscribers); ++i) {
    if (s_conn_params_subscribers[i] == conn_handle) {
      if (!subscribed) {
        s_conn_params_subscribers[i] = BLE_HS_CONN_HANDLE_NONE;
      }
      return;
    }
    if (!free_slot && s_conn_params_subscribers[i] == BLE_HS_CONN_HANDLE_NONE) {
      free_slot = &s_conn_params_subscribers[i];
    }
  }
  // One slot per connection, so a subscribing connection always finds one.
  if (subscribed && free_slot) {
    *free_slot = conn_handle;
  }
}

static bool prv_is_conn_params_subscriber(uint16_t conn_handle) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_conn_params_subscribers); ++i) {
    if (s_conn_params_subscribers[i] == conn_handle) {
      return true;
    }
  }
  return false;
}

static void prv_notify_conn_params(uint16_t conn_handle) {
  struct pbl_bt_pps_conn_params_read_notif params;
  if (prv_get_conn_params(conn_handle, &params) != 0) {
    return;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(&params, sizeof(params));
  int rc = ble_gatts_notify_custom(conn_handle, s_conn_params_val_handle, om);
  if (rc != 0) {
    PBL_LOG_ERR("Connection parameters notification failed: 0x%04x", (uint16_t)rc);
  }
}

static int prv_handle_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_SUBSCRIBE:
      if (event->subscribe.attr_handle == s_conn_params_val_handle) {
        prv_set_conn_params_subscriber(event->subscribe.conn_handle, event->subscribe.cur_notify);
      }
      break;
    case BLE_GAP_EVENT_CONN_UPDATE:
      if (event->conn_update.status == 0 &&
          prv_is_conn_params_subscriber(event->conn_update.conn_handle)) {
        prv_notify_conn_params(event->conn_update.conn_handle);
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      prv_set_conn_params_subscriber(event->disconnect.conn.conn_handle, false);
      break;
    default:
      break;
  }
  return 0;
}

void pebble_pairing_service_init(void) {
  int rc;

  for (size_t i = 0; i < ARRAY_LENGTH(s_conn_params_subscribers); ++i) {
    s_conn_params_subscribers[i] = BLE_HS_CONN_HANDLE_NONE;
  }

  rc = ble_gatts_count_cfg(pebble_pairing_svc);
  PBL_ASSERTN(rc == 0);
  rc = ble_gatts_add_svcs(pebble_pairing_svc);
  PBL_ASSERTN(rc == 0);
  // Called on every pbl_bt_start, but the listener list is only cleared on
  // nimble_port_init, so tolerate EALREADY.
  rc = ble_gap_event_listener_register(&s_gap_event_listener, prv_handle_gap_event, NULL);
  PBL_ASSERTN(rc == 0 || rc == BLE_HS_EALREADY);
}

void prv_notify_chr_updated(const GAPLEConnection *connection, const ble_uuid_t *chr_uuid) {
  int rc;
  uint16_t conn_handle;

  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    PBL_LOG_ERR("prv_notify_chr_updated: failed to find connection handle");
    return;
  }

  uint16_t attr_handle;
  rc = ble_gatts_find_chr(pebble_pairing_svc[0].uuid, chr_uuid, NULL, &attr_handle);
  if (rc != 0) {
    PBL_LOG_ERR("prv_notify_chr_updated: failed to find characteristic handle");
    return;
  }
  pebble_pairing_service_get_connectivity_send_notification(conn_handle, attr_handle);
}

void pbl_bt_pps_handle_status_change(const GAPLEConnection *connection) {
  prv_notify_chr_updated(connection,
                         BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PPS_CONNECTION_STATUS_UUID)));
}
