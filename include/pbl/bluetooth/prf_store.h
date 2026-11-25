/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

struct ble_store_value_sec;

/**
 * @defgroup bluetooth_prf_store PRF store
 * @ingroup bluetooth
 * @brief The bonding and root keys the recovery firmware (PRF) and the normal firmware share.
 *
 * Kept in a flash partition of its own. How the partition is laid out is a build choice
 * (@c NIMBLE_STORE_PRF_LAYOUT) and private to the store, whose layout headers describe it. Both
 * firmwares of a device have to agree on it, and it cannot change once a device is in the field:
 * the recovery firmware there is not updated. All functions are serialized by a mutex. Those that
 * write return 0, or a negative errno if the flash failed.
 *
 * Implemented by the NimBLE store (@c NIMBLE_STORE_PRF), which can also persist in it
 * (@c NIMBLE_STORE_BACKEND_PRF).
 * @{
 */

/**
 * @brief Get a BLE root key.
 *
 * @param key_type Encryption Root (ER) or Identity Root (IR) key.
 * @param[out] key_out Key, may be NULL.
 * @return true if a non-zero key is stored.
 */
bool pbl_bt_prf_store_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                   struct pbl_bt_sm_key *key_out);

/**
 * @brief Store the BLE root keys.
 *
 * @param keys_in Array of @c PBL_BT_SM_ROOT_KEY_TYPE_NUM keys indexed by
 *                @ref pbl_bt_sm_root_key_type, or NULL to store zeroed keys.
 * @return 0 on success, negative errno otherwise.
 */
int pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in);

/**
 * @brief Get the BLE pairing, as the keys NimBLE stores for a peer.
 *
 * Output parameters may be NULL and are only valid when true is returned.
 *
 * @param[out] peer The peer's keys: its address, LTK and IRK, whichever are stored.
 * @param[out] our The keys of the watch for that peer: its LTK, if stored.
 * @param[out] name Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes for the remote device name,
 *                  empty if none is stored.
 * @return true if a pairing is stored.
 */
bool pbl_bt_prf_store_get_pairing(struct ble_store_value_sec *peer, struct ble_store_value_sec *our,
                                  char *name);

/**
 * @brief Store the BLE pairing, replacing the previous one.
 *
 * A pairing with no key is refused.
 *
 * @param peer The peer's keys.
 * @param our The keys of the watch for that peer, or NULL if it has none.
 * @param name Remote device name, or NULL to keep the stored name if it is of the same peer.
 * @retval 0 on success.
 * @retval -EINVAL if the pairing has no key.
 * @return Another negative errno if the flash failed.
 */
int pbl_bt_prf_store_set_pairing(const struct ble_store_value_sec *peer,
                                 const struct ble_store_value_sec *our, const char *name);

/**
 * @brief Store the device name of the BLE pairing. Does nothing without a pairing.
 *
 * @param name Device name.
 * @return 0 on success, negative errno otherwise.
 */
int pbl_bt_prf_store_set_pairing_name(const char *name);

/**
 * @brief Erase the BLE pairing and its device name.
 *
 * @return 0 on success, negative errno otherwise.
 */
int pbl_bt_prf_store_erase_pairing(void);

/**
 * @brief Erase everything, for a factory reset.
 *
 * @return 0 on success, negative errno otherwise.
 */
int pbl_bt_prf_store_wipe(void);

/**
 * @brief Initialize the PRF store.
 *
 * Finds what is stored, and erases the partition, keeping it, when more than 75% of the partition
 * is used so that later writes are unlikely to block on an erase.
 */
void pbl_bt_prf_store_init(void);

/** @} */
