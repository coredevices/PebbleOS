/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include "shell.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/responsiveness.h>
#include <pbl/shell/shell.h>

#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <services/gatt/ble_svc_gatt.h>

/* Whatever uses the subsystem adds its own commands to the set. */
PBL_SHELL_SUBCMD_SET_CREATE(sub_bt);
PBL_SHELL_CMD_REGISTER(bt, sub_bt, "Bluetooth", nullptr);

static struct pbl_bt_responsiveness_req s_response_time_req;

static int prv_cmd_host_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  ble_hs_sched_reset(BLE_HS_EAPP);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, host_reset, nullptr, "Reset the host stack", prv_cmd_host_reset, 0, 0);

static int prv_cmd_prefs_wipe(const struct pbl_shell *sh, size_t argc, char **argv) {
  return pbl_bt_unpair(nullptr);
}

PBL_SHELL_SUBCMD_ADD(sub_bt, prefs_wipe, nullptr, "Delete all pairings", prv_cmd_prefs_wipe, 0, 0);

/* The commands are for the connection made last. */
static struct ble_gap_event_listener s_listener;
static atomic_uint_fast16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  if (event->type == BLE_GAP_EVENT_CONNECT && event->connect.status == 0) {
    s_conn_handle = event->connect.conn_handle;
  } else if (event->type == BLE_GAP_EVENT_DISCONNECT &&
             event->disconnect.conn.conn_handle == s_conn_handle) {
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  }

  return 0;
}

void pbl_bt__shell_init(void) {
  (void)ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
}

static int prv_get_conn(const struct pbl_shell *sh, uint16_t *conn_handle) {
  *conn_handle = s_conn_handle;
  if (*conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    pbl_shell_error(sh, "no device connected");
    return -ENOTCONN;
  }

  return 0;
}

static int prv_cmd_le_mode(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long state;
  uint16_t conn_handle;
  int ret;

  if (pbl_shell_strtoul(argv[1], &state) != 0 || state >= PBL_BT_RESPONSE_TIME_NUM) {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  ret = prv_get_conn(sh, &conn_handle);
  if (ret != 0) {
    return ret;
  }

  pbl_bt_responsiveness_request(conn_handle, &s_response_time_req, state,
                                PBL_BT_RESPONSE_TIME_FOREVER);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, le_mode, nullptr, "Set the LE response time <0-1>", prv_cmd_le_mode, 2,
                     0);

static int prv_cmd_conn_params(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct ble_gap_upd_params params = {};
  unsigned long val[4];
  uint16_t conn_handle;
  int rc;

  for (size_t i = 0; i < 4; i++) {
    if (pbl_shell_strtoul(argv[i + 1], &val[i]) != 0 || val[i] > UINT16_MAX) {
      pbl_shell_error(sh, "invalid value '%s'", argv[i + 1]);
      return -EINVAL;
    }
  }

  rc = prv_get_conn(sh, &conn_handle);
  if (rc != 0) {
    return rc;
  }

  params.itvl_min = val[0];
  params.itvl_max = val[1];
  params.latency = val[2];
  params.supervision_timeout = val[3];

  rc = ble_gap_update_params(conn_handle, &params);
  if (rc != 0) {
    pbl_shell_error(sh, "request failed (%d)", rc);
    return -EIO;
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, conn_params, nullptr,
                     "Request connection parameters <min_1.25ms> <max_1.25ms> <latency> "
                     "<timeout_10ms>",
                     prv_cmd_conn_params, 5, 0);

static int prv_cmd_svc_changed(const struct pbl_shell *sh, size_t argc, char **argv) {
  ble_svc_gatt_changed(0x0001, 0xFFFF);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, svc_changed, nullptr, "Send a Service Changed indication",
                     prv_cmd_svc_changed, 0, 0);

#endif
