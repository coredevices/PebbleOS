/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

//! Used to version the struct if we have to add additional fields in the future.
//! 1: Added BLE and BT Classic pairing data
//! 2: Added getting started is complete bit
//! 3: Added remote Rand, remote EDIV, local DIV, local EDIV, is_..._valid flags, local device name
#define SHARED_PRF_STORAGE_VERSION 3

typedef struct PBL_PACKED {
  // Remote device name
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  // DIV / EDIV that was handed to the remote with our LTK (used when Pebble is Slave):
  uint16_t local_ediv;
  uint16_t local_div;

  // Remote encryption info (used when Pebble is Master):
  struct pbl_bt_sm_key ltk;
  uint64_t rand;
  uint16_t ediv;

  // Remote identity info (used when Pebble is Slave):
  struct pbl_bt_sm_key irk;
  struct pbl_bt_device_internal identity;

  // Remote signature key:
  struct pbl_bt_sm_key csrk;

  //! True if local_div and local_ediv are valid
  bool is_local_encryption_info_valid : 1;

  //! True if ltk, rand and ediv are valid
  bool is_remote_encryption_info_valid : 1;

  //! True if irk and identity are valid
  bool is_remote_identity_info_valid : 1;

  //! True if csrk is valid
  //! @note Since iOS 9, CSRK is no longer exchanged.
  bool is_remote_signing_info_valid : 1;
} BLEPairingData;

typedef struct PBL_PACKED {
  struct pbl_bt_addr address;
  struct pbl_bt_sm_key link_key;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  uint8_t platform_bits;
} BTClassicPairingData;

typedef struct PBL_PACKED {
  uint32_t version;

  // Customized local device name, or zero-length string if the default device name should be used
  char local_device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  struct pbl_bt_sm_key root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM]; // ER and IR key

  // We rely on these two pieces of data being adjacent to each other
  BLEPairingData ble_data;
  BTClassicPairingData bt_classic_data;

  bool getting_started_is_complete;
} SharedPRFData;
