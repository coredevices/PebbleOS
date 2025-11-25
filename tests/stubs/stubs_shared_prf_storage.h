/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/shared_prf_storage/shared_prf_storage.h"

void shared_prf_storage_erase_ble_pairing_data(void) {
}

void shared_prf_storage_erase_bt_classic_pairing_data(void) {
}

bool shared_prf_storage_get_local_device_name(char *local_device_name_out, size_t max_size) {
  return false;
}

void shared_prf_storage_set_local_device_name(const char *local_device_name) {
}

bool shared_prf_storage_get_root_key(SMRootKeyType key_type, SM128BitKey *key_out) {
  return false;
}

void shared_prf_storage_set_root_keys(SM128BitKey *keys_in) {
}


bool shared_prf_storage_get_ble_pairing_data(SMPairingInfo *pairing_info_out,
                                             char *name_out, bool *requires_address_pinning_out,
                                             uint8_t *flags) {
  return false;
}

void shared_prf_storage_store_ble_pairing_data(const SMPairingInfo *pairing_info,
                                               const char *name, bool requires_address_pinning,
                                               uint8_t flags) {
}

bool shared_prf_storage_get_ble_pinned_address(BTDeviceAddress *address_out) {
  return false;
}

void shared_prf_storage_set_ble_pinned_address(const BTDeviceAddress *address) {
}

bool shared_prf_storage_get_bt_classic_pairing_data(BTDeviceAddress *addr_out,
                                                    char *device_name_out,
                                                    SM128BitKey *link_key_out,
                                                    uint8_t *platform_bits) {
  return false;
}

void shared_prf_storage_store_bt_classic_pairing_data(BTDeviceAddress *addr,
                                                      const char *device_name,
                                                      SM128BitKey *link_key,
                                                      uint8_t platform_bits) {
}

void shared_prf_storage_store_platform_bits(uint8_t platform_bits) {
}

void shared_prf_store_pairing_data(SMPairingInfo *pairing_info,
                                   const char *device_name_ble,
                                   BTDeviceAddress *addr,
                                   const char *device_name_classic,
                                   SM128BitKey *link_key,
                                   uint8_t platform_bits) {
}

bool shared_prf_storage_get_getting_started_complete(void) {
  return false;
}

void shared_prf_storage_set_getting_started_complete(bool set) {
}

void shared_prf_storage_wipe_all(void) {
}
