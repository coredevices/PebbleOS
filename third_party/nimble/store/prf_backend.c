/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/prf_store.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <syscfg/syscfg.h>

// NimBLE's private header, which needs the ones above.
// clang-format off
#include "ble_store_config_priv.h"
// clang-format on

/* The PRF store keeps one bonding: the one written last. */
static struct ble_store_value_sec s_seen_our_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
static struct ble_store_value_sec s_seen_peer_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
static int s_num_seen_our_secs;
static int s_num_seen_peer_secs;
static ble_addr_t s_last_addr;

static bool prv_sec_equal(const struct ble_store_value_sec *a,
                          const struct ble_store_value_sec *b) {
  return ble_addr_cmp(&a->peer_addr, &b->peer_addr) == 0 && a->key_size == b->key_size &&
         a->ediv == b->ediv && a->rand_num == b->rand_num && a->ltk_present == b->ltk_present &&
         memcmp(a->ltk, b->ltk, sizeof(a->ltk)) == 0 && a->irk_present == b->irk_present &&
         memcmp(a->irk, b->irk, sizeof(a->irk)) == 0 && a->authenticated == b->authenticated &&
         a->sc == b->sc;
}

/* Note the peer of a value that was not there the last time, if any. */
static void prv_note_written(const struct ble_store_value_sec *secs, int num,
                             struct ble_store_value_sec *seen, int *num_seen) {
  for (int i = 0; i < num; i++) {
    bool found = false;

    for (int j = 0; j < *num_seen && !found; j++) {
      found = prv_sec_equal(&secs[i], &seen[j]);
    }
    if (!found) {
      s_last_addr = secs[i].peer_addr;
    }
  }

  memcpy(seen, secs, (size_t)num * sizeof(*secs));
  *num_seen = num;
}

static const struct ble_store_value_sec *prv_find(const struct ble_store_value_sec *secs, int num) {
  for (int i = 0; i < num; i++) {
    if (ble_addr_cmp(&secs[i].peer_addr, &s_last_addr) == 0) {
      return &secs[i];
    }
  }

  return nullptr;
}

static int prv_save(void) {
  const struct ble_store_value_sec *peer;
  const struct ble_store_value_sec *our;
  int ret;

  prv_note_written(ble_store_config_our_secs, ble_store_config_num_our_secs, s_seen_our_secs,
                   &s_num_seen_our_secs);
  prv_note_written(ble_store_config_peer_secs, ble_store_config_num_peer_secs, s_seen_peer_secs,
                   &s_num_seen_peer_secs);

  peer = prv_find(ble_store_config_peer_secs, ble_store_config_num_peer_secs);
  our = prv_find(ble_store_config_our_secs, ble_store_config_num_our_secs);
  if (peer == nullptr) {
    // Gone, or its keys are still to come.
    if (our != nullptr) {
      return 0;
    }

    ret = pbl_bt_prf_store_erase_pairing();
  } else {
    ret = pbl_bt_prf_store_set_pairing(peer, our, nullptr);
  }

  return (ret == 0) ? 0 : BLE_HS_ESTORE_FAIL;
}

int ble_store_config_persist_our_secs(void) {
  return prv_save();
}

int ble_store_config_persist_peer_secs(void) {
  return prv_save();
}

int ble_store_config_persist_cccds(void) {
  return 0;
}

void ble_store_config_conf_init(void) {
  struct ble_store_value_sec *peer = &ble_store_config_peer_secs[0];
  struct ble_store_value_sec *our = &ble_store_config_our_secs[0];

  pbl_bt_prf_store_init();

  if (!pbl_bt_prf_store_get_pairing(peer, our, nullptr)) {
    return;
  }

  ble_store_config_num_peer_secs = 1;
  s_seen_peer_secs[0] = *peer;
  s_num_seen_peer_secs = 1;
  s_last_addr = peer->peer_addr;

  if (!our->ltk_present) {
    return;
  }

  ble_store_config_num_our_secs = 1;
  s_seen_our_secs[0] = *our;
  s_num_seen_our_secs = 1;
}
