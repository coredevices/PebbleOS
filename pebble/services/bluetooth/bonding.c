/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/drivers/rng.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/bluetooth/link.h>
#include <pbl/services/bluetooth/pairability.h>
#include <pbl/util/size.h>

#include <syscfg/syscfg.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

static bool prv_get_prf_pairing(struct pbl_bt_bond_keys *keys, char *name) {
  memset(keys, 0, sizeof(*keys));

  return pbl_bt_prf_store_get_pairing(&keys->peer, &keys->our, name);
}

static bool prv_ltk_equal(const struct ble_store_value_sec *a,
                          const struct ble_store_value_sec *b) {
  if (a->ltk_present != b->ltk_present) {
    return false;
  }

  return !a->ltk_present || (a->ediv == b->ediv && a->rand_num == b->rand_num &&
                             memcmp(a->ltk, b->ltk, sizeof(a->ltk)) == 0);
}

// Of what the PRF store keeps.
static bool prv_keys_equal(const struct pbl_bt_bond_keys *a, const struct pbl_bt_bond_keys *b) {
  const struct ble_store_value_sec *pa = &a->peer;
  const struct ble_store_value_sec *pb = &b->peer;

  if (ble_addr_cmp(&pa->peer_addr, &pb->peer_addr) != 0 || pa->sc != pb->sc ||
      pa->authenticated != pb->authenticated || pa->irk_present != pb->irk_present) {
    return false;
  }

  if (pa->irk_present && memcmp(pa->irk, pb->irk, sizeof(pa->irk)) != 0) {
    return false;
  }

  return prv_ltk_equal(pa, pb) && prv_ltk_equal(&a->our, &b->our);
}

static void prv_copy_to_prf_store(const ble_addr_t *addr) {
  struct pbl_bt_bond_keys keys;
  struct pbl_bt_bond_keys stored;

  if (pbl_bt_bond_get_keys(addr, &keys) != 0) {
    return;
  }

  if (prv_get_prf_pairing(&stored, nullptr) && prv_keys_equal(&keys, &stored)) {
    return;
  }

  if (pbl_bt_prf_store_set_pairing(&keys.peer, &keys.our, nullptr) != 0) {
    PBL_LOG_ERR("Could not copy the bond to the PRF store");
  }
}

struct others {
  const ble_addr_t *keep;
  ble_addr_t addrs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
  size_t count;
};

static void prv_collect_others_cb(const struct pbl_bt_bond_info *info, void *user_data) {
  struct others *others = user_data;

  if (others->count < PBL_ARRAY_SIZE(others->addrs) &&
      ble_addr_cmp(&info->addr, others->keep) != 0) {
    others->addrs[others->count++] = info->addr;
  }
}

static void prv_delete_others(const ble_addr_t *keep) {
  struct others others = {
    .keep = keep,
  };

  pbl_bt_foreach_bond(prv_collect_others_cb, &others);

  for (size_t i = 0U; i < others.count; i++) {
    (void)pbl_bt_unpair(&others.addrs[i]);
  }
}

struct first_bond {
  ble_addr_t addr;
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
    prv_delete_others(&keys.peer.peer_addr);
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

int bt_bonding_get_root_keys(struct pbl_bt_sm_key *keys) {
  if (pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION]) &&
      pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                    &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    return 0;
  }

  for (size_t i = 0U; i < PBL_BT_SM_ROOT_KEY_TYPE_NUM; i++) {
    for (size_t j = 0U; j < sizeof(keys[i].data); j += sizeof(uint32_t)) {
      uint32_t word;

      if (rng_rand(&word) != 0) {
        return -EIO;
      }

      memcpy(&keys[i].data[j], &word, sizeof(word));
    }
  }

  // They still serve until the next boot.
  if (pbl_bt_prf_store_set_root_keys(keys) != 0) {
    PBL_LOG_ERR("Could not store the root keys");
  }

  return 0;
}

bool bt_bonding_get_peer_name(const ble_addr_t *addr, char *name) {
  struct pbl_bt_bond_keys stored;

  return prv_get_prf_pairing(&stored, name) && ble_addr_cmp(&stored.peer.peer_addr, addr) == 0 &&
         name[0] != '\0';
}

void bt_bonding_set_peer_name(const ble_addr_t *addr, const char *name) {
  char stored_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  struct pbl_bt_bond_keys stored;

  if (!prv_get_prf_pairing(&stored, stored_name) ||
      ble_addr_cmp(&stored.peer.peer_addr, addr) != 0 ||
      strncmp(stored_name, name, sizeof(stored_name)) == 0) {
    return;
  }

  (void)pbl_bt_prf_store_set_pairing_name(name);
}

void pbl_bt_cb_bond_added(const struct pbl_bt_bond_info *info) {
  prv_delete_others(&info->addr);
  prv_copy_to_prf_store(&info->addr);

  // Only one bond is kept, so a new one means a new phone.
  bt_settings_set_unfaithful(true);

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  bt_link_handle_bond_added();
  bt_advertising_set_reconnectable(true);
  bt_pairability_update_due_to_bonding_change();
}

void pbl_bt_cb_bond_changed(const struct pbl_bt_bond_info *info) {
  prv_copy_to_prf_store(&info->addr);
}

void pbl_bt_cb_bond_deleted(const ble_addr_t *addr) {
  struct pbl_bt_bond_keys stored;

  // The peer paired again since.
  if (pbl_bt_bond_get(addr, nullptr) == 0) {
    return;
  }

  if (prv_get_prf_pairing(&stored, nullptr) && ble_addr_cmp(&stored.peer.peer_addr, addr) == 0) {
    (void)pbl_bt_prf_store_erase_pairing();
  }

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  bt_link_handle_bond_deleted();
  bt_advertising_set_reconnectable(pbl_bt_bond_count() > 0);
  bt_pairability_update_due_to_bonding_change();
}
