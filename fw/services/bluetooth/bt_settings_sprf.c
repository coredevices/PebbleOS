/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bt_settings_priv.h"

#include <string.h>

#include <pbl/bluetooth/sm_types.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>

static bool prv_is_sprf_bond(const struct pbl_bt_device_internal *addr,
                             char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  struct pbl_bt_sm_pairing_info info;

  return shared_prf_storage_get_ble_pairing_data(&info, name, nullptr) &&
         bt_device_internal_equal(&info.identity, addr);
}

bool bt_settings__sprf_get_peer_name(const struct pbl_bt_device_internal *addr,
                                     char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  return prv_is_sprf_bond(addr, name) && name[0] != '\0';
}

void bt_settings__sprf_set_peer_name(const struct pbl_bt_device_internal *addr, const char *name) {
  char stored[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  if (!prv_is_sprf_bond(addr, stored) ||
      strncmp(stored, name, PBL_BT_DEVICE_NAME_BUFFER_SIZE) == 0) {
    return;
  }

  shared_prf_storage_set_ble_pairing_name(name);
}
