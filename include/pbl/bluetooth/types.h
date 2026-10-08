/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

/** @brief Bluetooth error codes. */
enum pbl_bt_errno {
  /** The operation was successful. */
  PBL_BT_ERRNO_OK = 0,

  /** Connection established successfully. */
  PBL_BT_ERRNO_CONNECTED = PBL_BT_ERRNO_OK,

  /** One or more parameters were invalid. */
  PBL_BT_ERRNO_INVALID_PARAMETER = 1,

  /**
   * The connection timed out, for example because the devices went out of range of each other
   * or packets were lost due to RF interference.
   */
  PBL_BT_ERRNO_CONNECTION_TIMEOUT = 2,

  /** The connection was terminated by the remote device. */
  PBL_BT_ERRNO_REMOTELY_TERMINATED = 3,

  /** The connection was terminated by the system. */
  PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_SYSTEM = 4,

  /** The connection was terminated by the application. */
  PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_APP = 5,

  /** The system did not have enough resources for the operation. */
  PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES = 6,

  /** The remote device does not support pairing. */
  PBL_BT_ERRNO_PAIRING_NOT_SUPPORTED = 7,

  /** The pairing failed because the user did not confirm. */
  PBL_BT_ERRNO_PAIRING_CONFIRMATION_FAILED = 8,

  /** The pairing failed because it timed out. */
  PBL_BT_ERRNO_PAIRING_TIME_OUT = 9,

  /** The pairing failed because Out-of-Band data was not available. */
  PBL_BT_ERRNO_PAIRING_OOB_NOT_AVAILABLE = 10,

  /** The requested operation cannot be performed in the current state. */
  PBL_BT_ERRNO_INVALID_STATE = 11,

  /** GATT service discovery timed out. */
  PBL_BT_ERRNO_SERVICE_DISCOVERY_TIMEOUT = 12,

  /** GATT service discovery failed due to disconnection. */
  PBL_BT_ERRNO_SERVICE_DISCOVERY_DISCONNECTED = 13,

  /**
   * GATT service discovery was restarted because the remote device changed its GATT database.
   *
   * Service, characteristic and descriptor references obtained before are invalid. Discovery is
   * restarted automatically.
   */
  PBL_BT_ERRNO_SERVICE_DISCOVERY_DATABASE_CHANGED = 14,

  /**
   * Start of the range of stack errors that could not be mapped to a more meaningful code.
   *
   * The NimBLE backend reports its error codes added to this value.
   */
  PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN = 9000,

  /** Start of the range of HCI errors that could not be mapped to a more meaningful code. */
  PBL_BT_ERRNO_HCI_ERROR_BEGIN = 10000,

  /**
   * Other, uncategorized error.
   *
   * Also the highest allowed value: it must fit in 14 bits, the width used for errors in
   * PebbleBLEGATTClientEvent.
   */
  PBL_BT_ERRNO_OTHER = 0x3fff,
};

/**
 * @brief ATT error codes returned by a GATT server for read, write and subscribe operations.
 *
 * The values up to 0xFF are the codes of the Bluetooth Core Specification, Vol 3, Part F,
 * 3.4.1.1, and the common profile error codes. The values from 0x100 are local errors.
 */
