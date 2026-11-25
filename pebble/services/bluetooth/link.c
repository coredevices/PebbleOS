/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/clients/gap.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/link.h>

#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <kernel/events.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

static PBL_MUTEX_DEFINE(s_lock);
static struct ble_gap_event_listener s_listener;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_is_gateway;
static char s_peer_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

/* Only used by the Settings and recovery UIs, to refresh. */
static void prv_put_connection_event(bool connected) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .bluetooth = {
      .connection = {
        .is_ble = true,
        .state = connected ? PebbleBluetoothConnectionEventStateConnected
                           : PebbleBluetoothConnectionEventStateDisconnected,
      },
    },
  };

  event_put(&event);
}

static void prv_put_gateway_event(uint16_t conn_handle, uint8_t hci_reason, bool connected) {
  PebbleEvent event = {
    .type = PEBBLE_BLE_CONNECTION_EVENT,
    .task_mask = (PebbleTaskBitset) ~(1u << PebbleTask_KernelMain),
    .bluetooth = {
      .le = {
        .connection = {
          .conn_handle = conn_handle,
          .hci_reason = hci_reason,
          .connected = connected,
        },
      },
    },
  };

  event_put(&event);
}

static void prv_stop_interval_timers(void) {
  PBL_ANALYTICS_TIMER_STOP(ble_conn_itvl_min_time_ms);
  PBL_ANALYTICS_TIMER_STOP(ble_conn_itvl_mid_time_ms);
  PBL_ANALYTICS_TIMER_STOP(ble_conn_itvl_other_time_ms);
  PBL_ANALYTICS_TIMER_STOP(ble_conn_slave_lat0_time_ms);
}

/* How long the link spends at the intervals of the fast and of the slow response times. */
static void prv_start_interval_timers(const struct ble_gap_conn_desc *desc) {
  prv_stop_interval_timers();

  if (desc->conn_itvl == BLE_GAP_CONN_ITVL_MS(15)) {
    PBL_ANALYTICS_TIMER_START(ble_conn_itvl_min_time_ms);
  } else if (desc->conn_itvl >= BLE_GAP_CONN_ITVL_MS(45) &&
             desc->conn_itvl <= BLE_GAP_CONN_ITVL_MS(60)) {
    PBL_ANALYTICS_TIMER_START(ble_conn_itvl_mid_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(ble_conn_itvl_other_time_ms);
  }

  /* The intervals do not tell a link that never got its latency: several times the duty. */
  if (desc->conn_latency == 0) {
    PBL_ANALYTICS_TIMER_START(ble_conn_slave_lat0_time_ms);
  }
}

static void prv_count_disconnect(uint8_t reason) {
  switch (reason) {
    case BLE_ERR_CONN_SPVN_TMO:
      PBL_ANALYTICS_ADD(ble_disconnect_conn_spvn_tmo_count, 1);
      break;
    case BLE_ERR_REM_USER_CONN_TERM:
      PBL_ANALYTICS_ADD(ble_disconnect_rem_user_term_count, 1);
      break;
    case BLE_ERR_CONN_TERM_LOCAL:
      PBL_ANALYTICS_ADD(ble_disconnect_conn_term_local_count, 1);
      break;
    case BLE_ERR_LMP_LL_RSP_TMO:
      PBL_ANALYTICS_ADD(ble_disconnect_lmp_ll_rsp_tmo_count, 1);
      break;
    case BLE_ERR_CONN_ESTABLISHMENT:
      PBL_ANALYTICS_ADD(ble_disconnect_conn_establishment_count, 1);
      break;
    default:
      PBL_ANALYTICS_ADD(ble_disconnect_other_count, 1);
      break;
  }
}

static bool prv_is_bonded(const struct ble_gap_conn_desc *desc) {
  const struct ble_store_key_sec key_sec = {.peer_addr = desc->peer_id_addr};
  struct ble_store_value_sec value_sec;

  return ble_store_read_peer_sec(&key_sec, &value_sec) == 0;
}

/* s_lock must be held. */
static void prv_update_gateway(void) {
  struct ble_gap_conn_desc desc;

  if (s_is_gateway || ble_gap_conn_find(s_conn_handle, &desc) != 0 || !desc.sec_state.encrypted ||
      !prv_is_bonded(&desc)) {
    return;
  }

  s_is_gateway = true;
  prv_put_gateway_event(s_conn_handle, 0, true);
}

static void prv_handle_connect(const struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;

  if (event->connect.status != 0 || ble_gap_conn_find(event->connect.conn_handle, &desc) != 0) {
    return;
  }

  PBL_LOG_INFO("Connected: handle %u, interval %u, latency %u, timeout %u", desc.conn_handle,
               desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_conn_handle = desc.conn_handle;
  s_is_gateway = false;
  s_peer_name[0] = '\0';
  pbl_mutex_unlock(&s_lock);

#ifdef CONFIG_RECOVERY_FW
  static struct pbl_bt_responsiveness_req s_response_time_req;

  /* Power matters little, and firmware updates are to be quick. */
  pbl_bt_responsiveness_request(desc.conn_handle, &s_response_time_req, PBL_BT_RESPONSE_TIME_MIN,
                                PBL_BT_RESPONSE_TIME_FOREVER);
#endif

  prv_start_interval_timers(&desc);
  bt_advertising_handle_connect();
  prv_put_connection_event(true);
}

static void prv_handle_disconnect(const struct ble_gap_event *event) {
  /* What is left of the host's reason is HCI's. */
  const uint8_t reason = (uint8_t)event->disconnect.reason;
  bool was_gateway;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (event->disconnect.conn.conn_handle != s_conn_handle) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  was_gateway = s_is_gateway;
  s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  s_is_gateway = false;
  s_peer_name[0] = '\0';
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_INFO("Disconnected: handle %u, reason 0x%02x", event->disconnect.conn.conn_handle,
               reason);

  prv_stop_interval_timers();
  prv_count_disconnect(reason);
  bt_advertising_handle_disconnect();
  prv_put_connection_event(false);
  if (was_gateway) {
    prv_put_gateway_event(event->disconnect.conn.conn_handle, reason, false);
  }
}

static void prv_handle_enc_change(const struct ble_gap_event *event) {
  if (event->enc_change.status != 0) {
    PBL_LOG_ERR("Encryption failed (0x%04x)", (uint16_t)event->enc_change.status);
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (event->enc_change.conn_handle == s_conn_handle) {
    prv_update_gateway();
  }
  pbl_mutex_unlock(&s_lock);
}

static void prv_handle_conn_update(const struct ble_gap_event *event) {
  struct ble_gap_conn_desc desc;

  if (event->conn_update.status != 0) {
    PBL_LOG_ERR("Connection parameters update failed (0x%04x)",
                (uint16_t)event->conn_update.status);
    return;
  }

  if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) != 0) {
    return;
  }

  PBL_LOG_INFO("Connection parameters: interval %u, latency %u, timeout %u", desc.conn_itvl,
               desc.conn_latency, desc.supervision_timeout);
  prv_start_interval_timers(&desc);
  PBL_ANALYTICS_ADD(ble_conn_param_update_count, 1);
}

static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      prv_handle_connect(event);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      prv_handle_disconnect(event);
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      prv_handle_enc_change(event);
      break;
    case BLE_GAP_EVENT_CONN_UPDATE:
      prv_handle_conn_update(event);
      break;
    default:
      break;
  }

  return 0;
}

