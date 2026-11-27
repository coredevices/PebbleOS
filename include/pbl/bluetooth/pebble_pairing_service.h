/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/**
 * @defgroup bluetooth_pebble_pairing_service Pebble Pairing Service
 * @ingroup bluetooth
 * @brief GATT service through which the phone app checks the connection and triggers pairing.
 *
 * The service (@ref PBL_BT_PPS_UUID_16BIT) has a Connectivity Status characteristic (read, notify)
 * and a Trigger Pairing characteristic (read, write). Reading the latter, or writing it without
 * @ref PBL_BT_PPS_TRIGGER_NO_SECURITY_REQUEST, makes the watch ask for security if the link is not
 * encrypted. The central is notified of the status when its link gets encrypted, subscribed or
 * not.
 * @{
 */

/** @brief Bluetooth SIG registered 16-bit UUID of the service. */
#define PBL_BT_PPS_UUID_16BIT 0xFED9U
/** @brief 32-bit UUID of the Connectivity Status characteristic, see PBL_BT_PPS_UUID128_INIT(). */
#define PBL_BT_PPS_CONNECTION_STATUS_UUID_32BIT 1U
/** @brief 32-bit UUID of the Trigger Pairing characteristic, see PBL_BT_PPS_UUID128_INIT(). */
#define PBL_BT_PPS_TRIGGER_PAIRING_UUID_32BIT 2U

/**
 * @brief Expand a 32-bit characteristic UUID with the base UUID of the service,
 * @c XXXXXXXX-328E-0FBB-C642-1AA6699BDADA.
 *
 * Expands to the initializer of a @c ble_uuid128_t.
 *
 * @param u The 32-bit UUID value.
 */
#define PBL_BT_PPS_UUID128_INIT(u)                                                         \
  BLE_UUID128_INIT(0xDA, 0xDA, 0x9B, 0x69, 0xA6, 0x1A, 0x42, 0xC6, 0xBB, 0x0F, 0x8E, 0x32, \
                   (u) & 0xff, ((u) >> 8) & 0xff, ((u) >> 16) & 0xff, ((u) >> 24) & 0xff)

/** @brief Size of the Connectivity Status value: the flags below, two reserved bytes, a zero. */
#define PBL_BT_PPS_CONNECTIVITY_STATUS_SIZE 4U

/**
 * @name Flags of the Connectivity Status value, in its first byte
 *
 * With respect to the device reading it.
 * @{
 */
/** The reading device is connected (always set). */
#define PBL_BT_PPS_STATUS_CONNECTED PBL_BIT(0)
/** The reading device is bonded. */
#define PBL_BT_PPS_STATUS_BONDED PBL_BIT(1)
/** The link is encrypted. */
#define PBL_BT_PPS_STATUS_ENCRYPTED PBL_BIT(2)
/** The watch has a bonding. */
#define PBL_BT_PPS_STATUS_HAS_BONDED_GATEWAY PBL_BIT(3)
/** The watch honours @ref PBL_BT_PPS_TRIGGER_NO_SECURITY_REQUEST (always set). */
#define PBL_BT_PPS_STATUS_NO_SECURITY_REQUEST_SUPPORTED PBL_BIT(4)
/** @} */

/**
 * @name Flags of the byte written to the Trigger Pairing characteristic
 *
 * The other bits are ignored.
 * @{
 */
/** Do not send a security request. */
#define PBL_BT_PPS_TRIGGER_NO_SECURITY_REQUEST PBL_BIT(1)
/** Send a security request even if the link is encrypted. */
#define PBL_BT_PPS_TRIGGER_FORCE_SECURITY_REQUEST PBL_BIT(2)
/** @} */

/** @} */
