/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "prf_sync.h"

#include <pbl/bluetooth/prf_store.h>

/* One bonding is kept, in the PRF store. The others, and the CCCDs, stay in RAM. */

int ble_store_config_persist_our_secs(void) {
  return nimble_store__prf_sync_save();
}

int ble_store_config_persist_peer_secs(void) {
  return nimble_store__prf_sync_save();
}

int ble_store_config_persist_cccds(void) {
  return 0;
}

void ble_store_config_conf_init(void) {
  pbl_bt_prf_store_init();
  (void)nimble_store__prf_sync_load();
}
