/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include <string.h>

#include <pbl/btutil/bt_device.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/util/rand32.h>

#include <bluetooth/nimble_type_conversions.h>
#include <host/ble_hs.h>

static bool prv_has_record(const struct pbl_bt_sm_pairing_info *info, int obj_type) {
  if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
    return info->is_local_encryption_info_valid;
  }

  return info->is_remote_encryption_info_valid || info->is_remote_identity_info_valid ||
         info->is_remote_signing_info_valid;
}

static void prv_clear_record(struct pbl_bt_sm_pairing_info *info, int obj_type) {
  if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
    info->local_encryption_info = (struct pbl_bt_sm_local_encryption_info){};
    info->is_local_encryption_info_valid = false;
    return;
  }

  info->remote_encryption_info = (struct pbl_bt_sm_remote_encryption_info){};
  info->irk = (struct pbl_bt_sm_key){};
  info->csrk = (struct pbl_bt_sm_key){};
  info->is_remote_encryption_info_valid = false;
  info->is_remote_identity_info_valid = false;
  info->is_remote_signing_info_valid = false;
}

static void prv_store(const struct pbl_bt_sm_pairing_info *info, uint8_t flags) {
  if (sm_is_pairing_info_empty(info)) {
    shared_prf_storage_erase_ble_pairing_data();
    return;
  }

  shared_prf_storage_store_ble_pairing_data(info, nullptr, flags);
}

int pbl_bt__sprf_read_sec(int obj_type, const struct ble_store_key_sec *key_sec,
                          struct ble_store_value_sec *value_sec) {
  struct pbl_bt_sm_pairing_info info;
  ble_addr_t peer_addr;
  uint8_t flags;

  if (!shared_prf_storage_get_ble_pairing_data(&info, nullptr, &flags) ||
      !prv_has_record(&info, obj_type) || key_sec->idx != 0U) {
    return BLE_HS_ENOENT;
  }

  pebble_device_to_nimble_addr(&info.identity, &peer_addr);
  if (ble_addr_cmp(&key_sec->peer_addr, BLE_ADDR_ANY) != 0 &&
      ble_addr_cmp(&key_sec->peer_addr, &peer_addr) != 0) {
    return BLE_HS_ENOENT;
  }

  *value_sec = (struct ble_store_value_sec){
    .peer_addr = peer_addr,
    .key_size = PBL_BT__STORE_KEY_SIZE,
    .sc = (flags & PBL_BT__STORE_FLAG_SC) != 0U,
    .authenticated = (flags & PBL_BT__STORE_FLAG_AUTH) != 0U,
  };

  if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
    value_sec->ltk_present = true;
    value_sec->ediv = info.local_encryption_info.ediv;
    value_sec->rand_num = info.local_encryption_info.rand;
    memcpy(value_sec->ltk, info.local_encryption_info.ltk.data, PBL_BT__STORE_KEY_SIZE);
    return 0;
  }

  if (info.is_remote_encryption_info_valid) {
    value_sec->ltk_present = true;
    value_sec->ediv = info.remote_encryption_info.ediv;
    value_sec->rand_num = info.remote_encryption_info.rand;
    memcpy(value_sec->ltk, info.remote_encryption_info.ltk.data, PBL_BT__STORE_KEY_SIZE);
  }

  if (info.is_remote_identity_info_valid) {
    value_sec->irk_present = true;
    memcpy(value_sec->irk, info.irk.data, PBL_BT__STORE_KEY_SIZE);
  }

  if (info.is_remote_signing_info_valid) {
    value_sec->csrk_present = true;
    memcpy(value_sec->csrk, info.csrk.data, PBL_BT__STORE_KEY_SIZE);
  }

  return 0;
}

