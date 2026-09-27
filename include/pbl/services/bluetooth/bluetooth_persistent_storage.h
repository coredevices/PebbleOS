/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/comm_session/session_remote_version.h"

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/bonding_sync.h>
#include <pbl/bluetooth/sm_types.h>

//! @file bluetooth_persistent_storage.h
//! Future file-based database for Bluetooth related credentials, cached data, etc.
//! The idea is to replace the deprecated, registry-based "remote_prefs.c" and "known_device_list.c"
//! For now this module contains temporary implementations that use the legacy registry.

typedef enum {
  BtPersistBondingOpInvalid = -1,
  BtPersistBondingOpDidAdd,
  BtPersistBondingOpDidChange,
  BtPersistBondingOpWillDelete,
} BtPersistBondingOp;

typedef enum {
  BtPersistBondingTypeBTClassic,
  BtPersistBondingTypeBLE,
  BtPersistBondingNumTypes
} BtPersistBondingType;

//! Signature of function that handles changes in the pairing database
typedef void (*BtPersistBondingChangeHandler)(pbl_bt_bonding_id_t affected_bonding,
                                              BtPersistBondingOp operation);

typedef void (*BtPersistBondingDBEachBLE)(struct pbl_bt_device_internal *device,
                                          struct pbl_bt_sm_key *irk, const char *name,
                                          pbl_bt_bonding_id_t *id, void *context);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Info

bool bt_persistent_storage_has_pinned_ble_pairings(void);

bool bt_persistent_storage_set_ble_pinned_address(const struct pbl_bt_addr *address);

bool bt_persistent_storage_get_ble_pinned_address(struct pbl_bt_addr *address_out);

pbl_bt_bonding_id_t bt_persistent_storage_store_ble_pairing(
    const struct pbl_bt_sm_pairing_info *pairing_info, bool is_gateway, const char *device_name,
    bool requires_address_pinning, uint8_t flags);

bool bt_persistent_storage_update_ble_device_name(pbl_bt_bonding_id_t bonding,
                                                  const char *device_name);

void bt_persistent_storage_delete_ble_pairing_by_id(pbl_bt_bonding_id_t);

void bt_persistent_storage_delete_ble_pairing_by_addr(const struct pbl_bt_device_internal *device);

bool bt_persistent_storage_get_ble_pairing_by_id(pbl_bt_bonding_id_t bonding,
                                                 struct pbl_bt_sm_key *IRK_out,
                                                 struct pbl_bt_device_internal *device_out,
                                                 char *name_out);

bool bt_persistent_storage_get_ble_pairing_by_addr(const struct pbl_bt_device_internal *device,
                                                   struct pbl_bt_sm_key *IRK_out,
                                                   char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

//! Returns the first ANCS supported bonding that is found
//! The case of having multiple supported ANCS bondings isn't handled well yet.
//! When this happens this could easily be changed to a for_each_ancs_supported_bonding(cb)
pbl_bt_bonding_id_t bt_persistent_storage_get_ble_ancs_bonding(void);

//! Returns true if the bondings is BLE and supports ANCS
bool bt_persistent_storage_is_ble_ancs_bonding(pbl_bt_bonding_id_t bonding);

//! Returns true if there exists a BLE bonding which supports ANCS
bool bt_persistent_storage_has_ble_ancs_bonding(void);

//! Returns true if the active gateway uses BLE
//! [PG]: This will currently always return false until PPoGATT is supported
bool bt_persistent_storage_has_active_ble_gateway_bonding(void);

//! Runs the callback for each BLE pairing
//! The callback is NOT allowed to acquire the bt_lock() (or we could deadlock).
void bt_persistent_storage_for_each_ble_pairing(BtPersistBondingDBEachBLE cb, void *context);

//! Registers all the existing BLE bondings with the BT driver lib.
void bt_persistent_storage_register_existing_ble_bondings(void);

pbl_bt_cccd_id_t bt_persistent_storage_store_cccd(const struct pbl_bt_cccd *cccd);

bool bt_persistent_storage_delete_cccd(const struct pbl_bt_device_internal *peer,
                                       uint16_t chr_val_handle);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Local Device Info

//! Updates the active gateway (the gateway which implements PP)
//! This bonding is used for BT Classic reconnection as well
//! @param bonding The desired active gateway
void bt_persistent_storage_set_active_gateway(pbl_bt_bonding_id_t bonding);

//! Returns false if no active gateway exists, true if one does exist
//! bonding_out and type_out are only valid when this function returns true;
bool bt_persistent_storage_get_active_gateway(pbl_bt_bonding_id_t *bonding_out,
                                              BtPersistBondingType *type_out);

//! Returns true when the active gateway is changed until a sync happens
bool bt_persistent_storage_is_unfaithful(void);

//! Marks the device as being unfaithful
void bt_persistent_storage_set_unfaithful(bool is_unfaithful);

//! Copies the BLE Encryption Root (ER) or Identity Root (IR) keys out of storage
//! @param key_out Storage into which ER or IR should be copied.
//! @param key_type The type of key to copy
//! @return true if ER and IR are copied, false if there are no keys have been found to copy.
bool bt_persistent_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                        struct pbl_bt_sm_key *key_out);

//! Stores new BLE Encryption Root (ER) and Identity Root (IR) keys
void bt_persistent_storage_set_root_keys(struct pbl_bt_sm_key *keys_in);

//! @param local_device_name_out Storage for the local device name.
//! @param max_size Size of the local_device_name_out buffer
//! @return true if there is a valid local device name stored, otherwise false (a zero-length string
bool bt_persistent_storage_get_local_device_name(char *local_device_name_out, size_t max_size);

//! Stores the customized local device name
//! @param local_device_name The device name to store
//! @param max_size The size of the string
void bt_persistent_storage_set_local_device_name(char *local_device_name, size_t max_size);

//! Retrieve the airplane mode setting
//! @return the stored airplane mode flag
bool bt_persistent_storage_get_airplane_mode_enabled(void);

//! Store the airplane mode setting
//! @param enable the airplane mode state to be saved
void bt_persistent_storage_set_airplane_mode_enabled(bool enable);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Remote Device Info

//! Retrieve the most recent system session capabilities
//! @param capabilities_out Storage for system session capabilities
//! @note It's preferable to use \ref comm_session_get_capabilities when possible
void bt_persistent_storage_get_cached_system_capabilities(
    PebbleProtocolCapabilities *capabilities_out);

//! Store the most recent system session capabilities
//! @param capabilities The capability flags to be saved (cache will be cleared if NULL)
void bt_persistent_storage_set_cached_system_capabilities(
    const PebbleProtocolCapabilities *capabilities);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Common

void bt_persistent_storage_init(void);

//! This will not delete the local device info, only pairings
void bt_persistent_storage_delete_all_pairings(void);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Unit testing
int bt_persistent_storage_get_raw_data(const void *key, size_t key_len, void *data_out,
                                       size_t buf_len);
