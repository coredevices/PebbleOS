/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/drivers/rng.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/bluetooth/pairability.h>
#include <pbl/util/size.h>

#include <comm/ble/gap_le_connect.h>
#include <comm/ble/gap_le_connection.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

#define MAX_BONDS 4U

static bool prv_get_prf_pairing(struct pbl_bt_bond_keys *keys, char *name) {
  memset(keys, 0, sizeof(*keys));

  return pbl_bt_prf_store_get_pairing(&keys->info, name, &keys->flags);
}

static bool prv_encryption_equal(const struct pbl_bt_bond_keys *a,
                                 const struct pbl_bt_bond_keys *b) {
  const struct pbl_bt_sm_pairing_info *ia = &a->info;
  const struct pbl_bt_sm_pairing_info *ib = &b->info;

  if (ia->is_local_encryption_info_valid != ib->is_local_encryption_info_valid ||
      ia->is_remote_encryption_info_valid != ib->is_remote_encryption_info_valid) {
    return false;
  }

  if (ia->is_local_encryption_info_valid &&
      (ia->local_encryption_info.ediv != ib->local_encryption_info.ediv ||
       ia->local_encryption_info.rand != ib->local_encryption_info.rand ||
       memcmp(&ia->local_encryption_info.ltk, &ib->local_encryption_info.ltk,
              sizeof(struct pbl_bt_sm_key)) != 0)) {
    return false;
  }

  if (ia->is_remote_encryption_info_valid &&
      (ia->remote_encryption_info.ediv != ib->remote_encryption_info.ediv ||
       ia->remote_encryption_info.rand != ib->remote_encryption_info.rand ||
       memcmp(&ia->remote_encryption_info.ltk, &ib->remote_encryption_info.ltk,
              sizeof(struct pbl_bt_sm_key)) != 0)) {
    return false;
  }

  return true;
}

static bool prv_keys_equal(const struct pbl_bt_bond_keys *a, const struct pbl_bt_bond_keys *b) {
  const struct pbl_bt_sm_pairing_info *ia = &a->info;
  const struct pbl_bt_sm_pairing_info *ib = &b->info;

  if (a->flags != b->flags || !bt_device_internal_equal(&ia->identity, &ib->identity) ||
      ia->is_remote_identity_info_valid != ib->is_remote_identity_info_valid) {
    return false;
  }

  if (ia->is_remote_identity_info_valid && memcmp(&ia->irk, &ib->irk, sizeof(ia->irk)) != 0) {
    return false;
  }

  return prv_encryption_equal(a, b);
}

static void prv_copy_to_prf_store(const struct pbl_bt_device_internal *addr) {
  struct pbl_bt_bond_keys keys;
  struct pbl_bt_bond_keys stored;

  if (pbl_bt_bond_get_keys(addr, &keys) != 0) {
    return;
  }

  if (prv_get_prf_pairing(&stored, nullptr)) {
    if (prv_keys_equal(&keys, &stored)) {
      return;
    }

    // The name there is another peer's.
    if (!bt_device_internal_equal(&keys.info.identity, &stored.info.identity)) {
      pbl_bt_prf_store_erase_pairing();
    }
  }

  pbl_bt_prf_store_set_pairing(&keys.info, nullptr, keys.flags);
}

struct others {
  const struct pbl_bt_device_internal *keep;
  struct pbl_bt_device_internal addrs[MAX_BONDS];
  size_t count;
};

static void prv_collect_others_cb(const struct pbl_bt_bond_info *info, void *user_data) {
  struct others *others = user_data;

  if (others->count < PBL_ARRAY_SIZE(others->addrs) &&
      !bt_device_internal_equal(&info->addr, others->keep)) {
    others->addrs[others->count++] = info->addr;
  }
}

static void prv_delete_others(const struct pbl_bt_device_internal *keep) {
  struct others others = {
    .keep = keep,
  };

  pbl_bt_foreach_bond(prv_collect_others_cb, &others);

  for (size_t i = 0U; i < others.count; i++) {
    (void)pbl_bt_unpair(&others.addrs[i]);
  }
}

