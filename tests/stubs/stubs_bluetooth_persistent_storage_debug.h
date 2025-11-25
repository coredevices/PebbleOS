/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/sm_types.h>
#include <btutil/sm_util.h>

void bluetooth_persistent_storage_debug_dump_ble_pairing_info(
  char *display_buf, const SMPairingInfo *info) {
}

void bluetooth_persistent_storage_debug_dump_classic_pairing_info(
  char *display_buf, BTDeviceAddress *addr, char *device_name, SM128BitKey *link_key,
  uint8_t platform_bits) {
}

void bluetooth_persistent_storage_debug_dump_root_keys(SM128BitKey *irk, SM128BitKey *erk) {
}
