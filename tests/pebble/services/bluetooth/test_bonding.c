/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/prf_store.h>
#include <pbl/drivers/rng.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/bluetooth/gateway.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_spi_flash.h>

#define MAX_BONDS 4

static struct pbl_bt_bond_keys s_bonds[MAX_BONDS];
static size_t s_num_bonds;
static int s_unfaithful_count;

static int prv_find(const ble_addr_t *addr) {
  for (size_t i = 0; i < s_num_bonds; i++) {
    if (ble_addr_cmp(&s_bonds[i].peer.peer_addr, addr) == 0) {
      return (int)i;
    }
  }

  return -ENOENT;
}

void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data) {
  for (size_t i = 0; i < s_num_bonds; i++) {
    const struct pbl_bt_bond_info info = {
      .addr = s_bonds[i].peer.peer_addr,
    };

    func(&info, user_data);
  }
}

size_t pbl_bt_bond_count(void) {
  return s_num_bonds;
}

int pbl_bt_bond_get(const ble_addr_t *addr, struct pbl_bt_bond_info *info) {
  return prv_find(addr) < 0 ? -ENOENT : 0;
}

int pbl_bt_bond_get_keys(const ble_addr_t *addr, struct pbl_bt_bond_keys *keys) {
  const int idx = prv_find(addr);

  if (idx < 0) {
    return idx;
  }

  *keys = s_bonds[idx];

  return 0;
}

int pbl_bt_bond_set_keys(const struct pbl_bt_bond_keys *keys) {
  int idx = prv_find(&keys->peer.peer_addr);

  if (idx < 0) {
    cl_assert(s_num_bonds < MAX_BONDS);
    idx = (int)s_num_bonds++;
  }

  s_bonds[idx] = *keys;

  return 0;
}

int pbl_bt_unpair(const ble_addr_t *addr) {
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

void bt_link_handle_bond_deleted(void) {
}

void bt_link_handle_bond_added(void) {
}

void bt_advertising_set_reconnectable(bool reconnectable) {
}

void bt_pairability_update_due_to_bonding_change(void) {
}

static struct pbl_bt_bond_keys prv_keys(uint8_t id) {
  struct pbl_bt_bond_keys keys = {
    .peer =
        {
          .peer_addr.val = {id, 2, 3, 4, 5, 6},
          .key_size = 16,
          .ediv = id,
          .rand_num = 0x1000U + id,
          .ltk_present = 1,
          .irk_present = 1,
          .sc = 1,
        },
    .our = {
      .peer_addr.val = {id, 2, 3, 4, 5, 6},
      .key_size = 16,
      .ediv = (uint16_t)(id + 1U),
      .rand_num = 0x2000U + id,
      .ltk_present = 1,
      .sc = 1,
    },
  };

  memset(keys.peer.ltk, id, sizeof(keys.peer.ltk));
  memset(keys.our.ltk, id + 1, sizeof(keys.our.ltk));
  memset(keys.peer.irk, id + 2, sizeof(keys.peer.irk));

  return keys;
}

static void prv_add(const struct pbl_bt_bond_keys *keys) {
  const struct pbl_bt_bond_info info = {
    .addr = keys->peer.peer_addr,
  };

  pbl_bt_bond_set_keys(keys);
  pbl_bt_cb_bond_added(&info);
}

static void prv_assert_prf_pairing(const struct pbl_bt_bond_keys *keys) {
  struct ble_store_value_sec peer;
  struct ble_store_value_sec our;

  cl_assert(pbl_bt_prf_store_get_pairing(&peer, &our, nullptr));
  cl_assert_equal_i(ble_addr_cmp(&peer.peer_addr, &keys->peer.peer_addr), 0);
  cl_assert_equal_m(peer.ltk, keys->peer.ltk, sizeof(peer.ltk));
  cl_assert_equal_m(peer.irk, keys->peer.irk, sizeof(peer.irk));
  cl_assert_equal_m(our.ltk, keys->our.ltk, sizeof(our.ltk));
  cl_assert_equal_i(peer.sc, keys->peer.sc);
  cl_assert_equal_i(peer.authenticated, keys->peer.authenticated);
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
  pbl_bt_prf_store_set_pairing(&prf.peer, &prf.our, "Phone");

  bt_bonding_init();

  cl_assert_equal_i(s_num_bonds, 1);
  cl_assert_equal_i(pbl_bt_bond_get_keys(&prf.peer.peer_addr, &keys), 0);
  cl_assert_equal_m(keys.peer.ltk, prf.peer.ltk, sizeof(keys.peer.ltk));
  cl_assert_equal_m(keys.our.ltk, prf.our.ltk, sizeof(keys.our.ltk));
  cl_assert_equal_i(keys.peer.sc, prf.peer.sc);
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

  bt_bonding_set_peer_name(&first.peer.peer_addr, "First");
  cl_assert(bt_bonding_get_peer_name(&first.peer.peer_addr, name));
  cl_assert_equal_s(name, "First");

  prv_add(&second);

  cl_assert_equal_i(s_num_bonds, 1);
  cl_assert_equal_i(pbl_bt_bond_get(&second.peer.peer_addr, nullptr), 0);
  prv_assert_prf_pairing(&second);
  cl_assert_equal_i(s_unfaithful_count, 2);
  cl_assert(!bt_bonding_get_peer_name(&second.peer.peer_addr, name));
  cl_assert(!bt_bonding_get_peer_name(&first.peer.peer_addr, name));

  // The first bond's deletion is reported after the second is there.
  pbl_bt_cb_bond_deleted(&first.peer.peer_addr);
  prv_assert_prf_pairing(&second);
}

void test_bonding__changed_keys_are_copied(void) {
  struct pbl_bt_bond_keys bond = prv_keys(1);
  const struct pbl_bt_bond_info info = {
    .addr = bond.peer.peer_addr,
  };
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  prv_add(&bond);
  bt_bonding_set_peer_name(&bond.peer.peer_addr, "Phone");

  memset(bond.peer.ltk, 0x55, sizeof(bond.peer.ltk));
  pbl_bt_bond_set_keys(&bond);
  pbl_bt_cb_bond_changed(&info);

  prv_assert_prf_pairing(&bond);
  cl_assert(bt_bonding_get_peer_name(&bond.peer.peer_addr, name));
  cl_assert_equal_s(name, "Phone");
}

void test_bonding__deleted_bond_leaves_the_prf_store(void) {
  const struct pbl_bt_bond_keys bond = prv_keys(1);

  prv_add(&bond);
  pbl_bt_unpair(&bond.peer.peer_addr);
  pbl_bt_cb_bond_deleted(&bond.peer.peer_addr);

  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

void test_bonding__peer_paired_again_keeps_the_prf_pairing(void) {
  const struct pbl_bt_bond_keys bond = prv_keys(1);

  prv_add(&bond);

  // Deleted and added again before the deletion is reported.
  pbl_bt_cb_bond_deleted(&bond.peer.peer_addr);

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
