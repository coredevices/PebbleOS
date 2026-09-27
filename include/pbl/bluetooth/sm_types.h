/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

enum pbl_bt_sm_root_key_type {
  PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
  PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
  PBL_BT_SM_ROOT_KEY_TYPE_NUM,
};

struct PBL_PACKED pbl_bt_sm_key {
  uint8_t data[16];
};

struct PBL_PACKED pbl_bt_sm_local_encryption_info {
  uint16_t ediv;

  //! @note Only used by cc2564x/Bluetopia driver!
  uint16_t div;

  //! @note Only used by Dialog driver!
  struct pbl_bt_sm_key ltk;

  //! @note Only used by Dialog driver!
  uint64_t rand;
};

struct PBL_PACKED pbl_bt_sm_remote_encryption_info {
  struct pbl_bt_sm_key ltk;
  uint64_t rand;
  uint16_t ediv;
};

//! @note Some fields might not get populated/used, this depends on the BT Driver implementation.
//! @note Packed, because this is used in HC protocol messages.
struct PBL_PACKED pbl_bt_sm_pairing_info {
  //! The encryption info that will be used when the local device is the slave.
  struct pbl_bt_sm_local_encryption_info local_encryption_info;

  //! The encryption info that will be used when the local device is the master.
  struct pbl_bt_sm_remote_encryption_info remote_encryption_info;

  struct pbl_bt_sm_key irk;
  struct pbl_bt_device_internal identity;

  struct pbl_bt_sm_key csrk;

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
};
