/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_sm_types Security Manager types
 * @ingroup bluetooth
 * @brief Keys and pairing information.
 * @{
 */

/** @brief Root keys of the local device, from which the local keys are derived. */
enum pbl_bt_sm_root_key_type {
  /** Encryption Root key (ER). */
  PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
  /** Identity Root key (IR), from which the local IRK is derived. */
  PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
  /** Number of root key types. */
  PBL_BT_SM_ROOT_KEY_TYPE_NUM,
};

/** @brief A 128-bit key. */
struct PBL_PACKED pbl_bt_sm_key {
  /** Key bytes. */
  uint8_t data[16];
};

/** @brief Encryption information used when the local device is the peripheral. */
struct PBL_PACKED pbl_bt_sm_local_encryption_info {
  /** Encrypted Diversifier. */
  uint16_t ediv;

  /** Diversifier. Only used by the legacy cc2564x/Bluetopia backend. */
  uint16_t div;

  /** Long Term Key. */
  struct pbl_bt_sm_key ltk;

  /** Random number. */
  uint64_t rand;
};

/** @brief Encryption information distributed by the remote device. */
struct PBL_PACKED pbl_bt_sm_remote_encryption_info {
  /** Long Term Key. */
  struct pbl_bt_sm_key ltk;
  /** Random number. */
  uint64_t rand;
  /** Encrypted Diversifier. */
  uint16_t ediv;
};

/**
 * @brief Keys and identity exchanged during pairing.
 *
 * Packed because it is serialized. Which fields are populated depends on the backend; the
 * @c is_*_valid flags tell.
 */
struct PBL_PACKED pbl_bt_sm_pairing_info {
  /** Encryption information used when the local device is the peripheral. */
  struct pbl_bt_sm_local_encryption_info local_encryption_info;

  /** Encryption information used when the local device is the central. */
  struct pbl_bt_sm_remote_encryption_info remote_encryption_info;

  /** Identity Resolving Key of the remote device. */
  struct pbl_bt_sm_key irk;
  /** Identity address of the remote device. */
  struct pbl_bt_device_internal identity;

  /** Connection Signature Resolving Key of the remote device. */
  struct pbl_bt_sm_key csrk;

  /** True if @ref local_encryption_info is valid. */
  bool is_local_encryption_info_valid;

  /** True if @ref remote_encryption_info is valid. */
  bool is_remote_encryption_info_valid;

  /** True if @ref irk and @ref identity are valid. */
  bool is_remote_identity_info_valid;

  /** True if @ref csrk is valid. */
  bool is_remote_signing_info_valid;

  /** True if the pairing is MITM protected. Not set by every backend. */
  bool is_mitm_protection_enabled;
};

/** @} */
