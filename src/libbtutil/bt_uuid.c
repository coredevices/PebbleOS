/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bt_uuid.h"
#include <bluetooth/bluetooth_types.h>

#include <stdint.h>

Uuid bt_uuid_expand_16bit(uint16_t uuid16) {
  return bt_uuid_expand_32bit(uuid16);
}

Uuid bt_uuid_expand_32bit(uint32_t uuid32) {
  return (const Uuid) { BT_UUID_EXPAND(uuid32) };
}
