/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/id.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/responsiveness.h>
#include <pbl/bluetooth/types.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/link.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/shell/shell.h>

#include <host/ble_gap.h>
#include <host/ble_store.h>
#include <services/gatt/ble_svc_gatt.h>

PBL_SHELL_SUBCMD_SET_CREATE(sub_bt);
PBL_SHELL_CMD_REGISTER(bt, sub_bt, "Bluetooth", nullptr);

static struct pbl_bt_responsiveness_req s_response_time_req;

static int prv_cmd_mac(const struct pbl_shell *sh, size_t argc, char **argv) {
  char addr_hex_str[PBL_BT_BD_ADDR_FMT_BUFFER_SIZE];

  bt_local_id_copy_address_hex_string(addr_hex_str);
  pbl_shell_print(sh, "%s", addr_hex_str);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, mac, nullptr, "Print the local address", prv_cmd_mac, 0, 0);

static int prv_cmd_name(const struct pbl_shell *sh, size_t argc, char **argv) {
  bt_local_id_set_device_name(argv[1]);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, name, nullptr, "Set the device name <name>", prv_cmd_name, 2, 0);

static int prv_cmd_prefs_wipe(const struct pbl_shell *sh, size_t argc, char **argv) {
  return pbl_bt_unpair(nullptr);
}

PBL_SHELL_SUBCMD_ADD(sub_bt, prefs_wipe, nullptr, "Delete all pairings", prv_cmd_prefs_wipe, 0, 0);

static int prv_cmd_status(const struct pbl_shell *sh, size_t argc, char **argv) {
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  const bool connected = bt_link_is_connected();

  pbl_shell_print(sh, "Alive: %s", bt_ctl_is_bluetooth_running() ? "yes" : "no");

  pbl_shell_print(sh, "Connected: %s", connected ? "yes" : "no");
  if (connected) {
    bt_link_copy_peer_name(name, sizeof(name));
    pbl_shell_print(sh, "Device: %s", (name[0] != '\0') ? name : "<Unknown>");
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, status, nullptr, "Show the Bluetooth status", prv_cmd_status, 0, 0);

static int prv_cmd_le_mode(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long state;

  if (pbl_shell_strtoul(argv[1], &state) != 0 || state >= PBL_BT_RESPONSE_TIME_NUM) {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  pbl_bt_responsiveness_request(bt_link_get_conn_handle(), &s_response_time_req, state,
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

  conn_handle = bt_link_get_conn_handle();
  if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    pbl_shell_error(sh, "no device connected");
    return -ENOTCONN;
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

#ifndef CONFIG_RELEASE
static void prv_print_bond(const struct pbl_bt_bond_info *info, void *user_data) {
  const struct pbl_shell *sh = user_data;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  if (!bt_bonding_get_peer_name(&info->addr, name)) {
    strncpy(name, "<Unknown>", sizeof(name));
  }

  pbl_shell_print(sh, "Bond: " PBL_BT_ADDR_FMT " random=%u irk=%u",
                  PBL_BT_ADDR_XPLODE_VAL(info->addr.val), info->addr.type == BLE_ADDR_RANDOM,
                  info->irk_valid);
  pbl_shell_print(sh, " Name: %s", name);
}

static int prv_cmd_bonds(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct ble_store_value_sec peer;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  pbl_bt_foreach_bond(prv_print_bond, (void *)sh);

  if (pbl_bt_prf_store_get_pairing(&peer, nullptr, name)) {
    pbl_shell_print(sh, "PRF store: " PBL_BT_ADDR_FMT " random=%u sc=%u authenticated=%u",
                    PBL_BT_ADDR_XPLODE_VAL(peer.peer_addr.val),
                    peer.peer_addr.type == BLE_ADDR_RANDOM, peer.sc, peer.authenticated);
    pbl_shell_print(sh, " Name: %s", name);
  } else {
    pbl_shell_print(sh, "PRF store: no pairing");
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, bonds, nullptr, "Dump the bonds", prv_cmd_bonds, 0, 0);

static int prv_cmd_sprf_nuke(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int ret = pbl_bt_prf_store_wipe();

  if (ret != 0) {
    pbl_shell_error(sh, "wipe failed (%d)", ret);
    return ret;
  }

#ifdef CONFIG_RECOVERY_FW
  // Reset to get the host and controller caches back in sync
  extern void factory_reset_set_reason_and_reset(void);
  factory_reset_set_reason_and_reset();
#endif
  return 0;
}

static const struct pbl_shell_cmd sub_bt_sprf[] = {
  PBL_SHELL_CMD(nuke, nullptr, "Wipe the shared PRF storage", prv_cmd_sprf_nuke),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_SUBCMD_ADD(sub_bt, sprf, sub_bt_sprf, "Shared PRF storage", nullptr, 0, 0);
#endif

#endif
