/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bonding_sync.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>
#include <pbl/services/bluetooth/pairability.h>
#include <pbl/settings/settings.h>

#include <comm/ble/gap_le_connect.h>
#include <comm/ble/kernel_le_client/kernel_le_client.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

//! This is just an interface for the shared PRF storage

//! These don't matter at all
#define BLE_BONDING_ID (0)

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Info

static void prv_call_ble_bonding_change_handlers(pbl_bt_bonding_id_t bonding,
                                                 BtPersistBondingOp op) {
  gap_le_connect_handle_bonding_change(bonding, op);
  kernel_le_client_handle_bonding_change(bonding, op);
  bt_pairability_update_due_to_bonding_change();
}

// The Bluetooth stack keeps the pairing in the PRF store itself. Tracked here to tell a new
// pairing from an update of the one there was.
static struct pbl_bt_sm_pairing_info s_pairing_info;
static bool s_has_pairing;

pbl_bt_bonding_id_t bt_persistent_storage_store_ble_pairing(
    const struct pbl_bt_sm_pairing_info *new_pairing_info, bool is_gateway, const char *device_name,
    uint8_t flags) {
  BtPersistBondingOp op = BtPersistBondingOpDidAdd;

  if (!new_pairing_info || !is_gateway) {
    PBL_LOG_WRN("BLE pairing not stored (gateway=%u)", is_gateway);
    return PBL_BT_BONDING_ID_INVALID;
  }

  if (s_has_pairing) {
    if (sm_is_pairing_info_equal_identity(new_pairing_info, &s_pairing_info)) {
      // There is only one bonding ID: a deletion and an addition would disconnect.
      PBL_LOG_INFO("Re-pairing previously paired LE device");
      op = BtPersistBondingOpDidChange;
    } else {
      prv_call_ble_bonding_change_handlers(BLE_BONDING_ID, BtPersistBondingOpWillDelete);
    }
  }

  PBL_LOG_INFO("Storing BLE pairing: addr=" PBL_BT_ADDR_FMT " random=%u",
               PBL_BT_ADDR_XPLODE(new_pairing_info->identity.address),
               new_pairing_info->identity.is_random_address);

  s_pairing_info = *new_pairing_info;
  s_has_pairing = true;

  if (device_name) {
    pbl_bt_prf_store_set_pairing_name(device_name);
  }

  prv_call_ble_bonding_change_handlers(BLE_BONDING_ID, op);

  return BLE_BONDING_ID;
}

bool bt_persistent_storage_update_ble_device_name(pbl_bt_bonding_id_t bonding,
                                                  const char *device_name) {
  char existing_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE] = {};

  if (!pbl_bt_prf_store_get_pairing(nullptr, existing_name, nullptr)) {
    PBL_LOG_ERR("Tried to store device name, but pairing no longer around.");
    return false;
  }

  if (strncmp(existing_name, device_name, PBL_BT_DEVICE_NAME_BUFFER_SIZE) == 0) {
    return true;
  }

  pbl_bt_prf_store_set_pairing_name(device_name);
  prv_call_ble_bonding_change_handlers(BLE_BONDING_ID, BtPersistBondingOpDidChange);

  return true;
}

static void prv_remove_ble_bonding_from_backend(void) {
  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }
  struct pbl_bt_bonding bonding = {
    .is_gateway = true,
  };
  if (!pbl_bt_prf_store_get_pairing(&bonding.pairing_info, nullptr, nullptr)) {
    return;
  }
  pbl_bt_handle_host_removed_bonding(&bonding);
}

void bt_persistent_storage_delete_ble_pairing_by_id(pbl_bt_bonding_id_t bonding) {
  PBL_LOG_INFO("Deleting stored BLE pairing");
  prv_remove_ble_bonding_from_backend();
  pbl_bt_prf_store_erase_pairing();
  s_has_pairing = false;
  prv_call_ble_bonding_change_handlers(bonding, BtPersistBondingOpWillDelete);
}

void bt_persistent_storage_delete_ble_pairing_by_addr(const struct pbl_bt_device_internal *device) {
  bt_persistent_storage_delete_ble_pairing_by_id(BLE_BONDING_ID);
}

bool bt_persistent_storage_get_ble_pairing_by_id(pbl_bt_bonding_id_t bonding,
                                                 struct pbl_bt_sm_key *IRK_out,
                                                 struct pbl_bt_device_internal *device_out,
                                                 char *name_out) {
  struct pbl_bt_sm_pairing_info data;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  if (!pbl_bt_prf_store_get_pairing(&data, name, nullptr)) {
    return false;
  }

  if (IRK_out) {
    *IRK_out = data.irk;
  }
  if (device_out) {
    *device_out = data.identity;
  }
  if (name_out) {
    strncpy(name_out, name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
    name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE - 1] = 0;
  }

  return true;
}

bool bt_persistent_storage_get_ble_pairing_by_addr(const struct pbl_bt_device_internal *device,
                                                   struct pbl_bt_sm_key *IRK_out,
                                                   char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  struct pbl_bt_device_internal device_out = {};
  bool rv = bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, IRK_out, &device_out, name);
  return (rv && bt_device_internal_equal(device, &device_out));
}

void bt_persistent_storage_set_active_ble_gateway(pbl_bt_bonding_id_t bonding) {
}

pbl_bt_bonding_id_t bt_persistent_storage_get_ble_ancs_bonding(void) {
  return BLE_BONDING_ID;
}

bool bt_persistent_storage_is_ble_ancs_bonding(pbl_bt_bonding_id_t bonding) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, nullptr, nullptr, nullptr);
}

bool bt_persistent_storage_has_ble_ancs_bonding(void) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, nullptr, nullptr, nullptr);
}

bool bt_persistent_storage_has_active_ble_gateway_bonding(void) {
  return bt_persistent_storage_get_ble_pairing_by_id(BLE_BONDING_ID, nullptr, nullptr, nullptr);
}

void bt_persistent_storage_for_each_ble_pairing(BtPersistBondingDBEachBLE cb, void *context) {
  return;
}

// The Bluetooth stack loads the pairing from the PRF store itself.
void bt_persistent_storage_register_existing_ble_bondings(void) {
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Local Device Info

void bt_persistent_storage_set_active_gateway(pbl_bt_bonding_id_t bonding) {
  return;
}

bool bt_persistent_storage_get_active_gateway(pbl_bt_bonding_id_t *bonding_out,
                                              BtPersistBondingType *type_out) {
  return false;
}

bool bt_persistent_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                        struct pbl_bt_sm_key *key_out) {
  return pbl_bt_prf_store_get_root_key(key_type, key_out);
}

void bt_persistent_storage_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  pbl_bt_prf_store_set_root_keys(keys_in);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Common

void bt_persistent_storage_init(void) {
  s_has_pairing = pbl_bt_prf_store_get_pairing(&s_pairing_info, nullptr, nullptr);
}

void bt_persistent_storage_delete_all(void) {
}

void bt_persistent_storage_delete_all_pairings(void) {
  bt_persistent_storage_delete_ble_pairing_by_id(BLE_BONDING_ID);
}
