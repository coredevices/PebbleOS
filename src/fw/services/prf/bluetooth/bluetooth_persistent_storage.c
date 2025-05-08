/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "services/common/bluetooth/bluetooth_persistent_storage.h"

#include "comm/ble/gap_le_connect.h"
#include "comm/ble/gap_le_slave_reconnect.h"

#include "comm/bt_lock.h"

#include "services/common/bluetooth/pairability.h"
#include "services/common/analytics/analytics.h"
#include "services/normal/settings/settings_file.h"
#include "services/common/shared_prf_storage/shared_prf_storage.h"

#include "comm/ble/kernel_le_client/kernel_le_client.h"

#include "system/logging.h"

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/bonding_sync.h>
#include <bluetooth/features.h>
#include <btutil/bt_device.h>
#include <btutil/sm_util.h>


//! This is just an interface for the shared PRF storage


//! These don't matter at all
#define BLE_BONDING_ID (0)
#define BT_CLASSIC_BONDING_ID (1)


///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Info

static void prv_call_ble_bonding_change_handlers(BTBondingID bonding, BtPersistBondingOp op) {
  gap_le_connect_handle_bonding_change(bonding, op);
  kernel_le_client_handle_bonding_change(bonding, op);
  bt_pairability_update_due_to_bonding_change();
}

static BTBondingID prv_bt_persistent_storage_store_ble_pairing(
    const SMPairingInfo *new_pairing_info, bool is_gateway, bool requires_address_pinning,
    uint8_t flags, const char *device_name, BtPersistBondingOp op) {
  if (new_pairing_info && is_gateway) {
    shared_prf_storage_store_ble_pairing_data(new_pairing_info, device_name,
                                              requires_address_pinning,
                                              flags);
    prv_call_ble_bonding_change_handlers(BLE_BONDING_ID, op);
    return BLE_BONDING_ID;
  }

  return BT_BONDING_ID_INVALID;
}

bool bt_persistent_storage_set_ble_pinned_address(const BTDeviceAddress *addr) {
  shared_prf_storage_set_ble_pinned_address(addr);
  return true;
}

bool bt_persistent_storage_has_pinned_ble_pairings(void) {
  bool requires_address_pinning_out = false;
  shared_prf_storage_get_ble_pairing_data(NULL, NULL, &requires_address_pinning_out, NULL);
  return requires_address_pinning_out;
}

bool bt_persistent_storage_get_ble_pinned_address(BTDeviceAddress *address_out) {
  return shared_prf_storage_get_ble_pinned_address(address_out);
}

BTBondingID bt_persistent_storage_store_ble_pairing(const SMPairingInfo *new_pairing_info,
                                                    bool is_gateway, const char *device_name,
                                                    bool requires_address_pinning,
                                                    uint8_t flags) {
  // We only have one slot in PRF and all pairing info (except the device
  // name) will arrive in one-shot so anytime this routine gets called it
  // means we have 'added' a new pairing

  bool is_updating_existing = false;
  SMPairingInfo existing_pairing_info;
  if (shared_prf_storage_get_ble_pairing_data(&existing_pairing_info, NULL, NULL, NULL)) {
    if (sm_is_pairing_info_equal_identity(new_pairing_info, &existing_pairing_info)) {
      // Treat re-pairing an existing device as an "update" instead of deletion+addition,
      // because there is only one bonding ID that gets re-used, a deletion would otherwise cause a
      // disconnection to happen. See PBL-24737.
      PBL_LOG(LOG_LEVEL_INFO, "Re-pairing previously paired LE device");
      is_updating_existing = true;
    } else {
      // Since we only have one slot, this means we are about to delete what was
      // already there so handle the deletion if a valid pairing was stored
      prv_call_ble_bonding_change_handlers(BLE_BONDING_ID, BtPersistBondingOpWillDelete);
    }
  }

  BtPersistBondingOp pairing_op =
      is_updating_existing ? BtPersistBondingOpDidChange : BtPersistBondingOpDidAdd;
  return (prv_bt_persistent_storage_store_ble_pairing(new_pairing_info, is_gateway,
                                               requires_address_pinning,
                                               flags, device_name, pairing_op));
}

bool bt_persistent_storage_update_ble_device_name(BTBondingID bonding, const char *device_name) {
  // A device name has come in, update the name of our currently paired device
  SMPairingInfo data = {};
  bool requires_address_pinning = false;
  uint8_t flags = 0;
  if (!shared_prf_storage_get_ble_pairing_data(&data, NULL, &requires_address_pinning, &flags)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Tried to store device name, but pairing no longer around.");
    return false;
  }
  // In PRF, only the gateway should get paired, so default to "true":
  return (BT_BONDING_ID_INVALID !=
          prv_bt_persistent_storage_store_ble_pairing(&data, true /* is_gateway */,
                                                      requires_address_pinning, flags,
                                                      device_name, BtPersistBondingOpDidChange));
}

