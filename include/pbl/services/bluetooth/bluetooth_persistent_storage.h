/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/comm_session/session_remote_version.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/bonding_sync.h>
#include <pbl/bluetooth/sm_types.h>

/**
 * @defgroup services_bluetooth_bluetooth_persistent_storage Bonding database
 * @ingroup services_bluetooth
 * @brief Persistent BLE bondings, CCCDs and local Bluetooth settings.
 *
 * Normal firmware keeps them in a settings file and mirrors the gateway pairing, root keys,
 * device name and pinned address to shared PRF storage. Recovery firmware uses shared PRF
 * storage directly, with a single bonding slot. Only one BLE bonding is kept: storing a new
 * one replaces the others.
 *
 * Bonding changes are announced on KernelMain to the address, connection and client modules.
 * @{
 */

/** @brief Change made to a bonding. */
typedef enum {
  /** No change. */
  BtPersistBondingOpInvalid = -1,
  /** Bonding was added. */
  BtPersistBondingOpDidAdd,
  /** Existing bonding was updated. */
  BtPersistBondingOpDidChange,
  /** Bonding is about to be deleted. */
  BtPersistBondingOpWillDelete,
} BtPersistBondingOp;

/** @brief Transport of a bonding. */
typedef enum {
  /** Bluetooth Classic (legacy, no longer stored). */
  BtPersistBondingTypeBTClassic,
  /** Bluetooth Low Energy. */
  BtPersistBondingTypeBLE,
  /** Number of bonding types. */
  BtPersistBondingNumTypes
} BtPersistBondingType;

/**
 * @brief Handler of bonding database changes.
 *
 * @param affected_bonding Bonding that changed.
 * @param operation Change made.
 */
typedef void (*BtPersistBondingChangeHandler)(pbl_bt_bonding_id_t affected_bonding,
                                              BtPersistBondingOp operation);

/**
 * @brief Callback for bt_persistent_storage_for_each_ble_pairing().
 *
 * @param device Remote identity address.
 * @param irk Remote identity resolving key.
 * @param name Remote device name.
 * @param id Bonding ID.
 * @param context User context.
 */
typedef void (*BtPersistBondingDBEachBLE)(struct pbl_bt_device_internal *device,
                                          struct pbl_bt_sm_key *irk, const char *name,
                                          pbl_bt_bonding_id_t *id, void *context);

/**
 * @brief Check whether any BLE bonding requires address pinning.
 *
 * @return True if one does.
 */
bool bt_persistent_storage_has_pinned_ble_pairings(void);

/**
 * @brief Store the pinned local BLE address.
 *
 * @param address Address to pin.
 * @return True on success.
 */
bool bt_persistent_storage_set_ble_pinned_address(const struct pbl_bt_addr *address);

/**
 * @brief Get the pinned local BLE address.
 *
 * @param[out] address_out Pinned address, may be NULL.
 * @return True if a pinned address is stored.
 */
bool bt_persistent_storage_get_ble_pinned_address(struct pbl_bt_addr *address_out);

/**
 * @brief Add or update a BLE bonding.
 *
 * An existing bonding with the same identity is updated; otherwise a new one is added, the
 * watch is marked unfaithful and any other BLE bonding is deleted.
 *
 * @param pairing_info Pairing info; empty pairing info is rejected.
 * @param is_gateway True if the device is the phone running the Pebble app.
 * @param device_name Remote device name, may be NULL.
 * @param requires_address_pinning True if the remote needs a stable local address.
 * @param flags Bonding flags, passed back to the Bluetooth driver.
 * @return Bonding ID, or @c PBL_BT_BONDING_ID_INVALID on failure.
 */
pbl_bt_bonding_id_t bt_persistent_storage_store_ble_pairing(
    const struct pbl_bt_sm_pairing_info *pairing_info, bool is_gateway, const char *device_name,
    bool requires_address_pinning, uint8_t flags);

