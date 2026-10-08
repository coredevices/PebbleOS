/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RELEASE)

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage_debug.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage_debug.h>
#include <pbl/shell/shell.h>
#include <pbl/util/string.h>

void shared_prf_storage_dump_contents(const struct pbl_shell *sh) {
  pbl_shell_print(sh, "---Shared PRF Contents---");

  struct pbl_bt_sm_pairing_info pairing_info;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  bool requires_address_pinning;
  uint8_t flags;
  if (shared_prf_storage_get_ble_pairing_data(&pairing_info, name, &requires_address_pinning,
                                              &flags)) {
    bluetooth_persistent_storage_debug_dump_ble_pairing_info(sh, &pairing_info);
    pbl_shell_print(sh, "Req addr pin: %u, flags: %x, BLE Dev Name: %s", requires_address_pinning,
                    flags, name);
  } else {
    pbl_shell_print(sh, "No BLE Data");
  }

  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  if (shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]) &&
      shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    bluetooth_persistent_storage_debug_dump_root_keys(sh, &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY],
                                                      &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]);
  } else {
    pbl_shell_print(sh, "Missing IRK and/or ERK root key(s)!");
  }

  struct pbl_bt_addr addr;
  if (shared_prf_storage_get_ble_pinned_address(&addr)) {
    pbl_shell_print(sh, "Pinned address: " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE_PTR(&addr));
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
