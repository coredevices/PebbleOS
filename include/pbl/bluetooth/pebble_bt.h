/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include "pbl/util/uuid.h"

/**
 * @defgroup bluetooth_pebble_bt Pebble identifiers
 * @ingroup bluetooth
 * @brief Pebble-specific Bluetooth identifiers: service and characteristic UUIDs.
 *
 * Pebble UUIDs are 32-bit values expanded with the Pebble Base UUID,
 * @c XXXXXXXX-328E-0FBB-C642-1AA6699BDADA, either at run time or at compile time:
 *
 * @code{.c}
 * Uuid uuid;
 * pbl_bt_pebble_uuid_expand(&uuid, PBL_BT_PEBBLE_PPOGATT_SERVICE_UUID_32BIT);
 *
 * static const Uuid s_app_launch_uuid = {
 *   PBL_BT_PEBBLE_UUID_EXPAND(PBL_BT_PEBBLE_APP_LAUNCH_SERVICE_UUID_32BIT),
 * };
 * @endcode
 * @{
 */

/** @brief Bluetooth SIG registered 16-bit UUID of Pebble Technology, the Pebble service. */
#define PBL_BT_PPS_UUID_16BIT (0xFED9)

/** @brief Service UUID of Pebble Protocol over GATT (PPoGATT), the phone being the server. */
#define PBL_BT_PEBBLE_PPOGATT_SERVICE_UUID_32BIT (0x10000000)
/** @brief PPoGATT data characteristic. */
#define PBL_BT_PEBBLE_PPOGATT_DATA_CHARACTERISTIC_UUID_32BIT (0x10000001)
/** @brief PPoGATT meta characteristic. */
#define PBL_BT_PEBBLE_PPOGATT_META_CHARACTERISTIC_UUID_32BIT (0x10000002)

/**
 * @brief Service UUID of the V1 watch-as-server PPoGATT, as shipped on Pebble 2 (DA1468x).
 *
 * Reserved so new allocations don't collide.
 */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_V1_SERVICE_UUID_32BIT (0x30000003)
/** @brief V1 watch-as-server PPoGATT data characteristic. */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_V1_DATA_CHARACTERISTIC_UUID_32BIT (0x30000004)
/** @brief V1 watch-as-server PPoGATT meta characteristic. */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_V1_META_CHARACTERISTIC_UUID_32BIT (0x30000005)
/** @brief V1 watch-as-server PPoGATT data write characteristic. */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_V1_DATA_WR_CHARACTERISTIC_UUID_32BIT (0x30000006)

/**
 * @brief Service UUID of the V2 "reversed" PPoGATT, see @ref bluetooth_ppog_reversed.
 *
 * The phone is the GATT client and sends the first ResetRequest after subscribing. There is no
 * meta characteristic; 0x40000002 is reserved in case a future revision needs one.
 */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_SERVICE_UUID_32BIT (0x40000000)
/** @brief Reversed PPoGATT data characteristic, notified by the watch. */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_DATA_CHARACTERISTIC_UUID_32BIT (0x40000001)
/** @brief Reversed PPoGATT data write characteristic, written by the phone. */
#define PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_DATA_WR_CHARACTERISTIC_UUID_32BIT (0x40000003)

/** @brief Service UUID of the Pebble App Launch service. */
#define PBL_BT_PEBBLE_APP_LAUNCH_SERVICE_UUID_32BIT (0x20000000)
/** @brief Pebble App Launch characteristic. */
#define PBL_BT_PEBBLE_APP_LAUNCH_CHARACTERISTIC_UUID_32BIT (0x20000001)

/**
 * @brief Expand a 32-bit (or 16-bit) Pebble UUID with the Pebble Base UUID.
 *
 * See bt_uuid_expand_32bit() and bt_uuid_expand_16bit() to expand with the Bluetooth SIG Base
 * UUID instead.
 *
 * @param[out] uuid The expanded UUID.
 * @param value The 32-bit (or 16-bit) UUID value.
 */
void pbl_bt_pebble_uuid_expand(Uuid *uuid, uint32_t value);

/**
 * @brief Compile-time version of pbl_bt_pebble_uuid_expand().
 *
 * Expands to the 16 bytes of the UUID, as an initializer list.
 *
 * @param u The 32-bit (or 16-bit) UUID value.
 */
#define PBL_BT_PEBBLE_UUID_EXPAND(u)                                                           \
  (0xff & ((uint32_t)u) >> 24), (0xff & ((uint32_t)u) >> 16), (0xff & ((uint32_t)u) >> 8),     \
      (0xff & ((uint32_t)u) >> 0), 0x32, 0x8E, 0x0F, 0xBB, 0xC6, 0x42, 0x1A, 0xA6, 0x69, 0x9B, \
      0xDA, 0xDA

/** @} */