enum pbl_bt_gatt_error {
  /** Success. */
  PBL_BT_GATT_ERROR_SUCCESS = 0x00,
  /** The attribute handle is invalid on this server. */
  PBL_BT_GATT_ERROR_INVALID_HANDLE = 0x01,
  /** The attribute cannot be read. */
  PBL_BT_GATT_ERROR_READ_NOT_PERMITTED = 0x02,
  /** The attribute cannot be written. */
  PBL_BT_GATT_ERROR_WRITE_NOT_PERMITTED = 0x03,
  /** The attribute PDU was invalid. */
  PBL_BT_GATT_ERROR_INVALID_PDU = 0x04,
  /** The attribute requires authentication. */
  PBL_BT_GATT_ERROR_INSUFFICIENT_AUTHENTICATION = 0x05,
  /** The server does not support the request. */
  PBL_BT_GATT_ERROR_REQUEST_NOT_SUPPORTED = 0x06,
  /** The offset is past the end of the attribute. */
  PBL_BT_GATT_ERROR_INVALID_OFFSET = 0x07,
  /** The attribute requires authorization. */
  PBL_BT_GATT_ERROR_INSUFFICIENT_AUTHORIZATION = 0x08,
  /** Too many prepare writes have been queued. */
  PBL_BT_GATT_ERROR_PREPARE_QUEUE_FULL = 0x09,
  /** No attribute found in the given handle range. */
  PBL_BT_GATT_ERROR_ATTRIBUTE_NOT_FOUND = 0x0A,
  /** The attribute cannot be read with a Read Blob request. */
  PBL_BT_GATT_ERROR_ATTRIBUTE_NOT_LONG = 0x0B,
  /** The encryption key size is insufficient. */
  PBL_BT_GATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE = 0x0C,
  /** The attribute value length is invalid. */
  PBL_BT_GATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH = 0x0D,
  /** The request failed for an unlikely reason. */
  PBL_BT_GATT_ERROR_UNLIKELY_ERROR = 0x0E,
  /** The attribute requires encryption. */
  PBL_BT_GATT_ERROR_INSUFFICIENT_ENCRYPTION = 0x0F,
  /** The attribute type is not a supported grouping attribute. */
  PBL_BT_GATT_ERROR_UNSUPPORTED_GROUP_TYPE = 0x10,
  /** The server ran out of resources. */
  PBL_BT_GATT_ERROR_INSUFFICIENT_RESOURCES = 0x11,

  /** First application specific error code. */
  PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_START = 0x80,
  /** Last application specific error code. */
  PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_END = 0xFC,

  /** A Client Characteristic Configuration Descriptor is improperly configured. */
  PBL_BT_GATT_ERROR_CCCD_IMPROPERLY_CONFIGURED = 0xFD,
  /** A request is already in progress. */
  PBL_BT_GATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS = 0xFE,
  /** The value is out of range. */
  PBL_BT_GATT_ERROR_OUT_OF_RANGE = 0xFF,

  /** The request timed out. */
  PBL_BT_GATT_ERROR_REQUEST_TIME_OUT = 0x100,
  /** The data of a prepared write did not match what was sent. */
  PBL_BT_GATT_ERROR_REQUEST_PREPARE_WRITE_DATA_MISMATCH = 0x101,
  /** The local device ran out of resources. */
  PBL_BT_GATT_ERROR_LOCAL_INSUFFICIENT_RESOURCES = 0x102,
};

/**
 * @brief Map a legacy stack error to enum pbl_bt_errno.
 *
 * @param e Stack error code.
 */
#define PBL_BT_ERRNO_WITH_INTERNAL_ERROR(e) ((int)PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN - e)

/**
 * @brief Map an HCI error to enum pbl_bt_errno.
 *
 * @param e HCI error code.
 */
#define PBL_BT_ERRNO_WITH_HCI_ERROR(e) ((int)PBL_BT_ERRNO_HCI_ERROR_BEGIN + e)

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

/** @brief Opaque reference to a service object. */
typedef uintptr_t pbl_bt_service_t;

/** @brief Opaque reference to a characteristic object. */
typedef uintptr_t pbl_bt_characteristic_t;

/** @brief Opaque reference to a descriptor object. */
typedef uintptr_t pbl_bt_descriptor_t;

_Static_assert(sizeof(pbl_bt_descriptor_t) == sizeof(uintptr_t),
               "pbl_bt_descriptor_t is invalid size");
_Static_assert(sizeof(pbl_bt_characteristic_t) == sizeof(uintptr_t),
               "pbl_bt_characteristic_t is invalid size");

/** @brief Invalid service reference. */
#define PBL_BT_SERVICE_INVALID ((pbl_bt_service_t)0)
/** @brief Invalid characteristic reference. */
#define PBL_BT_CHARACTERISTIC_INVALID ((pbl_bt_characteristic_t)0)
/** @brief Invalid descriptor reference. */
#define PBL_BT_DESCRIPTOR_INVALID ((pbl_bt_descriptor_t)0)

/**
 * @brief Identifier of a bonding.
 *
 * Stable across reboots, so it can be persisted.
 */
typedef uint8_t pbl_bt_bonding_id_t;

/** @brief Invalid bonding identifier. */
#define PBL_BT_BONDING_ID_INVALID (0xFFU)

/** @brief Identifier of a stored Client Characteristic Configuration Descriptor entry. */
typedef uint16_t pbl_bt_cccd_id_t;