/**
 * @brief Update the name of a BLE bonding.
 *
 * @param bonding Bonding ID.
 * @param device_name New name.
 * @return True on success.
 */
bool bt_persistent_storage_update_ble_device_name(pbl_bt_bonding_id_t bonding,
                                                  const char *device_name);

/**
 * @brief Delete the BLE bonding with the given ID, with its CCCDs.
 */
void bt_persistent_storage_delete_ble_pairing_by_id(pbl_bt_bonding_id_t);

/**
 * @brief Delete the BLE bonding with an identity address, with its CCCDs.
 *
 * @param device Remote identity address.
 */
void bt_persistent_storage_delete_ble_pairing_by_addr(const struct pbl_bt_device_internal *device);

/**
 * @brief Look up a BLE bonding by ID.
 *
 * @param bonding Bonding ID.
 * @param[out] IRK_out Remote identity resolving key, may be NULL.
 * @param[out] device_out Remote identity address, may be NULL.
 * @param[out] name_out Remote name, @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes, may be NULL.
 * @return True if found.
 */
bool bt_persistent_storage_get_ble_pairing_by_id(pbl_bt_bonding_id_t bonding,
                                                 struct pbl_bt_sm_key *IRK_out,
                                                 struct pbl_bt_device_internal *device_out,
                                                 char *name_out);

/**
 * @brief Look up a BLE bonding by identity address.
 *
 * @param device Remote identity address.
 * @param[out] IRK_out Remote identity resolving key, may be NULL.
 * @param[out] name_out Remote name, may be NULL.
 * @return True if found.
 */
bool bt_persistent_storage_get_ble_pairing_by_addr(const struct pbl_bt_device_internal *device,
                                                   struct pbl_bt_sm_key *IRK_out,
                                                   char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

/**
 * @brief Get the first BLE bonding that supports ANCS.
 *
 * @return Bonding ID, or @c PBL_BT_BONDING_ID_INVALID if none.
 */
pbl_bt_bonding_id_t bt_persistent_storage_get_ble_ancs_bonding(void);

/**
 * @brief Check whether a bonding is a BLE bonding that supports ANCS.
 *
 * @param bonding Bonding ID.
 * @return True if it is.
 */
bool bt_persistent_storage_is_ble_ancs_bonding(pbl_bt_bonding_id_t bonding);

/**
 * @brief Check whether any BLE bonding supports ANCS.
 *
 * @return True if one does.
 */
bool bt_persistent_storage_has_ble_ancs_bonding(void);

/**
 * @brief Check whether the active gateway is a BLE bonding.
 *
 * @return True if it is.
 */
bool bt_persistent_storage_has_active_ble_gateway_bonding(void);

/**
 * @brief Call a function for each BLE bonding.
 *
 * @p cb must not take bt_lock(), or it may deadlock.
 *
 * @param cb Callback.
 * @param context User context passed to @p cb.
 */
void bt_persistent_storage_for_each_ble_pairing(BtPersistBondingDBEachBLE cb, void *context);

/**
 * @brief Register all stored BLE bondings and CCCDs with the Bluetooth driver.
 */
void bt_persistent_storage_register_existing_ble_bondings(void);

/**
 * @brief Store a CCCD, replacing the one for the same peer and characteristic.
 *
 * @param cccd CCCD to store.
 * @return CCCD ID, or @c PBL_BT_CCCD_ID_INVALID on failure.
 */
pbl_bt_cccd_id_t bt_persistent_storage_store_cccd(const struct pbl_bt_cccd *cccd);

/**
 * @brief Delete a stored CCCD.
 *
 * @param peer Peer identity address.
 * @param chr_val_handle Characteristic value handle.
 * @return True if found and deleted.
 */
bool bt_persistent_storage_delete_cccd(const struct pbl_bt_device_internal *peer,
                                       uint16_t chr_val_handle);

/**
 * @brief Set the active gateway, the bonding that implements Pebble Protocol.
 *
 * On change, marks the watch unfaithful and clears the cached system capabilities.
 *
 * @param bonding Bonding ID.
 */
void bt_persistent_storage_set_active_gateway(pbl_bt_bonding_id_t bonding);

/**
 * @brief Get the active gateway.
 *
 * @param[out] bonding_out Bonding ID, may be NULL.
 * @param[out] type_out Bonding type, may be NULL.
 * @return True if an active gateway exists; outputs are only written then.
 */
bool bt_persistent_storage_get_active_gateway(pbl_bt_bonding_id_t *bonding_out,
                                              BtPersistBondingType *type_out);

/**
 * @brief Check whether the watch is unfaithful.
 *
 * The watch becomes unfaithful when its gateway changes, until the next sync.
 *
 * @return True if unfaithful; true by default.
 */
bool bt_persistent_storage_is_unfaithful(void);

/**
 * @brief Set whether the watch is unfaithful.
 *
 * @param is_unfaithful New state.
 */
void bt_persistent_storage_set_unfaithful(bool is_unfaithful);

/**
 * @brief Get a BLE root key.
 *
 * @param key_type Encryption root (ER) or identity root (IR).
 * @param[out] key_out Key, may be NULL.
 * @return True if a non-zero key is stored.
 */
bool bt_persistent_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                        struct pbl_bt_sm_key *key_out);

