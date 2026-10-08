/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup services_shared_prf_storage_v2_sprf Legacy shared PRF storage layout
 * @ingroup services_shared_prf_storage
 * @brief Former single-struct layout of the shared PRF storage, not used by the firmware.
 * @{
 */

/**
 * @brief Layout version.
 *
 * - 1: Added BLE and BT Classic pairing data.
 * - 2: Added getting started is complete bit.
 * - 3: Added remote Rand, remote EDIV, local DIV, local EDIV, is_..._valid flags, local device
 *   name.
 */
#define SHARED_PRF_STORAGE_VERSION 3

/** @brief BLE pairing. */
typedef struct PBL_PACKED {
  /** Remote device name. */
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  /** EDIV handed to the remote with our LTK, used when the watch is peripheral. */
  uint16_t local_ediv;
  /** DIV handed to the remote with our LTK, used when the watch is peripheral. */
  uint16_t local_div;

  /** Remote LTK, used when the watch is central. */
  struct pbl_bt_sm_key ltk;
  /** Remote Rand, used when the watch is central. */
  uint64_t rand;
  /** Remote EDIV, used when the watch is central. */
  uint16_t ediv;

  /** Remote IRK, used when the watch is peripheral. */
  struct pbl_bt_sm_key irk;
  /** Remote identity address, used when the watch is peripheral. */
  struct pbl_bt_device_internal identity;

  /** Remote signature key. */
  struct pbl_bt_sm_key csrk;

  /** @ref local_div and @ref local_ediv are valid. */
  bool is_local_encryption_info_valid : 1;

  /** @ref ltk, @ref rand and @ref ediv are valid. */
  bool is_remote_encryption_info_valid : 1;

  /** @ref irk and @ref identity are valid. */
  bool is_remote_identity_info_valid : 1;

  /** @ref csrk is valid. Since iOS 9, the CSRK is no longer exchanged. */
  bool is_remote_signing_info_valid : 1;
} BLEPairingData;

/** @brief BT Classic pairing. */
typedef struct PBL_PACKED {
  /** Remote address. */
  struct pbl_bt_addr address;
  /** Link key. */
  struct pbl_bt_sm_key link_key;
  /** Remote device name. */
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  /** Remote platform bits. */
  uint8_t platform_bits;
} BTClassicPairingData;

/** @brief Legacy shared PRF storage contents. */
typedef struct PBL_PACKED {
  /** Layout version, see @ref SHARED_PRF_STORAGE_VERSION. */
  uint32_t version;

  /** Custom local device name, empty to use the default name. */
  char local_device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  /** BLE root keys (ER and IR). */
  struct pbl_bt_sm_key root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];

  /** BLE pairing, must be adjacent to @ref bt_classic_data. */
  BLEPairingData ble_data;
  /** BT Classic pairing. */
  BTClassicPairingData bt_classic_data;

  /** Onboarding has been completed. */
  bool getting_started_is_complete;
} SharedPRFData;

/** @} */
