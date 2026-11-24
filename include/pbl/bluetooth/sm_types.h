/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_sm_types Security Manager types
 * @ingroup bluetooth
 * @brief Keys.
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

/** @} */
