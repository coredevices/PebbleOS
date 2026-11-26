/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

#include <nimble/ble.h>

/**
 * @defgroup services_bluetooth_bonding Bonding
 * @ingroup services_bluetooth
 * @brief What the firmware does with the bonds the Bluetooth stack keeps.
 *
 * Only one bond is kept, the phone's: bonding with a new peer deletes the others. The stack's
 * store shares it with the PRF store, which the recovery firmware uses. The PRF store also has
 * the phone's name and the root keys.
 * @{
 */

/**
 * @brief Leave the stack one bond, before it starts: the one the PRF store has, if any.
 */
void bt_bonding_init(void);

/**
 * @brief Get the root keys, generating them the first time.
 *
 * @param[out] keys Array of @c PBL_BT_SM_ROOT_KEY_TYPE_NUM keys, indexed by key type.
 * @retval 0 on success.
 * @retval -EIO if they could not be generated.
 */
int bt_bonding_get_root_keys(struct pbl_bt_sm_key *keys);

/**
 * @brief Get the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param[out] name Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes.
 * @return True if a name is stored for the peer.
 */
bool bt_bonding_get_peer_name(const ble_addr_t *addr, char *name);

/**
 * @brief Store the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param name The name.
 */
void bt_bonding_set_peer_name(const ble_addr_t *addr, const char *name);

/** @} */
