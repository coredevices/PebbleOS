/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

/**
 * @defgroup services_shared_prf_storage Shared PRF storage
 * @ingroup services
 * @brief Bluetooth pairing and settings shared between the recovery (PRF) and normal firmware.
 *
 * Data is kept in a dedicated flash region as a rolling list of 256-byte entries (see
 * @ref services_shared_prf_storage_v3_sprf). Each field carries its own CRC; changing a field
 * invalidates the current entry and rewrites the data into the next one, and the region is erased
 * once it fills up. All functions are serialized by a mutex.
 * @{
 */

/**
 * @brief Get the custom local device name.
 *
 * @param[out] local_device_name_out Buffer for the name, may be NULL. Set to an empty string when
 *                                   no name is stored.
 * @param max_size Size of @p local_device_name_out in bytes.
 * @return true if a non-empty name is stored.
 */
bool shared_prf_storage_get_local_device_name(char *local_device_name_out, size_t max_size);

/**
 * @brief Store the custom local device name.
 *
 * @param local_device_name Name to store, or NULL to erase it.
 */
void shared_prf_storage_set_local_device_name(const char *local_device_name);

/**
 * @brief Get a BLE root key.
 *
 * @param key_type Encryption Root (ER) or Identity Root (IR) key.
 * @param[out] key_out Key, may be NULL.
 * @return true if a non-zero key is stored.
 */
bool shared_prf_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                     struct pbl_bt_sm_key *key_out);

/**
 * @brief Store the BLE root keys.
 *
 * @param keys_in Array of @c PBL_BT_SM_ROOT_KEY_TYPE_NUM keys indexed by
 *                @ref pbl_bt_sm_root_key_type, or NULL to store zeroed keys.
 */
void shared_prf_storage_set_root_keys(struct pbl_bt_sm_key *keys_in);

/**
 * @brief Get the BLE pairing.
 *
 * Output parameters may be NULL and are only valid when true is returned.
 *
 * @param[out] pairing_info_out Pairing keys.
 * @param[out] name_out Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes for the remote device
 *                      name, empty if none is stored.
 * @param[out] requires_address_pinning_out Whether the pairing requires address pinning.
 * @param[out] flags Pairing flags.
 * @return true if a pairing is stored.
 */
bool shared_prf_storage_get_ble_pairing_data(struct pbl_bt_sm_pairing_info *pairing_info_out,
                                             char *name_out, bool *requires_address_pinning_out,
                                             uint8_t *flags);

/**
 * @brief Store the BLE pairing, replacing the previous one.
 *
 * Empty pairing info is ignored.
 *
 * @param pairing_info Pairing keys.
 * @param name Remote device name, or NULL to keep the stored name.
 * @param requires_address_pinning Whether the pairing requires address pinning.
 * @param flags Pairing flags.
 */
void shared_prf_storage_store_ble_pairing_data(const struct pbl_bt_sm_pairing_info *pairing_info,
                                               const char *name, bool requires_address_pinning,
                                               uint8_t flags);

/** @brief Erase the BLE pairing and its device name. */
void shared_prf_storage_erase_ble_pairing_data(void);

/**
 * @brief Get the pinned BLE address.
 *
 * @param[out] address_out Address, may be NULL. Only valid when true is returned.
 * @return true if a pinned address is stored.
 */
bool shared_prf_storage_get_ble_pinned_address(struct pbl_bt_addr *address_out);

/**
 * @brief Store the pinned BLE address.
 *
 * @param address Address, or NULL to erase it.
 */
void shared_prf_storage_set_ble_pinned_address(const struct pbl_bt_addr *address);

/**
 * @brief Get the local identity address.
 *
 * @param[out] address_out Address, may be NULL. Only valid when true is returned.
 * @return true if a local identity address is stored.
 */
bool shared_prf_storage_get_local_identity_address(struct pbl_bt_addr *address_out);

/**
 * @brief Store the local identity address.
 *
 * @param address Address, or NULL to erase it.
 */
void shared_prf_storage_set_local_identity_address(const struct pbl_bt_addr *address);

/**
 * @brief Get a BT Classic pairing.
 *
 * BT Classic is not supported: asserts if called.
 *
 * @param[out] addr_out Remote address.
 * @param[out] device_name_out Remote device name.
 * @param[out] link_key_out Link key.
 * @param[out] platform_bits Remote platform bits.
 * @return Does not return.
 */
bool shared_prf_storage_get_bt_classic_pairing_data(struct pbl_bt_addr *addr_out,
                                                    char *device_name_out,
                                                    struct pbl_bt_sm_key *link_key_out,
                                                    uint8_t *platform_bits);

/**
 * @brief Store a BT Classic pairing.
 *
 * BT Classic is not supported: asserts if called.
 *
 * @param addr Remote address.
 * @param device_name Remote device name.
 * @param link_key Link key.
 * @param platform_bits Remote platform bits.
 */
void shared_prf_storage_store_bt_classic_pairing_data(struct pbl_bt_addr *addr,
                                                      const char *device_name,
                                                      struct pbl_bt_sm_key *link_key,
                                                      uint8_t platform_bits);

/**
 * @brief Store the BT Classic remote platform bits.
 *
 * BT Classic is not supported: asserts if called.
 *
 * @param platform_bits Remote platform bits.
 */
void shared_prf_storage_store_platform_bits(uint8_t platform_bits);

/**
 * @brief Erase the BT Classic pairing.
 *
 * BT Classic is not supported: asserts if called.
 */
void shared_prf_storage_erase_bt_classic_pairing_data(void);

/**
 * @brief Check whether onboarding (getting started) has been completed.
 *
 * @return true if completed.
 */
bool shared_prf_storage_get_getting_started_complete(void);

/**
 * @brief Set whether onboarding (getting started) has been completed.
 *
 * @param set true if completed.
 */
void shared_prf_storage_set_getting_started_complete(bool set);

/** @brief Erase all shared data, for a factory reset. */
void shared_prf_storage_wipe_all(void);

/**
 * @brief Initialize shared PRF storage.
 *
 * Finds the valid entry, and erases the region, keeping that entry, when more than 75% of the
 * region is used so that later writes are unlikely to block on an erase.
 */
void shared_prf_storage_init(void);

/** @} */