static void prv_remove_ble_bonding_from_bt_driver(void) {
  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }
  BleBonding bonding = {
    .is_gateway = true,
  };
  if (!shared_prf_storage_get_ble_pairing_data(&bonding.pairing_info, NULL, NULL, NULL)) {
    return;
  }
  bt_driver_handle_host_removed_bonding(&bonding);
}

void bt_persistent_storage_delete_ble_pairing_by_id(BTBondingID bonding) {
  prv_remove_ble_bonding_from_bt_driver();
  shared_prf_storage_erase_ble_pairing_data();
  prv_call_ble_bonding_change_handlers(bonding, BtPersistBondingOpWillDelete);
}

void bt_persistent_storage_delete_ble_pairing_by_addr(const BTDeviceInternal *device) {
  bt_persistent_storage_delete_ble_pairing_by_id(BLE_BONDING_ID);
}

bool bt_persistent_storage_get_ble_pairing_by_id(BTBondingID bonding,
                                          SMIdentityResolvingKey *IRK_out,
                                          BTDeviceInternal *device_out,
                                          char *name_out) {
  SMPairingInfo data;
  char name[BT_DEVICE_NAME_BUFFER_SIZE];
  if (!shared_prf_storage_get_ble_pairing_data(&data, name, NULL, NULL)) {
    return false;
  }

  if (IRK_out) {
    *IRK_out = data.irk;
  }
  if (device_out) {
    *device_out = data.identity;
  }
  if (name_out) {
    strncpy(name_out, name, BT_DEVICE_NAME_BUFFER_SIZE);
    name_out[BT_DEVICE_NAME_BUFFER_SIZE - 1] = 0;
  }

  return true;
}

bool bt_persistent_storage_get_ble_pairing_by_addr(const BTDeviceInternal *device,
                                                 SMIdentityResolvingKey *IRK_out,
                                                 char name[BT_DEVICE_NAME_BUFFER_SIZE]) {
  BTDeviceInternal device_out = {};
  bool rv = bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, IRK_out, &device_out, name);
  return (rv && bt_device_equal(&device->opaque, &device_out.opaque));
}

void bt_persistent_storage_set_active_ble_gateway(BTBondingID bonding) {
}

BTBondingID bt_persistent_storage_get_ble_ancs_bonding(void) {
  return BLE_BONDING_ID;
}

bool bt_persistent_storage_is_ble_ancs_bonding(BTBondingID bonding) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, NULL, NULL, NULL);
}

bool bt_persistent_storage_has_ble_ancs_bonding(void) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, NULL, NULL, NULL);
}

bool bt_persistent_storage_has_active_ble_gateway_bonding(void) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, NULL, NULL, NULL);
}

void bt_persistent_storage_for_each_ble_pairing(BtPersistBondingDBEachBLE cb, void *context) {
  return;
}

