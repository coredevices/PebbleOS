/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/sm_types.h>

//! Used to version the struct if we have to add additional fields in the future.
//! 1: Added BLE and BT Classic pairing data
//! 2: Added getting started is complete bit
//! 3: Added remote Rand, remote EDIV, local DIV, local EDIV, is_..._valid flags, local device name
#define SHARED_PRF_STORAGE_VERSION 3

typedef struct PACKED {
  // Remote device name
  char name[BT_DEVICE_NAME_BUFFER_SIZE];

  // DIV / EDIV that was handed to the remote with our LTK (used when Pebble is Slave):
  uint16_t local_ediv;
  uint16_t local_div;

  // Remote encryption info (used when Pebble is Master):
  SMLongTermKey ltk;
  uint64_t rand;
  uint16_t ediv;

  // Remote identity info (used when Pebble is Slave):
  SMIdentityResolvingKey irk;
  BTDeviceInternal identity;

  // Remote signature key:
  SM128BitKey csrk;

  //! True if local_div and local_ediv are valid
  bool is_local_encryption_info_valid:1;

  //! True if ltk, rand and ediv are valid
  bool is_remote_encryption_info_valid:1;

  //! True if irk and identity are valid
  bool is_remote_identity_info_valid:1;

  //! True if csrk is valid
  //! @note Since iOS 9, CSRK is no longer exchanged.
  bool is_remote_signing_info_valid:1;
} BLEPairingData;

typedef struct PACKED {
  BTDeviceAddress address;
  SM128BitKey link_key;
  char name[BT_DEVICE_NAME_BUFFER_SIZE];
  uint8_t platform_bits;
} BTClassicPairingData;

typedef struct PACKED {
  uint32_t version;

  // Customized local device name, or zero-length string if the default device name should be used
  char local_device_name[BT_DEVICE_NAME_BUFFER_SIZE];

  SM128BitKey root_keys[SMRootKeyTypeNum];  // ER and IR key

  // We rely on these two pieces of data being adjacent to each other
  BLEPairingData ble_data;
  BTClassicPairingData bt_classic_data;

  bool getting_started_is_complete;
} SharedPRFData;
