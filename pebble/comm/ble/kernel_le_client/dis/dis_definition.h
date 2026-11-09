/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dis.h"

//! DIS Service UUID - 0000180A-0000-1000-8000-00805F9B34FB
static const Uuid s_dis_service_uuid = {PBL_BT_SIG_UUID_EXPAND(0x180A)};

//! DIS Characteristic UUIDs
static const Uuid s_dis_characteristic_uuids[NumDISCharacteristic] = {
  //! Manufacturer Name String - 00002A29-0000-1000-8000-00805F9B34FB
  [DISCharacteristicManufacturerNameString] = {PBL_BT_SIG_UUID_EXPAND(0x2A29)},
};