/** @brief Invalid CCCD entry identifier. */
#define PBL_BT_CCCD_ID_INVALID (0xFFU)

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
 * @brief PBL_BT_ADDR_XPLODE() taking a pointer.
 *
 * @param a Pointer to a struct pbl_bt_addr.
 */
#define PBL_BT_ADDR_XPLODE_PTR(a) \
  (a)->octets[5], (a)->octets[4], (a)->octets[3], (a)->octets[2], (a)->octets[1], (a)->octets[0]

/**
 * @brief Remote Bluetooth device, as exposed to applications.
 *
 * Opaque: the layout is struct pbl_bt_device_internal.
 */
struct pbl_bt_device {
  union {
    /** Opaque storage, as 32-bit words. */
    uint32_t opaque[2];
    /** Opaque storage, as a 64-bit word. */
    uint64_t opaque_64;
  };
};

/**
 * @brief Remote Bluetooth device: its address and address type.
 *
 * The layout of struct pbl_bt_device. It is not exported to applications and its size must never
 * change.
 */
struct PBL_PACKED pbl_bt_device_internal {
  union {
    struct PBL_PACKED {
      /** Device address. */
      struct pbl_bt_addr address;
      /** True for a BR/EDR device, false for an LE device. */
      bool is_classic : 1;
      /** True if @ref address is a random address, false if it is a public address. */
      bool is_random_address : 1;
      /**
       * Unused, zero.
       *
       * When adding flags, also update the @c bt_device_bits field of PebbleBLEGATTClientEvent
       * and PebbleBLEConnectionEvent.
       */
      uint16_t zero : 14;
    };
    /** The same device, as the opaque type. */
    struct pbl_bt_device opaque;
  };
};

/** @brief Invalid (all zero) struct pbl_bt_device. */
#define PBL_BT_DEVICE_INVALID ((const struct pbl_bt_device){})
/** @brief Invalid (all zero) struct pbl_bt_device_internal. */
#define PBL_BT_DEVICE_INTERNAL_INVALID ((const struct pbl_bt_device_internal){})

_Static_assert(sizeof(struct pbl_bt_device_internal) == sizeof(struct pbl_bt_device),
               "struct pbl_bt_device_internal should be equal in size to struct pbl_bt_device");

struct pbl_bt_ad_data;

/** @brief Maximum size in bytes of advertising data or scan response data. */
#define PBL_BT_AD_REPORT_DATA_MAX_LENGTH (31)

/**
 * @name Advertising flags
 * Bits of the Flags AD type, see Supplement to the Bluetooth Core Specification, Part A, 1.3.
 * @{
 */
/** @brief LE Limited Discoverable Mode. */
#define PBL_BT_AD_FLAGS_LIM_DISCOVERABLE_MASK (1 << 0)
/** @brief LE General Discoverable Mode. */
#define PBL_BT_AD_FLAGS_GEN_DISCOVERABLE_MASK (1 << 1)
/** @brief BR/EDR Not Supported. */
#define PBL_BT_AD_FLAGS_BR_EDR_NOT_SUPPORTED_MASK (1 << 2)
/** @brief Simultaneous LE and BR/EDR to Same Device Capable (Controller). */
#define PBL_BT_AD_FLAGS_LE_BR_EDR_SIMULT_CONTROLLER_MASK (1 << 3)
/** @brief Simultaneous LE and BR/EDR to Same Device Capable (Host). */
#define PBL_BT_AD_FLAGS_LE_BR_EDR_SIMULT_HOST_MASK (1 << 4)
/** @} */

/** @brief Minimum connection interval, in 1.25 ms slots. */
#define PBL_BT_LL_CONN_INTV_MIN_SLOTS (6) // 1.25ms / slot
/** @brief Maximum connection interval, in 1.25 ms slots. */
#define PBL_BT_LL_CONN_INTV_MAX_SLOTS (3200) // 1.25ms / slot
/** @brief Minimum supervision timeout in milliseconds. */
#define PBL_BT_LL_SUPERVISION_TIMEOUT_MIN_MS (100)

/**
 * @brief Advertising data and scan response data.
 *
 * Variable length: allocate it with room for both payloads after the header.
 */
struct pbl_bt_ad_data {
  /** Length of the advertising data in bytes. */
  uint8_t ad_data_length;

  /** Length of the scan response data in bytes. */
  uint8_t scan_resp_data_length;

  /** The raw advertising data, followed by the raw scan response data. */
  uint8_t data[0];
};

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
