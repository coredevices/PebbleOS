/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ppogatt.h"

#include <bluetooth/pebble_bt.h>

static const Uuid s_ppogatt_service_uuid = {
  PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_PPOGATT_SERVICE_UUID_32BIT)
};

static const Uuid s_ppogatt_characteristic_uuids[PPoGATTCharacteristicNum] = {
  [PPoGATTCharacteristicData] = {
    PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_PPOGATT_DATA_CHARACTERISTIC_UUID_32BIT),
  },
  [PPoGATTCharacteristicMeta] = {
    PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_PPOGATT_META_CHARACTERISTIC_UUID_32BIT),
  },
};
