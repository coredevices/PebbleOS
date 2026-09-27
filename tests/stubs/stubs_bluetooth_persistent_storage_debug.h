/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/btutil/sm_util.h>

void bluetooth_persistent_storage_debug_dump_ble_pairing_info(
    char *display_buf, const struct pbl_bt_sm_pairing_info *info) {
}

void bluetooth_persistent_storage_debug_dump_classic_pairing_info(char *display_buf,
                                                                  struct pbl_bt_addr *addr,
                                                                  char *device_name,
                                                                  struct pbl_bt_sm_key *link_key,
                                                                  uint8_t platform_bits) {
}

void bluetooth_persistent_storage_debug_dump_root_keys(struct pbl_bt_sm_key *irk,
                                                       struct pbl_bt_sm_key *erk) {
}
