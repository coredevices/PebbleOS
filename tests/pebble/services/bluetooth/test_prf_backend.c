/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/prf_store.h>

#include <clar.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <syscfg/syscfg.h>

// NimBLE's private header, which needs the ones above.
// clang-format off
#include "ble_store_config_priv.h"
// clang-format on

void ble_store_config_conf_init(void);

/* What NimBLE's config store holds in RAM. */
struct ble_store_value_sec ble_store_config_our_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
int ble_store_config_num_our_secs;
struct ble_store_value_sec ble_store_config_peer_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
int ble_store_config_num_peer_secs;
struct ble_hs_cfg ble_hs_cfg;

/* The PRF store. */
static bool s_has_pairing;
static struct ble_store_value_sec s_peer;
static struct ble_store_value_sec s_our;
static int s_num_sets;
static int s_num_erases;
static int s_store_rc;

void pbl_bt_prf_store_init(void) {
}

bool pbl_bt_prf_store_get_pairing(struct ble_store_value_sec *peer, struct ble_store_value_sec *our,
                                  char *name) {
  if (s_has_pairing) {
    *peer = s_peer;
    *our = s_our;
  }

  return s_has_pairing;
}

int pbl_bt_prf_store_set_pairing(const struct ble_store_value_sec *peer,
                                 const struct ble_store_value_sec *our, const char *name) {
  cl_assert(name == nullptr);
  s_num_sets++;
  if (s_store_rc != 0) {
    return s_store_rc;
  }

  s_has_pairing = true;
  s_peer = *peer;
  memset(&s_our, 0, sizeof(s_our));
  if (our != nullptr) {
    s_our = *our;
  }

  return 0;
}

int pbl_bt_prf_store_erase_pairing(void) {
  s_num_erases++;
  if (s_store_rc != 0) {
    return s_store_rc;
  }

  s_has_pairing = false;

  return 0;
}

static struct ble_store_value_sec prv_sec(uint8_t peer, uint8_t key) {
  struct ble_store_value_sec sec = {
    .peer_addr = {.type = BLE_ADDR_PUBLIC, .val = {peer}},
    .key_size = 16,
    .ltk_present = 1,
    .sc = 1,
  };

  memset(sec.ltk, key, sizeof(sec.ltk));

  return sec;
}

/* NimBLE writes the keys of the watch, then those of the peer. */
static void prv_bond(uint8_t peer, uint8_t key) {
  ble_store_config_our_secs[ble_store_config_num_our_secs++] = prv_sec(peer, key);
  cl_assert_equal_i(ble_store_config_persist_our_secs(), 0);
  ble_store_config_peer_secs[ble_store_config_num_peer_secs++] = prv_sec(peer, key + 1);
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), 0);
}

static void prv_assert_stored(uint8_t peer, uint8_t key) {
  cl_assert(s_has_pairing);
  cl_assert_equal_i(s_peer.peer_addr.val[0], peer);
  cl_assert_equal_i(s_peer.ltk[0], key + 1);
  cl_assert_equal_i(s_our.ltk[0], key);
}

void test_prf_backend__initialize(void) {
  s_has_pairing = false;
  s_num_sets = 0;
  s_num_erases = 0;
  s_store_rc = 0;
  ble_store_config_num_our_secs = 0;
  ble_store_config_num_peer_secs = 0;
  ble_store_config_conf_init();
}

void test_prf_backend__loads_the_stored_bond(void) {
  s_has_pairing = true;
  s_peer = prv_sec(7, 0x21);
  s_our = prv_sec(7, 0x20);

  ble_store_config_conf_init();

  cl_assert_equal_i(ble_store_config_num_peer_secs, 1);
  cl_assert_equal_i(ble_store_config_num_our_secs, 1);
  cl_assert_equal_i(ble_store_config_peer_secs[0].peer_addr.val[0], 7);
  cl_assert_equal_i(ble_store_config_our_secs[0].ltk[0], 0x20);
}

void test_prf_backend__stores_a_bond_once_the_peer_keys_are_there(void) {
  ble_store_config_our_secs[ble_store_config_num_our_secs++] = prv_sec(1, 0x10);
  cl_assert_equal_i(ble_store_config_persist_our_secs(), 0);
  cl_assert_equal_i(s_num_sets, 0);
  cl_assert_equal_i(s_num_erases, 0);

  ble_store_config_peer_secs[ble_store_config_num_peer_secs++] = prv_sec(1, 0x11);
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), 0);
  prv_assert_stored(1, 0x10);
}

void test_prf_backend__keeps_the_bond_written_last(void) {
  prv_bond(1, 0x10);
  prv_bond(2, 0x30);
  prv_assert_stored(2, 0x30);

  /* Writing what is there again is not writing the first one last. */
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), 0);
  prv_assert_stored(2, 0x30);

  /* New keys for the first one are. */
  ble_store_config_our_secs[0] = prv_sec(1, 0x50);
  cl_assert_equal_i(ble_store_config_persist_our_secs(), 0);
  ble_store_config_peer_secs[0] = prv_sec(1, 0x51);
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), 0);
  prv_assert_stored(1, 0x50);
}

void test_prf_backend__erases_the_bond_when_it_goes(void) {
  prv_bond(1, 0x10);

  ble_store_config_num_our_secs = 0;
  cl_assert_equal_i(ble_store_config_persist_our_secs(), 0);
  cl_assert(s_has_pairing);

  ble_store_config_num_peer_secs = 0;
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), 0);
  cl_assert(!s_has_pairing);
}

void test_prf_backend__reports_a_store_that_fails(void) {
  ble_store_config_our_secs[ble_store_config_num_our_secs++] = prv_sec(1, 0x10);
  cl_assert_equal_i(ble_store_config_persist_our_secs(), 0);

  s_store_rc = -EIO;
  ble_store_config_peer_secs[ble_store_config_num_peer_secs++] = prv_sec(1, 0x11);
  cl_assert_equal_i(ble_store_config_persist_peer_secs(), BLE_HS_ESTORE_FAIL);
}