void bt_link_init(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  s_is_gateway = false;
  s_peer_name[0] = '\0';
  pbl_mutex_unlock(&s_lock);

  /* Listeners outlive the stack: a second registration is refused. */
  (void)ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
}

void bt_link_deinit(void) {
  uint16_t conn_handle;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  conn_handle = s_conn_handle;
  s_is_gateway = false;
  pbl_mutex_unlock(&s_lock);

  if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    (void)ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  }
}

uint16_t bt_link_get_conn_handle(void) {
  uint16_t conn_handle;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  conn_handle = s_conn_handle;
  pbl_mutex_unlock(&s_lock);

  return conn_handle;
}

bool bt_link_is_connected(void) {
  return bt_link_get_conn_handle() != BLE_HS_CONN_HANDLE_NONE;
}

bool bt_link_is_connected_to(const ble_addr_t *identity) {
  struct ble_gap_conn_desc desc;

  return ble_gap_conn_find(bt_link_get_conn_handle(), &desc) == 0 &&
         ble_addr_cmp(&desc.peer_id_addr, identity) == 0;
}

void bt_link_set_response_time(struct pbl_bt_responsiveness_req *req,
                               enum pbl_bt_response_time_state state, uint16_t max_period_secs) {
  pbl_bt_responsiveness_request(bt_link_get_conn_handle(), req, state, max_period_secs);
}

static void prv_peer_name_changed_cb(void *data) {
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  struct ble_gap_conn_desc desc;
  PebbleEvent event = {
    .type = PEBBLE_BLE_DEVICE_NAME_UPDATED_EVENT,
  };

  if (ble_gap_conn_find(bt_link_get_conn_handle(), &desc) != 0 || !prv_is_bonded(&desc)) {
    return;
  }

  bt_link_copy_peer_name(name, sizeof(name));
  bt_bonding_set_peer_name(&desc.peer_id_addr, name);
  event_put(&event);
}

static void prv_peer_name_cb(uint16_t conn_handle, const char *name) {
  bool changed = false;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (conn_handle == s_conn_handle && strcmp(s_peer_name, name) != 0) {
    strncpy(s_peer_name, name, sizeof(s_peer_name) - 1);
    s_peer_name[sizeof(s_peer_name) - 1] = '\0';
    changed = true;
  }
  pbl_mutex_unlock(&s_lock);

  /* The name goes to flash, not from the host task. */
  if (changed) {
    (void)pbl_sys_work_call(prv_peer_name_changed_cb, nullptr);
  }
}

void bt_link_read_peer_name(void) {
  const uint16_t conn_handle = bt_link_get_conn_handle();

  if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    pbl_bt_gapc_read_name(conn_handle, prv_peer_name_cb);
  }
}

void bt_link_copy_peer_name(char *name, size_t size) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  strncpy(name, s_peer_name, size);
  name[size - 1] = '\0';
  pbl_mutex_unlock(&s_lock);
}

void bt_link_handle_bond_added(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_update_gateway();
  pbl_mutex_unlock(&s_lock);

  bt_link_read_peer_name();
}

void bt_link_handle_bond_deleted(void) {
  uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
  struct ble_gap_conn_desc desc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_is_gateway && ble_gap_conn_find(s_conn_handle, &desc) == 0 && !prv_is_bonded(&desc)) {
    /* The disconnection that follows is no longer the gateway's. */
    conn_handle = s_conn_handle;
    s_is_gateway = false;
    prv_put_gateway_event(conn_handle, BT_LINK_REASON_BOND_REMOVED, false);
  }
  pbl_mutex_unlock(&s_lock);

  if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    (void)ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  }
}