struct first_bond {
  struct pbl_bt_device_internal addr;
  bool found;
};

static void prv_first_bond_cb(const struct pbl_bt_bond_info *info, void *user_data) {
  struct first_bond *first = user_data;

  if (!first->found) {
    first->addr = info->addr;
    first->found = true;
  }
}

void bt_bonding_init(void) {
  struct first_bond first = {};
  struct pbl_bt_bond_keys keys;

  if (prv_get_prf_pairing(&keys, nullptr)) {
    prv_delete_others(&keys.info.identity);
    if (pbl_bt_bond_set_keys(&keys) != 0) {
      PBL_LOG_ERR("Could not take the bond of the PRF store");
    }
    return;
  }

  pbl_bt_foreach_bond(prv_first_bond_cb, &first);
  if (!first.found) {
    return;
  }

  prv_delete_others(&first.addr);
  prv_copy_to_prf_store(&first.addr);
}

void bt_bonding_get_root_keys(struct pbl_bt_sm_key *keys) {
  if (pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]) &&
      pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    return;
  }

  for (size_t i = 0U; i < PBL_BT_SM_ROOT_KEY_TYPE_NUM; i++) {
    for (size_t j = 0U; j < sizeof(keys[i].data); j += sizeof(uint32_t)) {
      uint32_t word;

      if (rng_rand(&word) != 0) {
        PBL_LOG_ERR("Could not generate the root keys");
        memset(keys, 0, PBL_BT_SM_ROOT_KEY_TYPE_NUM * sizeof(*keys));
        return;
      }

      memcpy(&keys[i].data[j], &word, sizeof(word));
    }
  }

  pbl_bt_prf_store_set_root_keys(keys);
}

bool bt_bonding_get_peer_name(const struct pbl_bt_device_internal *addr, char *name) {
  struct pbl_bt_bond_keys stored;

  return prv_get_prf_pairing(&stored, name) &&
         bt_device_internal_equal(&stored.info.identity, addr) && name[0] != '\0';
}

void bt_bonding_set_peer_name(const struct pbl_bt_device_internal *addr, const char *name) {
  char stored_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  struct pbl_bt_bond_keys stored;

  if (!prv_get_prf_pairing(&stored, stored_name) ||
      !bt_device_internal_equal(&stored.info.identity, addr) ||
      strncmp(stored_name, name, sizeof(stored_name)) == 0) {
    return;
  }

  pbl_bt_prf_store_set_pairing_name(name);
}

void pbl_bt_cb_bond_added(const struct pbl_bt_bond_info *info) {
  prv_delete_others(&info->addr);
  prv_copy_to_prf_store(&info->addr);

  // Only one bond is kept, so a new one means a new phone.
  bt_settings_set_unfaithful(true);

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  gap_le_connect_handle_bond_added(info);
  bt_advertising_set_reconnectable(true);
  bt_pairability_update_due_to_bonding_change();
}

void pbl_bt_cb_bond_changed(const struct pbl_bt_bond_info *info) {
  prv_copy_to_prf_store(&info->addr);
}

void pbl_bt_cb_bond_deleted(const struct pbl_bt_device_internal *addr) {
  struct pbl_bt_bond_keys stored;

  // The peer paired again since.
  if (pbl_bt_bond_get(addr, nullptr) == 0) {
    return;
  }

  if (prv_get_prf_pairing(&stored, nullptr) &&
      bt_device_internal_equal(&stored.info.identity, addr)) {
    pbl_bt_prf_store_erase_pairing();
  }

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  gap_le_connection_handle_bond_deleted(addr);
  gap_le_connect_handle_bond_deleted(addr);
  bt_advertising_set_reconnectable(pbl_bt_bond_count() > 0);
  bt_pairability_update_due_to_bonding_change();
}
