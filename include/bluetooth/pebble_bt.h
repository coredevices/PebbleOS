/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include "pbl/util/uuid.h"

//! @file
//! This file contains Pebble-specific Bluetooth identifiers (numbers, UUIDs, etc.)
//! Also see https://pebbletechnology.atlassian.net/wiki/display/DEV/Pebble+GATT+Services

//! Our Bluetooth-SIG-Registered 16-bit UUID:
//! Pebble Technology Corporation
//! Pebble Smartwatch Service
#define PEBBLE_BT_PAIRING_SERVICE_UUID_16BIT (0xFED9)

//! The Service UUID of the "Pebble Protocol over GATT" (PPoGATT) service.
//! This UUID needs to be expanded using the Pebble Base UUID (@see pebble_bt_uuid_expand)
#define PEBBLE_BT_PPOGATT_SERVICE_UUID_32BIT             (0x10000000)
#define PEBBLE_BT_PPOGATT_DATA_CHARACTERISTIC_UUID_32BIT (0x10000001)
#define PEBBLE_BT_PPOGATT_META_CHARACTERISTIC_UUID_32BIT (0x10000002)

//! V1 watch-as-server PPoGATT, as shipped on Pebble 2 (DA1468x). Reserved so
//! new allocations don't collide.
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_V1_SERVICE_UUID_32BIT             (0x30000003)
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_V1_DATA_CHARACTERISTIC_UUID_32BIT (0x30000004)
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_V1_META_CHARACTERISTIC_UUID_32BIT (0x30000005)
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_V1_DATA_WR_CHARACTERISTIC_UUID_32BIT (0x30000006)

//! V2 "reversed" PPoGATT: the phone is the GATT client and sends the first
//! ResetRequest after subscribing. No meta characteristic (0x40000002 is
//! reserved in case a future revision wants one).
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_SERVICE_UUID_32BIT             (0x40000000)
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_DATA_CHARACTERISTIC_UUID_32BIT (0x40000001)
#define PEBBLE_BT_PPOGATT_WATCH_SERVER_DATA_WR_CHARACTERISTIC_UUID_32BIT (0x40000003)

//! The Service UUID of the "Pebble App Launch" service.
//! This UUID needs to be expanded using the Pebble Base UUID (@see pebble_bt_uuid_expand)
#define PEBBLE_BT_APP_LAUNCH_SERVICE_UUID_32BIT             (0x20000000)
#define PEBBLE_BT_APP_LAUNCH_CHARACTERISTIC_UUID_32BIT      (0x20000001)

//! Assigns a 32-bit (or 16-bit) UUID that is based on the Pebble Base UUID,
//! XXXXXXXX-328E-0FBB-C642-1AA6699BDADA.
//! @param uuid The UUID storage to assign the constructed UUID to.
//! @param value The 32-bit (or 16-bit) UUID value to use.
//! @see bt_uuid_expand_32bit and bt_uuid_expand_16bit for functions that expand
//! using the BT SIG's Base UUID.
void pebble_bt_uuid_expand(Uuid *uuid, uint32_t value);

//! Macro that does the same as pebble_bt_uuid_expand, but then at compile-time
#define PEBBLE_BT_UUID_EXPAND(u) \
  (0xff & ((uint32_t) u) >> 24), \
  (0xff & ((uint32_t) u) >> 16), \
  (0xff & ((uint32_t) u) >> 8), \
  (0xff & ((uint32_t) u) >> 0), \
  0x32, 0x8E, 0x0F, 0xBB, \
  0xC6, 0x42, 0x1A, 0xA6, \
  0x69, 0x9B, 0xDA, 0xDA
