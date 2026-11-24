/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup bluetooth_types Types
 * @ingroup bluetooth
 * @brief Error codes, addresses, devices and advertising data shared by the Bluetooth API.
 *
 * Addresses are stored least significant octet first, as on air. Use PBL_BT_ADDR_FMT with
 * PBL_BT_ADDR_XPLODE() to print them most significant octet first:
 *
 * @code{.c}
 * PBL_LOG_DBG("Peer " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(device.address));
 * @endcode
 * @{
 */

/**
 * @brief Property bits of a characteristic.
 *
 * The low byte holds the properties of the Bluetooth Core Specification, Vol 3, Part G, 3.3.1.1.
 */
enum pbl_bt_attribute_property {
  /** No properties. */
  PBL_BT_ATTRIBUTE_PROPERTY_NONE = 0,
  /** The value may be broadcast. */
  PBL_BT_ATTRIBUTE_PROPERTY_BROADCAST = (1 << 0),
  /** The value may be read. */
  PBL_BT_ATTRIBUTE_PROPERTY_READ = (1 << 1),
  /** The value may be written without response. */
  PBL_BT_ATTRIBUTE_PROPERTY_WRITE_WITHOUT_RESPONSE = (1 << 2),
  /** The value may be written. */
  PBL_BT_ATTRIBUTE_PROPERTY_WRITE = (1 << 3),
  /** The value may be notified. */
  PBL_BT_ATTRIBUTE_PROPERTY_NOTIFY = (1 << 4),
  /** The value may be indicated. */
  PBL_BT_ATTRIBUTE_PROPERTY_INDICATE = (1 << 5),
  /** The value may be written with an authenticated signed write. */
  PBL_BT_ATTRIBUTE_PROPERTY_AUTHENTICATED_SIGNED_WRITES = (1 << 6),
  /** Extended properties are in the Characteristic Extended Properties descriptor. */
  PBL_BT_ATTRIBUTE_PROPERTY_EXTENDED_PROPERTIES = (1 << 7),

  /** Reading requires an encrypted link. Only for attributes hosted by the local server. */
  PBL_BT_ATTRIBUTE_PROPERTY_READING_REQUIRES_ENCRYPTION = (1 << 8),
  /** Writing requires an encrypted link. Only for attributes hosted by the local server. */
  PBL_BT_ATTRIBUTE_PROPERTY_WRITING_REQUIRES_ENCRYPTION = (1 << 9),
};

/** @brief Bluetooth device address. */
struct PBL_PACKED pbl_bt_addr {
  /** Address octets, least significant first. */
  uint8_t octets[6];
};

/** @brief Size of a struct pbl_bt_addr in bytes. */
#define PBL_BT_ADDR_SIZE (sizeof(struct pbl_bt_addr))

/**
 * @brief printf() format of an address, as @c XX:XX:XX:XX:XX:XX.
 *
 * Use with PBL_BT_ADDR_XPLODE() or PBL_BT_ADDR_XPLODE_PTR().
 */
#define PBL_BT_ADDR_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
/** @brief Buffer size for an address printed with PBL_BT_ADDR_FMT, including the terminator. */
#define PBL_BT_ADDR_FMT_BUFFER_SIZE (18)

/**
 * @brief printf() format of an address, as @c 0xXXXXXXXXXXXX.
 *
 * Use with PBL_BT_ADDR_XPLODE() or PBL_BT_ADDR_XPLODE_PTR().
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

/**
 * @brief PBL_BT_ADDR_XPLODE() taking a pointer.
 *
 * @param a Pointer to a struct pbl_bt_addr.
 */
#define PBL_BT_ADDR_XPLODE_PTR(a) \
  (a)->octets[5], (a)->octets[4], (a)->octets[3], (a)->octets[2], (a)->octets[1], (a)->octets[0]

/** @brief Minimum connection interval, in 1.25 ms slots. */
#define PBL_BT_LL_CONN_INTV_MIN_SLOTS (6) // 1.25ms / slot
/** @brief Maximum connection interval, in 1.25 ms slots. */
#define PBL_BT_LL_CONN_INTV_MAX_SLOTS (3200) // 1.25ms / slot
/** @brief Minimum supervision timeout in milliseconds. */
#define PBL_BT_LL_SUPERVISION_TIMEOUT_MIN_MS (100)

/**
 * @brief Compile-time version of bt_uuid_expand_32bit() and bt_uuid_expand_16bit().
 *
 * Expands to the 16 bytes of the UUID, as an initializer list.
 *
 * @param u 16-bit or 32-bit UUID.
 */
#define PBL_BT_SIG_UUID_EXPAND(u)                                                              \
  (0xff & ((uint32_t)u) >> 24), (0xff & ((uint32_t)u) >> 16), (0xff & ((uint32_t)u) >> 8),     \
      (0xff & ((uint32_t)u) >> 0), 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, \
      0x34, 0xFB

/** @} */
