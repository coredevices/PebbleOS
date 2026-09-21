/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/kernel/compiler.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct PBL_PACKED BtPersistLEEncryptionInfo {
  SMLongTermKey ltk;
  uint16_t ediv;
  uint64_t rand;
} BtPersistLEEncryptionInfo;

typedef struct PBL_PACKED BtPersistLEPairingInfo {
  BtPersistLEEncryptionInfo local_encryption_info;

  BtPersistLEEncryptionInfo remote_encryption_info;

  SMIdentityResolvingKey irk;
  BTDeviceInternal identity;

  SMConnectionSignatureResolvingKey csrk;

  //! True if local_encryption_info is valid
  bool is_local_encryption_info_valid : 1;

  //! True if remote_encryption_info is valid
  bool is_remote_encryption_info_valid : 1;

  //! True if irk and identity are valid
  bool is_remote_identity_info_valid : 1;

  //! True if csrk is valid
  bool is_remote_signing_info_valid : 1;

  //! True if Man-in-the-middle protection was enabled during the pairing process.
  bool is_mitm_protection_enabled : 1;

  uint8_t rsvd : 3;
} BtPersistLEPairingInfo;

static void bt_persistent_storage_assign_persist_pairing_info(BtPersistLEPairingInfo *out,
                                                              const SMPairingInfo *in) {
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

static void bt_persistent_storage_assign_sm_pairing_info(SMPairingInfo *out,
                                                         const BtPersistLEPairingInfo *in) {
  *out = (SMPairingInfo){
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
