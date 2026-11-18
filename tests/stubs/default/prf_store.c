/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/prf_store.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK void pbl_bt_prf_store_erase_pairing(void) {
}

PBL_WEAK bool pbl_bt_prf_store_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                            struct pbl_bt_sm_key *key_out) {
  return false;
}

PBL_WEAK void pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in) {
}

PBL_WEAK bool pbl_bt_prf_store_get_pairing(struct pbl_bt_sm_pairing_info *pairing_info_out,
                                           char *name_out, uint8_t *flags) {
  return false;
}

PBL_WEAK void pbl_bt_prf_store_set_pairing(const struct pbl_bt_sm_pairing_info *pairing_info,
                                           const char *name, uint8_t flags) {
}

PBL_WEAK void pbl_bt_prf_store_wipe(void) {
}

PBL_WEAK void pbl_bt_prf_store_set_pairing_name(const char *name) {
}
