/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RELEASE)

#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage_debug.h>
#include <pbl/shell/shell.h>
#include <pbl/util/string.h>

void bluetooth_persistent_storage_debug_dump_ble_pairing_info(
    const struct pbl_shell *sh, const struct pbl_bt_sm_pairing_info *info) {
  pbl_shell_print(sh, " Local Encryption Info:");
  pbl_shell_hexdump(sh, &info->local_encryption_info, sizeof(info->local_encryption_info));

  pbl_shell_print(sh, " Remote Encryption Info:");
  pbl_shell_hexdump(sh, &info->remote_encryption_info, sizeof(info->remote_encryption_info));

  pbl_shell_print(sh, " IRK:");
  pbl_shell_hexdump(sh, &info->irk, sizeof(info->irk));

  pbl_shell_print(sh, " Identity:");
  pbl_shell_hexdump(sh, &info->identity, sizeof(info->identity));

  pbl_shell_print(sh, " CSRK:");
  pbl_shell_hexdump(sh, &info->csrk, sizeof(info->csrk));

  pbl_shell_print(sh, " local encryption valid:  %s",
                  bool_to_str(info->is_local_encryption_info_valid));
  pbl_shell_print(sh, " remote encryption valid: %s",
                  bool_to_str(info->is_remote_encryption_info_valid));
  pbl_shell_print(sh, " remote identity valid:   %s",
                  bool_to_str(info->is_remote_identity_info_valid));
  pbl_shell_print(sh, " remote signature valid:  %s",
                  bool_to_str(info->is_remote_signing_info_valid));
}

void bluetooth_persistent_storage_debug_dump_root_keys(const struct pbl_shell *sh,
                                                       const struct pbl_bt_sm_key *irk,
                                                       const struct pbl_bt_sm_key *erk) {
  pbl_shell_print(sh, "Root keys:");

  pbl_shell_print(sh, " IRK:");
  if (irk) {
    pbl_shell_hexdump(sh, irk, sizeof(*irk));
  } else {
    pbl_shell_print(sh, "  None");
  }

  pbl_shell_print(sh, " ERK:");
  if (erk) {
    pbl_shell_hexdump(sh, erk, sizeof(*erk));
  } else {
    pbl_shell_print(sh, "  None");
  }
}

static void prv_dump_prf_store(const struct pbl_shell *sh) {
  pbl_shell_print(sh, "---PRF store---");

  struct pbl_bt_sm_pairing_info pairing_info;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  uint8_t flags;
  if (pbl_bt_prf_store_get_pairing(&pairing_info, name, &flags)) {
    bluetooth_persistent_storage_debug_dump_ble_pairing_info(sh, &pairing_info);
    pbl_shell_print(sh, "flags: %x, BLE Dev Name: %s", flags, name);
  } else {
    pbl_shell_print(sh, "No BLE Data");
  }

  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  if (pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]) &&
      pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    bluetooth_persistent_storage_debug_dump_root_keys(sh, &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY],
                                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]);
  } else {
    pbl_shell_print(sh, "Missing IRK and/or ERK root key(s)!");
  }
}

static int prv_cmd_gapdb(const struct pbl_shell *sh, size_t argc, char **argv) {
#ifndef CONFIG_RECOVERY_FW
  bluetooth_persistent_storage_dump_contents(sh);
#endif
  prv_dump_prf_store(sh);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, gapdb, nullptr, "Dump the bonding database", prv_cmd_gapdb, 0, 0);

#endif
