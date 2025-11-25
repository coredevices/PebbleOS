/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/pairing_confirm.h>
#include <host/ble_hs.h>
#include <host/ble_sm.h>
#include <stdint.h>
#include <system/logging.h>

void bt_driver_pairing_confirm(const PairingUserConfirmationCtx *ctx, bool is_confirmed) {
  uint16_t conn_handle = (uintptr_t)ctx;
  struct ble_sm_io key = {
      .action = BLE_SM_IOACT_NUMCMP,
      .numcmp_accept = is_confirmed,
  };
  int rc = ble_sm_inject_io(conn_handle, &key);

  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "ble_sm_inject_io rc=0x%04x", (uint16_t)rc);
}
