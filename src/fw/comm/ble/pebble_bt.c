/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/pebble_bt.h>

#include <stddef.h>
#include <string.h>

void pebble_bt_uuid_expand(Uuid *uuid, uint32_t value) {
  static const uint8_t pebble_base_uuid_last_12_bytes[] = {
    0x32, 0x8E, 0x0F, 0xBB,
    0xC6, 0x42, 0x1A, 0xA6,
    0x69, 0x9B, 0xDA, 0xDA,
  };
  memcpy(&uuid->byte4, &pebble_base_uuid_last_12_bytes, sizeof(Uuid) - offsetof(Uuid, byte4));
  uuid->byte0 = (value >> 24) & 0xFF;
  uuid->byte1 = (value >> 16) & 0xFF;
  uuid->byte2 = (value >> 8) & 0xFF;
  uuid->byte3 = (value >> 0) & 0xFF;
}
