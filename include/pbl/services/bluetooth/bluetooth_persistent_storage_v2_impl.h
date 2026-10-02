/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/kernel/compiler.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_bluetooth_bluetooth_persistent_storage_v2_impl Bonding storage format
 * @ingroup services_bluetooth
 * @brief On-flash layout of BLE pairing info in the bonding database.
 *
 * The layout is persisted: fields may only be added, never changed or removed.
 * @{
 */

/** @brief Stored LE encryption info. */
typedef struct PBL_PACKED BtPersistLEEncryptionInfo {
  /** Long term key. */
  struct pbl_bt_sm_key ltk;
  /** Encrypted diversifier. */
  uint16_t ediv;
  /** Random number. */
  uint64_t rand;
} BtPersistLEEncryptionInfo;

/** @brief Stored BLE pairing info. */
typedef struct PBL_PACKED BtPersistLEPairingInfo {
  /** Encryption info distributed by the watch. */
  BtPersistLEEncryptionInfo local_encryption_info;

  /** Encryption info distributed by the remote device. */
  BtPersistLEEncryptionInfo remote_encryption_info;

  /** Remote identity resolving key. */
  struct pbl_bt_sm_key irk;
  /** Remote identity address. */
  struct pbl_bt_device_internal identity;

  /** Remote connection signature resolving key. */
  struct pbl_bt_sm_key csrk;

  /** True if @ref local_encryption_info is valid. */
  bool is_local_encryption_info_valid : 1;

  /** True if @ref remote_encryption_info is valid. */
  bool is_remote_encryption_info_valid : 1;

  /** True if @ref irk and @ref identity are valid. */
  bool is_remote_identity_info_valid : 1;

  /** True if @ref csrk is valid. */
  bool is_remote_signing_info_valid : 1;

  /** True if man-in-the-middle protection was used during pairing. */
  bool is_mitm_protection_enabled : 1;

  /** Reserved. */
  uint8_t rsvd : 3;
} BtPersistLEPairingInfo;

/**
 * @brief Convert pairing info to its stored form.
 *
 * @param[out] out Stored pairing info.
 * @param in Pairing info.
 */
static void bt_persistent_storage_assign_persist_pairing_info(
    BtPersistLEPairingInfo *out, const struct pbl_bt_sm_pairing_info *in) {
  *out = (BtPersistLEPairingInfo){
    .local_encryption_info =
        {
          .ltk = in->local_encryption_info.ltk,
          .rand = in->local_encryption_info.rand,
          .ediv = in->local_encryption_info.ediv,
        },
    .remote_encryption_info =
        {
          .ltk = in->remote_encryption_info.ltk,
          .rand = in->remote_encryption_info.rand,
          .ediv = in->remote_encryption_info.ediv,
        },
    .irk = in->irk,
    .identity = in->identity,
    .csrk = in->csrk,
    .is_local_encryption_info_valid = in->is_local_encryption_info_valid,
    .is_remote_encryption_info_valid = in->is_remote_encryption_info_valid,
    .is_remote_identity_info_valid = in->is_remote_identity_info_valid,
    .is_remote_signing_info_valid = in->is_remote_signing_info_valid,
    .is_mitm_protection_enabled = in->is_mitm_protection_enabled,
  };
}

/**
 * @brief Convert stored pairing info back to pairing info.
 *
 * @param[out] out Pairing info.
 * @param in Stored pairing info.
 */
static void bt_persistent_storage_assign_sm_pairing_info(struct pbl_bt_sm_pairing_info *out,
                                                         const BtPersistLEPairingInfo *in) {
  *out = (struct pbl_bt_sm_pairing_info){
    .local_encryption_info =
        {
          .ltk = in->local_encryption_info.ltk,
          .rand = in->local_encryption_info.rand,
          .ediv = in->local_encryption_info.ediv,
        },
    .remote_encryption_info =
        {
          .ltk = in->remote_encryption_info.ltk,
          .rand = in->remote_encryption_info.rand,
          .ediv = in->remote_encryption_info.ediv,
        },
    .irk = in->irk,
    .identity = in->identity,
    .csrk = in->csrk,
    .is_local_encryption_info_valid = in->is_local_encryption_info_valid,
    .is_remote_encryption_info_valid = in->is_remote_encryption_info_valid,
    .is_remote_identity_info_valid = in->is_remote_identity_info_valid,
    .is_remote_signing_info_valid = in->is_remote_signing_info_valid,
    .is_mitm_protection_enabled = in->is_mitm_protection_enabled,
  };
}

/** @} */
