/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <string.h>

#include <pbl/bluetooth/id.h>
#include <pbl/bluetooth/types.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/shell/shell.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_lock.h>

PBL_SHELL_SUBCMD_SET_CREATE(sub_bt);
PBL_SHELL_CMD_REGISTER(bt, sub_bt, "Bluetooth", nullptr);

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
  bt_persistent_storage_delete_all_pairings();
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, prefs_wipe, nullptr, "Delete all pairings", prv_cmd_prefs_wipe, 0, 0);

static int prv_cmd_status(const struct pbl_shell *sh, size_t argc, char **argv) {
  char chip_info[64];
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  bool connected = false;

  pbl_shell_print(sh, "Alive: %s", bt_ctl_is_bluetooth_running() ? "yes" : "no");

  pbl_bt_id_copy_chip_info_string(chip_info, sizeof(chip_info));
  pbl_shell_print(sh, "BT Chip Info: %s", chip_info);

  bt_lock();
  GAPLEConnection *connection = gap_le_connection_any();
  if (connection) {
    strncpy(name, connection->device_name ?: "<Unknown>", sizeof(name));
    name[sizeof(name) - 1] = '\0';
    connected = true;
  }
  bt_unlock();

  pbl_shell_print(sh, "Connected: %s", connected ? "yes" : "no");
  if (connected) {
    pbl_shell_print(sh, "Device: %s", name);
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, status, nullptr, "Show the Bluetooth status", prv_cmd_status, 0, 0);

#ifndef CONFIG_RELEASE
static int prv_cmd_sprf_nuke(const struct pbl_shell *sh, size_t argc, char **argv) {
  shared_prf_storage_wipe_all();
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