/**
 * @brief Store the BLE root keys.
 *
 * @param keys_in Array of @c PBL_BT_SM_ROOT_KEY_TYPE_NUM keys, indexed by key type. Ignored if
 *                NULL.
 */
void bt_persistent_storage_set_root_keys(struct pbl_bt_sm_key *keys_in);

/**
 * @brief Get the stored local device name.
 *
 * @param[out] local_device_name_out Destination buffer.
 * @param max_size Size of @p local_device_name_out in bytes.
 * @return True if a name is stored.
 */
bool bt_persistent_storage_get_local_device_name(char *local_device_name_out, size_t max_size);

/**
 * @brief Store the local device name.
 *
 * @param local_device_name Name to store. Ignored if NULL.
 * @param max_size Number of bytes to store.
 */
void bt_persistent_storage_set_local_device_name(char *local_device_name, size_t max_size);

/**
 * @brief Get the stored airplane mode setting.
 *
 * @return True if airplane mode is on; false by default.
 */
bool bt_persistent_storage_get_airplane_mode_enabled(void);

/**
 * @brief Store the airplane mode setting.
 *
 * @param enable Airplane mode state.
 */
void bt_persistent_storage_set_airplane_mode_enabled(bool enable);

/**
 * @brief Get the cached capabilities of the last system session.
 *
 * Prefer comm_session_get_capabilities() when connected.
 *
 * @param[out] capabilities_out Capabilities, zero if none cached. Ignored if NULL.
 */
void bt_persistent_storage_get_cached_system_capabilities(
    PebbleProtocolCapabilities *capabilities_out);

/**
 * @brief Cache the capabilities of the system session.
 *
 * When they change, they are persisted and a capabilities changed event is emitted.
 *
 * @param capabilities Capabilities, or NULL to clear the cache.
 */
void bt_persistent_storage_set_cached_system_capabilities(
    const PebbleProtocolCapabilities *capabilities);

/**
 * @brief Initialize the database, before the Bluetooth stack starts.
 *
 * Merges data from shared PRF storage and loads the cached capabilities.
 */
void bt_persistent_storage_init(void);

/**
 * @brief Delete all bondings, keeping the local device settings.
 */
void bt_persistent_storage_delete_all_pairings(void);

/**
 * @brief Read a raw database record, for unit tests.
 *
 * @param key Record key.
 * @param key_len Length of @p key in bytes.
 * @param[out] data_out Destination buffer.
 * @param buf_len Size of @p data_out in bytes.
 * @return Number of bytes read, 0 if not found.
 */
int bt_persistent_storage_get_raw_data(const void *key, size_t key_len, void *data_out,
                                       size_t buf_len);

/** @} */
