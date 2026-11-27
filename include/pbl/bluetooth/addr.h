/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup bluetooth_addr Addresses
 * @ingroup bluetooth
 * @brief How to print addresses.
 *
 * Addresses are stored least significant octet first, as on air. Use PBL_BT_ADDR_FMT with
 * PBL_BT_ADDR_XPLODE() to print them most significant octet first:
 *
 * @code{.c}
 * PBL_LOG_DBG("Peer " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(addr.val));
 * @endcode
 * @{
 */

/** @brief printf() format of an address, as @c XX:XX:XX:XX:XX:XX. Use with PBL_BT_ADDR_XPLODE(). */
#define PBL_BT_ADDR_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
/** @brief Buffer size for an address printed with PBL_BT_ADDR_FMT, including the terminator. */
#define PBL_BT_ADDR_FMT_BUFFER_SIZE (18)

/**
 * @brief Expand an address into the printf() arguments of PBL_BT_ADDR_FMT, most significant
 * octet first.
 *
 * @param v Its six octets, least significant first, e.g. the @c val of a @c ble_addr_t.
 */
#define PBL_BT_ADDR_XPLODE(v) (v)[5], (v)[4], (v)[3], (v)[2], (v)[1], (v)[0]

/** @} */
