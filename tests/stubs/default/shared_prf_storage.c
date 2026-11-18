/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>

PBL_WEAK void shared_prf_storage_erase_ble_pairing_data(void) {
}

PBL_WEAK bool shared_prf_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                              struct pbl_bt_sm_key *key_out) {
  return false;
}

PBL_WEAK void shared_prf_storage_set_root_keys(struct pbl_bt_sm_key *keys_in) {
}

PBL_WEAK bool shared_prf_storage_get_ble_pairing_data(
    struct pbl_bt_sm_pairing_info *pairing_info_out, char *name_out, uint8_t *flags) {
  return false;
}

PBL_WEAK void shared_prf_storage_store_ble_pairing_data(
    const struct pbl_bt_sm_pairing_info *pairing_info, const char *name, uint8_t flags) {
}

PBL_WEAK void shared_prf_storage_wipe_all(void) {
}
