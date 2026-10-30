/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RELEASE)

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage_debug.h>
#include <pbl/shell/shell.h>
#include <pbl/util/string.h>

static void prv_dump_ble_pairing_info(const struct pbl_shell *sh,
                                      const struct pbl_bt_sm_pairing_info *info) {
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

static void prv_dump_root_keys(const struct pbl_shell *sh, const struct pbl_bt_sm_key *irk,
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

void shared_prf_storage_dump_contents(const struct pbl_shell *sh) {
  pbl_shell_print(sh, "---Shared PRF Contents---");

  struct pbl_bt_sm_pairing_info pairing_info;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  uint8_t flags;
  if (shared_prf_storage_get_ble_pairing_data(&pairing_info, name, &flags)) {
    prv_dump_ble_pairing_info(sh, &pairing_info);
    pbl_shell_print(sh, "flags: %x, BLE Dev Name: %s", flags, name);
  } else {
    pbl_shell_print(sh, "No BLE Data");
  }

  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  if (shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]) &&
      shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    prv_dump_root_keys(sh, &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY],
                       &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]);
  } else {
    pbl_shell_print(sh, "Missing IRK and/or ERK root key(s)!");
  }

  if (shared_prf_storage_get_local_device_name(name, PBL_BT_DEVICE_NAME_BUFFER_SIZE)) {
    pbl_shell_print(sh, "Local device name: %s", name);
  } else {
    pbl_shell_print(sh, "No Device Name");
  }

  pbl_shell_print(sh, "Started Complete: %s",
                  bool_to_str(shared_prf_storage_get_getting_started_complete()));
}

#endif
