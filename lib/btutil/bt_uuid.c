/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/btutil/bt_uuid.h"
#include <pbl/bluetooth/types.h>

#include <stdint.h>

Uuid bt_uuid_expand_16bit(uint16_t uuid16) {
  return bt_uuid_expand_32bit(uuid16);
}

Uuid bt_uuid_expand_32bit(uint32_t uuid32) {
  return (const Uuid){PBL_BT_SIG_UUID_EXPAND(uuid32)};
}
