/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "pbl/util/uuid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//! Bluetooth error codes.
enum pbl_bt_errno {
  //! The operation was successful.
  PBL_BT_ERRNO_OK = 0,

  //! Connection established successfully.
  PBL_BT_ERRNO_CONNECTED = PBL_BT_ERRNO_OK,

  //! One or more parameters were invalid.
  PBL_BT_ERRNO_INVALID_PARAMETER = 1,

  //! The connection was terminated because it timed out. Examples of cause for
  //! a connection timeout are: devices going out of range of each other or
  //! lost packets due to RF interference.
  PBL_BT_ERRNO_CONNECTION_TIMEOUT = 2,

  //! The connection was terminated by the remote device.
  PBL_BT_ERRNO_REMOTELY_TERMINATED = 3,

  //! The connection was terminated by the system.
  PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_SYSTEM = 4,

  //! The connection was terminated by the application.
  PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_APP = 5,

  //! The system did not have enough resources for the operation.
  PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES = 6,

  //! The remote device does not support pairing.
  PBL_BT_ERRNO_PAIRING_NOT_SUPPORTED = 7,

  //! The pairing failed because the user did not confirm.
  PBL_BT_ERRNO_PAIRING_CONFIRMATION_FAILED = 8,

  //! The pairing failed because it timed out.
  PBL_BT_ERRNO_PAIRING_TIME_OUT = 9,

  //! The pairing failed because Out-of-Band data was not available.
  PBL_BT_ERRNO_PAIRING_OOB_NOT_AVAILABLE = 10,

  //! The requested operation cannot be performed in the current state.
  PBL_BT_ERRNO_INVALID_STATE = 11,

  //! GATT Service Discovery timed out
  PBL_BT_ERRNO_SERVICE_DISCOVERY_TIMEOUT = 12,

  //! GATT Service Discovery failed due to disconnection
  PBL_BT_ERRNO_SERVICE_DISCOVERY_DISCONNECTED = 13,

  //! GATT Service Discovery was restarted because the remote device indicated that it changed
  //! its GATT database. Prior pbl_bt_service_t, pbl_bt_characteristic_t and pbl_bt_descriptor_t
  //! handles must be invalidated when receiving this status code. The system will automatically
  //! start the service discovery process again, therefore apps do not need to call
  //! ble_client_discover_services_and_characteristics() again.
  PBL_BT_ERRNO_SERVICE_DISCOVERY_DATABASE_CHANGED = 14,

  //! Errors after this value are internal Bluetooth stack errors that could not
  //! be mapped onto more meaningful errors by the system.
  PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN = 9000,

  //! Errors after this fvalue are HCI errors that could not be mapped into more
  //! meaningful errors by the system.
  PBL_BT_ERRNO_HCI_ERROR_BEGIN = 10000,

  //! Other, uncategorized error.
  //! @internal This is also the highest allowed value (14 bits all set).
  //! See PebbleBLEGATTClientEvent for why.
  PBL_BT_ERRNO_OTHER = 0x3fff,
};

//! Error values that can be returned by the server in response to read, write
//! and subscribe operations. These error values correspond to the (G)ATT error
//! codes as specified in the Bluetooth 4.0 Specification, Volume 3, Part F,
//! 3.4.1.1, Table 3.3.
enum pbl_bt_gatt_error {
  PBL_BT_GATT_ERROR_SUCCESS = 0x00,
  PBL_BT_GATT_ERROR_INVALID_HANDLE = 0x01,
  PBL_BT_GATT_ERROR_READ_NOT_PERMITTED = 0x02,
  PBL_BT_GATT_ERROR_WRITE_NOT_PERMITTED = 0x03,
  PBL_BT_GATT_ERROR_INVALID_PDU = 0x04,
  PBL_BT_GATT_ERROR_INSUFFICIENT_AUTHENTICATION = 0x05,
  PBL_BT_GATT_ERROR_REQUEST_NOT_SUPPORTED = 0x06,
  PBL_BT_GATT_ERROR_INVALID_OFFSET = 0x07,
  PBL_BT_GATT_ERROR_INSUFFICIENT_AUTHORIZATION = 0x08,
  PBL_BT_GATT_ERROR_PREPARE_QUEUE_FULL = 0x09,
  PBL_BT_GATT_ERROR_ATTRIBUTE_NOT_FOUND = 0x0A,
  PBL_BT_GATT_ERROR_ATTRIBUTE_NOT_LONG = 0x0B,
  PBL_BT_GATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE = 0x0C,
  PBL_BT_GATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH = 0x0D,
  PBL_BT_GATT_ERROR_UNLIKELY_ERROR = 0x0E,
  PBL_BT_GATT_ERROR_INSUFFICIENT_ENCRYPTION = 0x0F,
  PBL_BT_GATT_ERROR_UNSUPPORTED_GROUP_TYPE = 0x10,
  PBL_BT_GATT_ERROR_INSUFFICIENT_RESOURCES = 0x11,

  PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_START = 0x80,
  PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_END = 0xFC,

  PBL_BT_GATT_ERROR_CCCD_IMPROPERLY_CONFIGURED = 0xFD,
  PBL_BT_GATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS = 0xFE,
  PBL_BT_GATT_ERROR_OUT_OF_RANGE = 0xFF,

  PBL_BT_GATT_ERROR_REQUEST_TIME_OUT = 0x100,
  PBL_BT_GATT_ERROR_REQUEST_PREPARE_WRITE_DATA_MISMATCH = 0x101,
  PBL_BT_GATT_ERROR_LOCAL_INSUFFICIENT_RESOURCES = 0x102,
};

//! @internal Macro to map Bluetopia errors to enum pbl_bt_errno
#define PBL_BT_ERRNO_WITH_INTERNAL_ERROR(e) ((int)PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN - e)

//! @internal Macro to map HCI errors to enum pbl_bt_errno
#define PBL_BT_ERRNO_WITH_HCI_ERROR(e) ((int)PBL_BT_ERRNO_HCI_ERROR_BEGIN + e)

//! Property bits of a characteristic
//! See the Bluetooth 4.0 Specification, Volume 3, Part G,
//! 3.3.1.1 "Characteristic Properties" for more details.
//! @see ble_characteristic_get_properties
enum pbl_bt_attribute_property {
  PBL_BT_ATTRIBUTE_PROPERTY_NONE = 0,
  PBL_BT_ATTRIBUTE_PROPERTY_BROADCAST = (1 << 0),
  PBL_BT_ATTRIBUTE_PROPERTY_READ = (1 << 1),
  PBL_BT_ATTRIBUTE_PROPERTY_WRITE_WITHOUT_RESPONSE = (1 << 2),
  PBL_BT_ATTRIBUTE_PROPERTY_WRITE = (1 << 3),
  PBL_BT_ATTRIBUTE_PROPERTY_NOTIFY = (1 << 4),
  PBL_BT_ATTRIBUTE_PROPERTY_INDICATE = (1 << 5),
  PBL_BT_ATTRIBUTE_PROPERTY_AUTHENTICATED_SIGNED_WRITES = (1 << 6),
  PBL_BT_ATTRIBUTE_PROPERTY_EXTENDED_PROPERTIES = (1 << 7),

  // Properties for Characteristics & Descriptors that
  // are hosted by the local server:
  PBL_BT_ATTRIBUTE_PROPERTY_READING_REQUIRES_ENCRYPTION = (1 << 8),
  PBL_BT_ATTRIBUTE_PROPERTY_WRITING_REQUIRES_ENCRYPTION = (1 << 9),
};

//! Opaque reference to a service object.
typedef uintptr_t pbl_bt_service_t;

//! Opaque reference to a characteristic object.
typedef uintptr_t pbl_bt_characteristic_t;

//! Opaque reference to a descriptor object.
typedef uintptr_t pbl_bt_descriptor_t;

_Static_assert(sizeof(pbl_bt_descriptor_t) == sizeof(uintptr_t),
               "pbl_bt_descriptor_t is invalid size");
_Static_assert(sizeof(pbl_bt_characteristic_t) == sizeof(uintptr_t),
               "pbl_bt_characteristic_t is invalid size");

#define PBL_BT_SERVICE_INVALID        ((pbl_bt_service_t)0)
#define PBL_BT_CHARACTERISTIC_INVALID ((pbl_bt_characteristic_t)0)
#define PBL_BT_DESCRIPTOR_INVALID     ((pbl_bt_descriptor_t)0)

//! Identifier for a device bonding.
//! They stay the same across reboots, so they can be persisted by apps.
typedef uint8_t pbl_bt_bonding_id_t;

#define PBL_BT_BONDING_ID_INVALID (0xFFU)

typedef uint16_t pbl_bt_cccd_id_t;

#define PBL_BT_CCCD_ID_INVALID (0xFFU)

struct PBL_PACKED pbl_bt_addr {
  uint8_t octets[6];
};

//! Size of a struct pbl_bt_addr
#define PBL_BT_ADDR_SIZE (sizeof(struct pbl_bt_addr))

