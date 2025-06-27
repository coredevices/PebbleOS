/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
