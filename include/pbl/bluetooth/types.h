/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_types Types
 * @ingroup bluetooth
 * @brief Addresses and how to print them.
 *
 * Addresses are stored least significant octet first, as on air. Use PBL_BT_ADDR_FMT with
 * PBL_BT_ADDR_XPLODE() to print them most significant octet first:
 *
 * @code{.c}
 * PBL_LOG_DBG("Peer " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(address));
 * @endcode
 * @{
 */

/** @brief Bluetooth device address. */
struct PBL_PACKED pbl_bt_addr {
  /** Address octets, least significant first. */
  uint8_t octets[6];
};

/**
 * @brief printf() format of an address, as @c XX:XX:XX:XX:XX:XX.
 *
 * Use with PBL_BT_ADDR_XPLODE() or PBL_BT_ADDR_XPLODE_VAL().
 */
#define PBL_BT_ADDR_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
/** @brief Buffer size for an address printed with PBL_BT_ADDR_FMT, including the terminator. */
#define PBL_BT_ADDR_FMT_BUFFER_SIZE (18)

/**
 * @brief printf() format of an address, as @c 0xXXXXXXXXXXXX.
 *
 * Use with PBL_BT_ADDR_XPLODE() or PBL_BT_ADDR_XPLODE_VAL().
 */
#define PBL_BT_BD_ADDR_FMT "0x%02X%02X%02X%02X%02X%02X"
/** @brief Buffer size for an address printed with PBL_BT_BD_ADDR_FMT, including the terminator. */
#define PBL_BT_BD_ADDR_FMT_BUFFER_SIZE (15)

/** @brief Buffer size of a device name, including the terminator. */
#define PBL_BT_DEVICE_NAME_BUFFER_SIZE (20)

/**
 * @brief Expand an address into the printf() arguments of PBL_BT_ADDR_FMT, most significant
 * octet first.
 *
 * @param a struct pbl_bt_addr value.
 */
#define PBL_BT_ADDR_XPLODE(a) \
  (a).octets[5], (a).octets[4], (a).octets[3], (a).octets[2], (a).octets[1], (a).octets[0]

/**
 * @brief PBL_BT_ADDR_XPLODE() for the octets of an address, e.g. the @c val of a @c ble_addr_t.
 *
 * @param v The six octets, least significant first.
 */
#define PBL_BT_ADDR_XPLODE_VAL(v) (v)[5], (v)[4], (v)[3], (v)[2], (v)[1], (v)[0]

/** @} */
