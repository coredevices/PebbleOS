/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/sm_util.h>

/**
 * @defgroup services_bluetooth_bluetooth_persistent_storage_debug Bonding database debug
 * @ingroup services_bluetooth
 * @brief Shell dumps of the bonding database.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_shell;
/** @endcond */

/**
 * @brief Print BLE pairing info.
 *
 * @param sh Shell to print to.
 * @param info Pairing info.
 */
void bluetooth_persistent_storage_debug_dump_ble_pairing_info(
    const struct pbl_shell *sh, const struct pbl_bt_sm_pairing_info *info);

/**
 * @brief Print the root keys.
 *
 * @param sh Shell to print to.
 * @param irk Identity root key, may be NULL.
 * @param erk Encryption root key, may be NULL.
 */
void bluetooth_persistent_storage_debug_dump_root_keys(const struct pbl_shell *sh,
                                                       const struct pbl_bt_sm_key *irk,
                                                       const struct pbl_bt_sm_key *erk);

/**
 * @brief Print the whole bonding database.
 *
 * @param sh Shell to print to.
 */
void bluetooth_persistent_storage_dump_contents(const struct pbl_shell *sh);

/** @} */
