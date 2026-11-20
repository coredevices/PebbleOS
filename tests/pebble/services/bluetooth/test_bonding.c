/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/drivers/rng.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/ble/kernel_le_client/kernel_le_client.h>
#include <fake_spi_flash.h>

#define MAX_BONDS 4

static struct pbl_bt_bond_keys s_bonds[MAX_BONDS];
static size_t s_num_bonds;
static int s_unfaithful_count;

static int prv_find(const struct pbl_bt_device_internal *addr) {
  for (size_t i = 0; i < s_num_bonds; i++) {
    if (bt_device_internal_equal(&s_bonds[i].info.identity, addr)) {
      return (int)i;
    }
  }

  return -ENOENT;
}

void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data) {
  for (size_t i = 0; i < s_num_bonds; i++) {
    const struct pbl_bt_bond_info info = {
      .addr = s_bonds[i].info.identity,
    };

    func(&info, user_data);
  }
}

size_t pbl_bt_bond_count(void) {
  return s_num_bonds;
}

int pbl_bt_bond_get(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_info *info) {
  return prv_find(addr) < 0 ? -ENOENT : 0;
}

int pbl_bt_bond_get_keys(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_keys *keys) {
  const int idx = prv_find(addr);

  if (idx < 0) {
    return idx;
  }

  *keys = s_bonds[idx];

  return 0;
}

int pbl_bt_bond_set_keys(const struct pbl_bt_bond_keys *keys) {
  int idx = prv_find(&keys->info.identity);

  if (idx < 0) {
    cl_assert(s_num_bonds < MAX_BONDS);
    idx = (int)s_num_bonds++;
  }

  s_bonds[idx] = *keys;

  return 0;
}

int pbl_bt_unpair(const struct pbl_bt_device_internal *addr) {
  const int idx = prv_find(addr);

  if (idx < 0) {
    return idx;
  }

  s_bonds[idx] = s_bonds[--s_num_bonds];

  return 0;
}

int rng_rand(uint32_t *rand_out) {
  static uint32_t s_next = 0x1234;

  *rand_out = s_next++;

  return 0;
}

void bt_settings_set_unfaithful(bool is_unfaithful) {
  cl_assert(is_unfaithful);
  s_unfaithful_count++;
}

bool bt_ctl_is_bluetooth_running(void) {
  return false;
}

void bt_lock(void) {
}

void bt_unlock(void) {
}

void gap_le_connection_handle_bond_deleted(const struct pbl_bt_device_internal *addr) {
}

void gap_le_connect_handle_bond_deleted(const struct pbl_bt_device_internal *addr) {
}

void gap_le_connect_handle_bond_added(const struct pbl_bt_bond_info *info) {
}

void bt_advertising_set_reconnectable(bool reconnectable) {
}

void bt_pairability_update_due_to_bonding_change(void) {
}

static struct pbl_bt_bond_keys prv_keys(uint8_t id) {
  struct pbl_bt_bond_keys keys = {
    .info =
        {
          .identity.address.octets = {id, 2, 3, 4, 5, 6},
          .is_remote_encryption_info_valid = true,
          .remote_encryption_info =
              {
                .ediv = id,
                .rand = 0x1000U + id,
              },
          .is_local_encryption_info_valid = true,
          .local_encryption_info =
              {
                .ediv = (uint16_t)(id + 1U),
                .rand = 0x2000U + id,
              },
          .is_remote_identity_info_valid = true,
        },
    .flags = PBL_BT_BOND_FLAG_SECURE_CONNECTIONS,
  };

  memset(keys.info.remote_encryption_info.ltk.data, id, sizeof(struct pbl_bt_sm_key));
  memset(keys.info.local_encryption_info.ltk.data, id + 1, sizeof(struct pbl_bt_sm_key));
  memset(keys.info.irk.data, id + 2, sizeof(struct pbl_bt_sm_key));

  return keys;
}

static void prv_add(const struct pbl_bt_bond_keys *keys) {
  const struct pbl_bt_bond_info info = {
    .addr = keys->info.identity,
  };

  pbl_bt_bond_set_keys(keys);
  pbl_bt_cb_bond_added(&info);
}

static void prv_assert_prf_pairing(const struct pbl_bt_bond_keys *keys) {
  struct pbl_bt_sm_pairing_info info;
  uint8_t flags;

  cl_assert(pbl_bt_prf_store_get_pairing(&info, nullptr, &flags));
  cl_assert(bt_device_internal_equal(&info.identity, &keys->info.identity));
  cl_assert_equal_m(&info.remote_encryption_info.ltk, &keys->info.remote_encryption_info.ltk,
                    sizeof(struct pbl_bt_sm_key));
  cl_assert_equal_m(&info.local_encryption_info.ltk, &keys->info.local_encryption_info.ltk,
                    sizeof(struct pbl_bt_sm_key));
  cl_assert_equal_i(flags, keys->flags);
}