void bt_persistent_storage_register_existing_ble_bondings(void) {
  BleBonding bonding = {};
  uint8_t flags;
  if (!shared_prf_storage_get_ble_pairing_data(&bonding.pairing_info, NULL, NULL, &flags)) {
    return;
  }
  bonding.is_gateway = true;
  bonding.flags = flags;
  bt_driver_handle_host_added_bonding(&bonding);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BT Classic Pairing Info


static void prv_call_bt_classic_bonding_change_handlers(BTBondingID bonding,
                                                        BtPersistBondingOp op) {
  bt_pairability_update_due_to_bonding_change();
}

BTBondingID bt_persistent_storage_store_bt_classic_pairing(BTDeviceAddress *address,
                                                           SM128BitKey *key,
                                                           char *name, uint8_t *platform_bits) {
  if (address) {
    if (key) {
      // We should really collect all of the classic info and store once its complete
      // However, since platform bits are going to be the last piece collected its ok
      // to 0 it out here
      uint8_t platform_bits_val = platform_bits ? *platform_bits : 0x00;
      shared_prf_storage_store_bt_classic_pairing_data(address, name, key, platform_bits_val);
    }
    if (platform_bits) {
      shared_prf_storage_store_platform_bits(*platform_bits);
    }
    prv_call_bt_classic_bonding_change_handlers(BT_CLASSIC_BONDING_ID, BtPersistBondingOpDidChange);
    return BT_CLASSIC_BONDING_ID;
  }

  return BT_BONDING_ID_INVALID;
}

void bt_persistent_storage_delete_bt_classic_pairing_by_id(BTBondingID bonding) {
  shared_prf_storage_erase_bt_classic_pairing_data();
  prv_call_bt_classic_bonding_change_handlers(bonding, BtPersistBondingOpWillDelete);
  bt_pairability_update_due_to_bonding_change();
}

void bt_persistent_storage_delete_bt_classic_pairing_by_addr(const BTDeviceAddress *bd_addr) {
  if (!bd_addr) {
    return;
  }

  bt_persistent_storage_delete_bt_classic_pairing_by_id(BT_CLASSIC_BONDING_ID);
}

bool bt_persistent_storage_get_bt_classic_pairing_by_id(BTBondingID bonding,
                                                 BTDeviceAddress *address_out,
                                                 SM128BitKey *link_key_out,
                                                 char *name_out,
                                                 uint8_t *platform_bits_out) {
  BTDeviceAddress addr;
  char name[BT_DEVICE_NAME_BUFFER_SIZE];
  SM128BitKey link_key;
  uint8_t platform_bits;
  if (!shared_prf_storage_get_bt_classic_pairing_data(&addr, name, &link_key, &platform_bits)) {
    return false;
  }

  if (address_out) {
    *address_out = addr;
  }
  if (link_key_out) {
    *link_key_out = link_key;
  }
  if (name_out) {
    strncpy(name_out, name, BT_DEVICE_NAME_BUFFER_SIZE);
    name_out[BT_DEVICE_NAME_BUFFER_SIZE - 1] = 0;
  }
  if (platform_bits_out) {
    *platform_bits_out = platform_bits;
  }

  return true;
}

BTBondingID bt_persistent_storage_get_bt_classic_pairing_by_addr(BTDeviceAddress* addr_in,
                                                          SM128BitKey *link_key_out,
                                                          char *name_out,
                                                          uint8_t *platform_bits_out) {
  if (bt_persistent_storage_get_bt_classic_pairing_by_id(BT_CLASSIC_BONDING_ID, NULL, link_key_out,
                                              name_out, platform_bits_out)) {
    return BT_CLASSIC_BONDING_ID;
  }

  return BT_BONDING_ID_INVALID;
}

bool bt_persistent_storage_has_active_bt_classic_gateway_bonding(void) {
  return bt_persistent_storage_get_bt_classic_pairing_by_id(BT_CLASSIC_BONDING_ID,
                                                            NULL, NULL, NULL, NULL);
}

void bt_persistent_storage_for_each_bt_classic_pairing(BtPersistBondingDBEachBTClassic cb,
                                                       void *context) {
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Local Device Info

void bt_persistent_storage_set_active_gateway(BTBondingID bonding) {
  return;
}

bool bt_persistent_storage_get_active_gateway(BTBondingID *bonding_out,
                                              BtPersistBondingType *type_out) {
  if (bt_persistent_storage_get_bt_classic_pairing_by_id(BT_CLASSIC_BONDING_ID,
                                                         NULL, NULL, NULL, NULL)) {
    *bonding_out = BT_CLASSIC_BONDING_ID;
    *type_out = BtPersistBondingTypeBTClassic;
    return true;
  } else {
    return false;
  }
}

bool bt_persistent_storage_is_unfaithful(void) {
  return true;
}

void bt_persistent_storage_set_unfaithful(bool is_unfaithful) {
  return;
}

bool bt_persistent_storage_get_root_key(SMRootKeyType key_type, SM128BitKey *key_out) {
  return shared_prf_storage_get_root_key(key_type, key_out);
}

void bt_persistent_storage_set_root_keys(SM128BitKey *keys_in) {
  shared_prf_storage_set_root_keys(keys_in);
}

bool bt_persistent_storage_get_local_device_name(char *local_device_name_out, size_t max_size) {
  return shared_prf_storage_get_local_device_name(local_device_name_out, max_size);
}

void bt_persistent_storage_set_local_device_name(char *local_device_name, size_t size) {
  shared_prf_storage_set_local_device_name(local_device_name);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Remote Device Info

void bt_persistent_storage_get_cached_system_capabilities(
    PebbleProtocolCapabilities *capabilities_out) {
}

void bt_persistent_storage_set_cached_system_capabilities(
    const PebbleProtocolCapabilities *capabilities) {
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Common

void bt_persistent_storage_init(void) {
}

void bt_persistent_storage_delete_all(void) {
}

void bt_persistent_storage_delete_all_pairings(void) {
  bt_persistent_storage_delete_ble_pairing_by_id(BLE_BONDING_ID);
  if (bt_driver_supports_bt_classic()) {
    bt_persistent_storage_delete_bt_classic_pairing_by_id(BT_CLASSIC_BONDING_ID);
  }
}
