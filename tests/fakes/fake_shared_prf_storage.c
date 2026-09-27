/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_shared_prf_storage.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

static int s_prf_storage_ble_store_count;
static int s_prf_storage_ble_delete_count;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Test functions

void fake_shared_prf_storage_reset_counts(void) {
  s_prf_storage_ble_store_count = 0;
  s_prf_storage_ble_delete_count = 0;
}

int fake_shared_prf_storage_get_ble_store_count(void) {
  return s_prf_storage_ble_store_count;
}

int fake_shared_prf_storage_get_ble_delete_count(void) {
  return s_prf_storage_ble_delete_count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Custom Local Device Name

bool shared_prf_storage_get_local_device_name(char *local_device_name_out, size_t max_size) {
  return false;
}

void shared_prf_storage_set_local_device_name(char *local_device_name) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Root Keys

bool shared_prf_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                     struct pbl_bt_sm_key *key_out) {
  return false;
}

void shared_prf_storage_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Data

bool shared_prf_storage_get_ble_pairing_data(struct pbl_bt_sm_pairing_info *pairing_info_out,
                                             char *name_out, bool *requires_address_pinning_out,
                                             uint8_t *flags) {
  return false;
}

void shared_prf_storage_store_ble_pairing_data(const struct pbl_bt_sm_pairing_info *pairing_info,
                                               char *name, bool requires_address_pinning,
                                               uint8_t flags) {
  s_prf_storage_ble_delete_count++;
  s_prf_storage_ble_store_count++;
}

void shared_prf_storage_erase_ble_pairing_data(void) {
  s_prf_storage_ble_delete_count++;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BT Classic Pairing Data

bool shared_prf_storage_get_bt_classic_pairing_data(struct pbl_bt_addr *addr_out,
                                                    char *device_name_out,
                                                    struct pbl_bt_sm_key *link_key_out,
                                                    uint8_t *platform_bits) {
  return false;
}

void shared_prf_storage_store_bt_classic_pairing_data(struct pbl_bt_addr *addr, char *device_name,
                                                      struct pbl_bt_sm_key *link_key,
                                                      uint8_t platform_bits) {
}

void shared_prf_storage_store_platform_bits(uint8_t platform_bits) {
}

void shared_prf_storage_erase_bt_classic_pairing_data(void) {
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Getting Started Is Complete

bool shared_prf_storage_get_getting_started_complete(void) {
  return true;
}

void shared_prf_storage_set_getting_started_complete(bool set) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Factory Reset

void shared_prf_storage_wipe_all(void) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Pinned Address

bool shared_prf_storage_get_ble_pinned_address(struct pbl_bt_addr *address_out) {
  return false;
}

//! Stores the new BLE Pinned Address in the shared storage.
void shared_prf_storage_set_ble_pinned_address(const struct pbl_bt_addr *address) {
}