int pbl_bt__sprf_write_sec(int obj_type, const struct ble_store_value_sec *value_sec,
                           ble_addr_t *replaced) {
  struct pbl_bt_sm_pairing_info info;
  struct pbl_bt_device_internal identity;
  uint8_t flags;

  if (replaced != nullptr) {
    *replaced = *BLE_ADDR_ANY;
  }

  // The shared PRF storage layout has no key size.
  if (value_sec->key_size != PBL_BT__STORE_KEY_SIZE) {
    return BLE_HS_ENOTSUP;
  }

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &identity);

  if (shared_prf_storage_get_ble_pairing_data(&info, nullptr, &flags)) {
    if (!bt_device_internal_equal(&info.identity, &identity)) {
      if (replaced != nullptr) {
        pebble_device_to_nimble_addr(&info.identity, replaced);
      }
      // Drops the name of the previous peer too.
      shared_prf_storage_erase_ble_pairing_data();
      info = (struct pbl_bt_sm_pairing_info){};
    }
  } else {
    info = (struct pbl_bt_sm_pairing_info){};
  }

  info.identity = identity;
  info.is_mitm_protection_enabled = value_sec->authenticated;
  flags = (value_sec->sc ? PBL_BT__STORE_FLAG_SC : 0U) |
          (value_sec->authenticated ? PBL_BT__STORE_FLAG_AUTH : 0U);

  prv_clear_record(&info, obj_type);

  if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
    if (value_sec->ltk_present) {
      info.is_local_encryption_info_valid = true;
      info.local_encryption_info.ediv = value_sec->ediv;
      info.local_encryption_info.rand = value_sec->rand_num;
      memcpy(info.local_encryption_info.ltk.data, value_sec->ltk, PBL_BT__STORE_KEY_SIZE);
    }
  } else {
    if (value_sec->ltk_present) {
      info.is_remote_encryption_info_valid = true;
      info.remote_encryption_info.ediv = value_sec->ediv;
      info.remote_encryption_info.rand = value_sec->rand_num;
      memcpy(info.remote_encryption_info.ltk.data, value_sec->ltk, PBL_BT__STORE_KEY_SIZE);
    }
    if (value_sec->irk_present) {
      info.is_remote_identity_info_valid = true;
      memcpy(info.irk.data, value_sec->irk, PBL_BT__STORE_KEY_SIZE);
    }
    if (value_sec->csrk_present) {
      info.is_remote_signing_info_valid = true;
      memcpy(info.csrk.data, value_sec->csrk, PBL_BT__STORE_KEY_SIZE);
    }
  }

  prv_store(&info, flags);

  return 0;
}

int pbl_bt__sprf_delete_sec(int obj_type, const ble_addr_t *peer_addr) {
  struct pbl_bt_sm_pairing_info info;
  ble_addr_t stored_addr;
  uint8_t flags;

  if (!shared_prf_storage_get_ble_pairing_data(&info, nullptr, &flags) ||
      !prv_has_record(&info, obj_type)) {
    return BLE_HS_ENOENT;
  }

  pebble_device_to_nimble_addr(&info.identity, &stored_addr);
  if (ble_addr_cmp(peer_addr, BLE_ADDR_ANY) != 0 && ble_addr_cmp(peer_addr, &stored_addr) != 0) {
    return BLE_HS_ENOENT;
  }

  prv_clear_record(&info, obj_type);
  prv_store(&info, flags);

  return 0;
}

int pbl_bt__sprf_irk_get(uint8_t irk[PBL_BT__STORE_KEY_SIZE]) {
  struct pbl_bt_sm_key key;

  if (!shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, &key)) {
    return BLE_HS_ENOENT;
  }

  memcpy(irk, key.data, PBL_BT__STORE_KEY_SIZE);

  return 0;
}

void pbl_bt__sprf_irk_set(const uint8_t irk[PBL_BT__STORE_KEY_SIZE]) {
  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  struct pbl_bt_sm_key *er = &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION];

  // The stack does not use the encryption root key, but shared PRF storage holds one.
  if (!shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION, er)) {
    for (size_t i = 0; i < sizeof(er->data); i += sizeof(uint32_t)) {
      const uint32_t r = pbl_rand32();
      memcpy(&er->data[i], &r, sizeof(r));
    }
  }

  memcpy(keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY].data, irk, PBL_BT__STORE_KEY_SIZE);
  shared_prf_storage_set_root_keys(keys);
}
