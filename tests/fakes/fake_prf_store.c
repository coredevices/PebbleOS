/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_prf_store.h"

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

static int s_prf_storage_ble_store_count;
static int s_prf_storage_ble_delete_count;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Test functions

void fake_prf_store_reset_counts(void) {
  s_prf_storage_ble_store_count = 0;
  s_prf_storage_ble_delete_count = 0;
}

int fake_prf_store_get_ble_store_count(void) {
  return s_prf_storage_ble_store_count;
}

int fake_prf_store_get_ble_delete_count(void) {
  return s_prf_storage_ble_delete_count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Root Keys

bool pbl_bt_prf_store_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                   struct pbl_bt_sm_key *key_out) {
  return false;
}

void pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Data

bool pbl_bt_prf_store_get_pairing(struct pbl_bt_sm_pairing_info *pairing_info_out, char *name_out,
                                  uint8_t *flags) {
  return false;
}

void pbl_bt_prf_store_set_pairing(const struct pbl_bt_sm_pairing_info *pairing_info,
                                  const char *name, uint8_t flags) {
  s_prf_storage_ble_delete_count++;
  s_prf_storage_ble_store_count++;
}

void pbl_bt_prf_store_erase_pairing(void) {
  s_prf_storage_ble_delete_count++;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Factory Reset

void pbl_bt_prf_store_wipe(void) {
  return;
}