//! Print format for printing struct pbl_bt_addr structs
//! @see PBL_BT_ADDR_XPLODE
#define PBL_BT_ADDR_FMT             "%02X:%02X:%02X:%02X:%02X:%02X"
#define PBL_BT_ADDR_FMT_BUFFER_SIZE (18)

#define PBL_BT_BD_ADDR_FMT             "0x%02X%02X%02X%02X%02X%02X"
#define PBL_BT_BD_ADDR_FMT_BUFFER_SIZE (15)

#define PBL_BT_DEVICE_NAME_BUFFER_SIZE (20)

//! Macro decompose a struct pbl_bt_addr struct into its parts, so it can be used
//! with the PBL_BT_ADDR_FMT format macro
#define PBL_BT_ADDR_XPLODE(a) \
  (a).octets[5], (a).octets[4], (a).octets[3], (a).octets[2], (a).octets[1], (a).octets[0]

#define PBL_BT_ADDR_XPLODE_PTR(a) \
  (a)->octets[5], (a)->octets[4], (a)->octets[3], (a)->octets[2], (a)->octets[1], (a)->octets[0]

//! Data structure that represents a remote Bluetooth device.
//! The fields of the structure are opaque. Its contents should not be changed
//! or relied upon by the application.
struct pbl_bt_device {
  union {
    uint32_t opaque[2];
    uint64_t opaque_64;
  };
};

//! @internal The internal layout of the opaque struct pbl_bt_device. This should not be
//! exported. It can also never be changed in size. It has to be exactly as
//! large as the struct pbl_bt_device struct.
struct PBL_PACKED pbl_bt_device_internal {
  union {
    struct PBL_PACKED {
      struct pbl_bt_addr address;
      bool is_classic : 1;
      bool is_random_address : 1;
      //! !!! WARNING: If you're adding more flags here, you need to update
      //! the bt_device_bits field in PebbleBLEGATTClientEvent and PebbleBLEConnectionEvent !!!
      uint16_t zero : 14;
    };
    struct pbl_bt_device opaque;
  };
};

#define PBL_BT_DEVICE_INVALID          ((const struct pbl_bt_device){})
#define PBL_BT_DEVICE_INTERNAL_INVALID ((const struct pbl_bt_device_internal){})

_Static_assert(sizeof(struct pbl_bt_device_internal) == sizeof(struct pbl_bt_device),
               "struct pbl_bt_device_internal should be equal in size to struct pbl_bt_device");

//! Opaque data structure representing an advertisement report and optional
//! scan response. Use the ble_ad... functions to query its contents.
struct pbl_bt_ad_data;

//! @internal
//! The maximum size in bytes of an advertising report.
#define PBL_BT_AD_REPORT_DATA_MAX_LENGTH (31)

//! Flags used in an LE Advertising packet. Listed in
//! Supplement to Bluetooth Core Specification | CSSv6, Part A, 1.3.1
#define PBL_BT_AD_FLAGS_LIM_DISCOVERABLE_MASK            (1 << 0)
#define PBL_BT_AD_FLAGS_GEN_DISCOVERABLE_MASK            (1 << 1)
#define PBL_BT_AD_FLAGS_BR_EDR_NOT_SUPPORTED_MASK        (1 << 2)
#define PBL_BT_AD_FLAGS_LE_BR_EDR_SIMULT_CONTROLLER_MASK (1 << 3)
#define PBL_BT_AD_FLAGS_LE_BR_EDR_SIMULT_HOST_MASK       (1 << 4)

#define PBL_BT_LL_CONN_INTV_MIN_SLOTS        (6)    // 1.25ms / slot
#define PBL_BT_LL_CONN_INTV_MAX_SLOTS        (3200) // 1.25ms / slot
#define PBL_BT_LL_SUPERVISION_TIMEOUT_MIN_MS (100)

//! Advertisement and scan response data
//! @internal Exported as forward struct
struct pbl_bt_ad_data {
  //! Lengths of the raw advertisement data
  uint8_t ad_data_length;

  //! Lengths of the raw scan response data
  uint8_t scan_resp_data_length;

  //! The raw advertisement data, concatenated with the raw scan response data.
  uint8_t data[0];
};

//! Macro that does the same as bt_uuid_expand_32bit / bt_uuid_expand_16bit, but at compile-time
#define PBL_BT_SIG_UUID_EXPAND(u)                                                              \
  (0xff & ((uint32_t)u) >> 24), (0xff & ((uint32_t)u) >> 16), (0xff & ((uint32_t)u) >> 8),     \
      (0xff & ((uint32_t)u) >> 0), 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, \
      0x34, 0xFB
