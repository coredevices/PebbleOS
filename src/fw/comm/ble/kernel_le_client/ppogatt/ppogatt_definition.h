/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ppogatt.h"

#include <pbl/bluetooth/pebble_bt.h>

static const Uuid s_ppogatt_service_uuid = {PBL_BT_PEBBLE_UUID_EXPAND(
    PBL_BT_PEBBLE_PPOGATT_SERVICE_UUID_32BIT)};

static const Uuid s_ppogatt_characteristic_uuids[PPoGATTCharacteristicNum] = {
  [PPoGATTCharacteristicData] =
      {
        PBL_BT_PEBBLE_UUID_EXPAND(PBL_BT_PEBBLE_PPOGATT_DATA_CHARACTERISTIC_UUID_32BIT),
      },
  [PPoGATTCharacteristicMeta] = {
    PBL_BT_PEBBLE_UUID_EXPAND(PBL_BT_PEBBLE_PPOGATT_META_CHARACTERISTIC_UUID_32BIT),
  },
};
