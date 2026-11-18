/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

/**
 * @defgroup services_bluetooth_bonding Bonding
 * @ingroup services_bluetooth
 * @brief What the firmware does with the bonds the Bluetooth stack keeps.
 *
 * Only one bond is kept, the phone's: bonding with a new peer deletes the others. It is copied
 * to the PRF store, which the recovery firmware uses, and taken from there when the recovery
 * firmware made it. The PRF store also has the phone's name and the root keys.
 * @{
 */

/**
 * @brief Bring the stack's bonds and the PRF store in line, before the stack starts.
 *
 * The pairing in the PRF store replaces the stack's bonds. Without one, the stack's bond is
 * copied there.
 */
void bt_bonding_init(void);

/**
 * @brief Get the root keys, generating them the first time.
 *
 * @param[out] keys Array of @c PBL_BT_SM_ROOT_KEY_TYPE_NUM keys, indexed by key type.
 */
void bt_bonding_get_root_keys(struct pbl_bt_sm_key *keys);

/**
 * @brief Get the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param[out] name Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes.
 * @return True if a name is stored for the peer.
 */
bool bt_bonding_get_peer_name(const struct pbl_bt_device_internal *addr, char *name);

/**
 * @brief Store the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param name The name.
 */
void bt_bonding_set_peer_name(const struct pbl_bt_device_internal *addr, const char *name);

/** @} */
