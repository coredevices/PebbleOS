/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_prf_store PRF store
 * @ingroup bluetooth
 * @brief The bonding and root keys the recovery firmware (PRF) and the normal firmware share.
 *
 * Kept in a flash partition of its own, as a rolling list of 256-byte entries (see
 * @ref bluetooth_prf_store_layout) that recovery firmware already in the field reads: the layout
 * cannot change. Each field carries its own CRC; changing a field invalidates the current entry
 * and rewrites the data into the next one, and the partition is erased once it fills up. All
 * functions are serialized by a mutex.
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
 */
void pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in);

/**
 * @brief Get the BLE pairing.
 *
 * Output parameters may be NULL and are only valid when true is returned.
 *
 * @param[out] pairing_info_out Pairing keys.
 * @param[out] name_out Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes for the remote device
 *                      name, empty if none is stored.
 * @param[out] flags Pairing flags.
 * @return true if a pairing is stored.
 */
bool pbl_bt_prf_store_get_pairing(struct pbl_bt_sm_pairing_info *pairing_info_out, char *name_out,
                                  uint8_t *flags);

/**
 * @brief Store the BLE pairing, replacing the previous one.
 *
 * Empty pairing info is ignored.
 *
 * @param pairing_info Pairing keys.
 * @param name Remote device name, or NULL to keep the stored name.
 * @param flags Pairing flags.
 */
void pbl_bt_prf_store_set_pairing(const struct pbl_bt_sm_pairing_info *pairing_info,
                                  const char *name, uint8_t flags);

/**
 * @brief Store the device name of the BLE pairing. Does nothing without a pairing.
 *
 * @param name Device name.
 */
void pbl_bt_prf_store_set_pairing_name(const char *name);

/** @brief Erase the BLE pairing and its device name. */
void pbl_bt_prf_store_erase_pairing(void);

/** @brief Erase everything, for a factory reset. */
void pbl_bt_prf_store_wipe(void);

/**
 * @brief Initialize the PRF store.
 *
 * Finds the valid entry, and erases the region, keeping that entry, when more than 75% of the
 * region is used so that later writes are unlikely to block on an erase.
 */
void pbl_bt_prf_store_init(void);

/** @} */
