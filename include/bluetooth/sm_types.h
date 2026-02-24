/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <bluetooth/bluetooth_types.h>
#include <util/attributes.h>

typedef enum {
  SMRootKeyTypeEncryption,
  SMRootKeyTypeIdentity,
  SMRootKeyTypeNum,
} SMRootKeyType;

typedef struct PACKED SM128BitKey {
  uint8_t data[16];
} SM128BitKey;

typedef SM128BitKey SMLongTermKey;
typedef SM128BitKey SMIdentityResolvingKey;
typedef SM128BitKey SMConnectionSignatureResolvingKey;

typedef struct PACKED SMLocalEncryptionInfo {
  uint16_t ediv;

  //! @note Only used by cc2564x/Bluetopia driver!
  uint16_t div;

  //! @note Only used by Dialog driver!
  SMLongTermKey ltk;

  //! @note Only used by Dialog driver!
  uint64_t rand;
} SMLocalEncryptionInfo;

typedef struct PACKED SMRemoteEncryptionInfo {
  SMLongTermKey ltk;
  uint64_t rand;
  uint16_t ediv;
} SMRemoteEncryptionInfo;

//! @note Some fields might not get populated/used, this depends on the BT Driver implementation.
//! @note Packed, because this is used in HC protocol messages.
typedef struct PACKED SMPairingInfo {
  //! The encryption info that will be used when the local device is the slave.
  SMLocalEncryptionInfo local_encryption_info;

  //! The encryption info that will be used when the local device is the master.
  SMRemoteEncryptionInfo remote_encryption_info;

  SMIdentityResolvingKey irk;
  BTDeviceInternal identity;

  SMConnectionSignatureResolvingKey csrk;

  //! True if div and ediv are valid
  bool is_local_encryption_info_valid;

  //! True if remote_encryption_info is valid
  bool is_remote_encryption_info_valid;

  //! True if irk and identity are valid
  bool is_remote_identity_info_valid;

  //! True if csrk is valid
  bool is_remote_signing_info_valid;

  //! @note NOT valid for cc2564x BT lib, only for Dialog BT lib!
  bool is_mitm_protection_enabled;
} SMPairingInfo;