void test_bonding__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pbl_bt_prf_store_init();
  s_num_bonds = 0;
  s_unfaithful_count = 0;
}

void test_bonding__init_without_bonds(void) {
  bt_bonding_init();

  cl_assert_equal_i(s_num_bonds, 0);
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

void test_bonding__init_takes_the_prf_pairing(void) {
  const struct pbl_bt_bond_keys other = prv_keys(1);
  const struct pbl_bt_bond_keys prf = prv_keys(2);
  struct pbl_bt_bond_keys keys;

  pbl_bt_bond_set_keys(&other);
  pbl_bt_prf_store_set_pairing(&prf.info, "Phone", prf.flags);

  bt_bonding_init();

  cl_assert_equal_i(s_num_bonds, 1);
  cl_assert_equal_i(pbl_bt_bond_get_keys(&prf.info.identity, &keys), 0);
  cl_assert_equal_m(&keys.info.remote_encryption_info.ltk, &prf.info.remote_encryption_info.ltk,
                    sizeof(struct pbl_bt_sm_key));
  cl_assert_equal_i(keys.flags, prf.flags);
}

void test_bonding__init_copies_the_bond_to_the_prf_store(void) {
  const struct pbl_bt_bond_keys bond = prv_keys(1);

  pbl_bt_bond_set_keys(&bond);

  bt_bonding_init();

  cl_assert_equal_i(s_num_bonds, 1);
  prv_assert_prf_pairing(&bond);
}

void test_bonding__new_bond_replaces_the_others(void) {
  const struct pbl_bt_bond_keys first = prv_keys(1);
  const struct pbl_bt_bond_keys second = prv_keys(2);
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  prv_add(&first);
  prv_assert_prf_pairing(&first);
  cl_assert_equal_i(s_unfaithful_count, 1);

  bt_bonding_set_peer_name(&first.info.identity, "First");
  cl_assert(bt_bonding_get_peer_name(&first.info.identity, name));
  cl_assert_equal_s(name, "First");

  prv_add(&second);

  cl_assert_equal_i(s_num_bonds, 1);
  cl_assert_equal_i(pbl_bt_bond_get(&second.info.identity, nullptr), 0);
  prv_assert_prf_pairing(&second);
  cl_assert_equal_i(s_unfaithful_count, 2);
  cl_assert(!bt_bonding_get_peer_name(&second.info.identity, name));
  cl_assert(!bt_bonding_get_peer_name(&first.info.identity, name));

  // The first bond's deletion is reported after the second is there.
  pbl_bt_cb_bond_deleted(&first.info.identity);
  prv_assert_prf_pairing(&second);
}

void test_bonding__changed_keys_are_copied(void) {
  struct pbl_bt_bond_keys bond = prv_keys(1);
  const struct pbl_bt_bond_info info = {
    .addr = bond.info.identity,
  };
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  prv_add(&bond);
  bt_bonding_set_peer_name(&bond.info.identity, "Phone");

  memset(bond.info.remote_encryption_info.ltk.data, 0x55, sizeof(struct pbl_bt_sm_key));
  pbl_bt_bond_set_keys(&bond);
  pbl_bt_cb_bond_changed(&info);

  prv_assert_prf_pairing(&bond);
  cl_assert(bt_bonding_get_peer_name(&bond.info.identity, name));
  cl_assert_equal_s(name, "Phone");
}

void test_bonding__deleted_bond_leaves_the_prf_store(void) {
  const struct pbl_bt_bond_keys bond = prv_keys(1);

  prv_add(&bond);
  pbl_bt_unpair(&bond.info.identity);
  pbl_bt_cb_bond_deleted(&bond.info.identity);

  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

void test_bonding__peer_paired_again_keeps_the_prf_pairing(void) {
  const struct pbl_bt_bond_keys bond = prv_keys(1);

  prv_add(&bond);

  // Deleted and added again before the deletion is reported.
  pbl_bt_cb_bond_deleted(&bond.info.identity);

  prv_assert_prf_pairing(&bond);
}

void test_bonding__root_keys_are_generated_once(void) {
  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  struct pbl_bt_sm_key again[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  const struct pbl_bt_sm_key none = {};

  bt_bonding_get_root_keys(keys);
  cl_assert(memcmp(&keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY], &none, sizeof(none)) != 0);

  bt_bonding_get_root_keys(again);
  cl_assert_equal_m(keys, again, sizeof(keys));
}
