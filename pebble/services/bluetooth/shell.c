/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/addr.h>
#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/link.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/shell/shell.h>

#include <host/ble_store.h>

#ifdef CONFIG_RECOVERY_FW
extern void factory_reset_set_reason_and_reset(void);
#endif

static int prv_cmd_mac(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t addr[6];

  bt_local_id_copy_address(addr);
  pbl_shell_print(sh, "0x%02X%02X%02X%02X%02X%02X", PBL_BT_ADDR_XPLODE(addr));
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, mac, nullptr, "Print the local address", prv_cmd_mac, 0, 0);

static int prv_cmd_name(const struct pbl_shell *sh, size_t argc, char **argv) {
  bt_local_id_set_device_name(argv[1]);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, name, nullptr, "Set the device name <name>", prv_cmd_name, 2, 0);

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

#ifndef CONFIG_RELEASE
static void prv_print_bond(const struct pbl_bt_bond_info *info, void *user_data) {
  const struct pbl_shell *sh = user_data;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  if (!bt_bonding_get_peer_name(&info->addr, name)) {
    strncpy(name, "<Unknown>", sizeof(name));
  }

  pbl_shell_print(sh, "Bond: " PBL_BT_ADDR_FMT " random=%u irk=%u",
                  PBL_BT_ADDR_XPLODE(info->addr.val), info->addr.type == BLE_ADDR_RANDOM,
                  info->irk_valid);
  pbl_shell_print(sh, " Name: %s", name);
}

static int prv_cmd_bonds(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct ble_store_value_sec peer;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  pbl_bt_foreach_bond(prv_print_bond, (void *)sh);

  if (pbl_bt_prf_store_get_pairing(&peer, nullptr, name)) {
    pbl_shell_print(sh, "PRF store: " PBL_BT_ADDR_FMT " random=%u sc=%u authenticated=%u",
                    PBL_BT_ADDR_XPLODE(peer.peer_addr.val), peer.peer_addr.type == BLE_ADDR_RANDOM,
                    peer.sc, peer.authenticated);
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
