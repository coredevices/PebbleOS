/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <nimble/nimble_port_pebble.h>
#include <store/config/ble_store_config.h>
#include <syscfg/syscfg.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define KEY_SIZE 16

#define MAX_BONDS MYNEWT_VAL(BLE_STORE_MAX_BONDS)

static struct pbl_bt_sm_key s_local_irk;

// NimBLE's store is not thread-safe, and the firmware reads and deletes bonds from its own
// threads.
static PBL_MUTEX_DEFINE(s_lock);

static int prv_read(int obj_type, const union ble_store_key *key, union ble_store_value *value) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_read(obj_type, key, value);
  pbl_mutex_unlock(&s_lock);

  return rc;
}

static int prv_write(int obj_type, const union ble_store_value *value) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_write(obj_type, value);
  pbl_mutex_unlock(&s_lock);

  return rc;
}

static int prv_delete(int obj_type, const union ble_store_key *key) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_delete(obj_type, key);
  pbl_mutex_unlock(&s_lock);

  return rc;
}

static bool prv_sec_equal(const struct ble_store_value_sec *a,
                          const struct ble_store_value_sec *b) {
  return ble_addr_cmp(&a->peer_addr, &b->peer_addr) == 0 && a->key_size == b->key_size &&
         a->ediv == b->ediv && a->rand_num == b->rand_num && a->ltk_present == b->ltk_present &&
         memcmp(a->ltk, b->ltk, sizeof(a->ltk)) == 0 && a->irk_present == b->irk_present &&
         memcmp(a->irk, b->irk, sizeof(a->irk)) == 0 && a->csrk_present == b->csrk_present &&
         a->authenticated == b->authenticated && a->sc == b->sc;
}

static int prv_write_if_changed(int obj_type, const union ble_store_value *value) {
  union ble_store_key key;
  union ble_store_value stored;
  int rc;

  ble_store_key_from_value_sec(&key.sec, &value->sec);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_read(obj_type, &key, &stored);
  if (rc != 0 || !prv_sec_equal(&stored.sec, &value->sec)) {
    rc = ble_store_config_write(obj_type, value);
  }
  pbl_mutex_unlock(&s_lock);

  return rc;
}

static void prv_value_to_info(const struct ble_store_value_sec *value_sec,
                              struct pbl_bt_bond_info *info) {
  *info = (struct pbl_bt_bond_info){
    .addr = value_sec->peer_addr,
    .irk_valid = value_sec->irk_present,
  };

  if (value_sec->irk_present) {
    memcpy(info->irk.data, value_sec->irk, KEY_SIZE);
  }
}

struct sec_written {
  struct pbl_bt_bond_info info;
  bool is_new;
};

static void prv_sec_written_cb(void *data) {
  struct sec_written *ctx = data;

  if (ctx->is_new) {
    pbl_bt_cb_bond_added(&ctx->info);
  } else {
    pbl_bt_cb_bond_changed(&ctx->info);
  }

  pbl_free(ctx);
}

static bool prv_is_sec(int obj_type) {
  return obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC || obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC;
}

// A bond is there once the store has the peer's keys.
static int prv_store_write(int obj_type, const union ble_store_value *value) {
  union ble_store_value peer;
  union ble_store_key key;
  struct sec_written *ctx;
  bool is_new;
  int rc;

  if (!prv_is_sec(obj_type)) {
    return prv_write(obj_type, value);
  }

  if (value->sec.key_size != KEY_SIZE || value->sec.csrk_present) {
    PBL_LOG_ERR("Unsupported security parameters, dropping bonding keys");
    return BLE_HS_ENOTSUP;
  }

  ble_store_key_from_value_sec(&key.sec, &value->sec);
  is_new = prv_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &peer) != 0;

  rc = prv_write(obj_type, value);
  if (rc != 0) {
    return rc;
  }

  if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
    peer.sec = value->sec;
  } else if (is_new) {
    return 0;
  }

  ctx = pbl_malloc_check(sizeof(*ctx));
  ctx->is_new = is_new;
  prv_value_to_info(&peer.sec, &ctx->info);
  if (is_new) {
    PBL_LOG_INFO("Bonded with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE_VAL(ctx->info.addr.val));
  }
  (void)pbl_sys_work_call(prv_sec_written_cb, ctx);

  return 0;
}

static void prv_bond_deleted_cb(void *data) {
  ble_addr_t *addr = data;

  pbl_bt_cb_bond_deleted(addr);

  pbl_free(addr);
}

static int prv_store_delete(int obj_type, const union ble_store_key *key) {
  ble_addr_t *addr;
  union ble_store_value value;
  int rc;

  // The key may not name the peer.
  if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
    rc = prv_read(obj_type, key, &value);
    if (rc != 0) {
      return rc;
    }
  }

  rc = prv_delete(obj_type, key);
  if (rc != 0 || obj_type != BLE_STORE_OBJ_TYPE_PEER_SEC) {
    return rc;
  }

  addr = pbl_malloc_check(sizeof(*addr));
  *addr = value.sec.peer_addr;
  PBL_LOG_INFO("Deleted bond with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE_VAL(addr->val));
  (void)pbl_sys_work_call(prv_bond_deleted_cb, addr);

  return 0;
}

// The local IRK is the firmware's identity root key, rather than one of the store's own. LTKs
// and CSRKs are generated by NimBLE.
static int prv_gen_key(uint8_t key, struct ble_store_gen_key *gen_key, uint16_t conn_handle) {
  static const struct pbl_bt_sm_key s_no_key;

  if (key != BLE_STORE_GEN_KEY_IRK || memcmp(&s_local_irk, &s_no_key, sizeof(s_no_key)) == 0) {
    return BLE_HS_ENOTSUP;
  }

  memcpy(gen_key->irk, s_local_irk.data, KEY_SIZE);

  return 0;
}

void pbl_bt__store_init(void) {
  ble_store_config_init();

  ble_hs_cfg.store_gen_key_cb = prv_gen_key;
  ble_hs_cfg.store_read_cb = prv_read;
  ble_hs_cfg.store_write_cb = prv_store_write;
  ble_hs_cfg.store_delete_cb = prv_store_delete;
  // A new bond replaces the oldest one when the store is full.
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
}

void pbl_bt__store_set_local_irk(const struct pbl_bt_sm_key *irk) {
  s_local_irk = *irk;
}

static int prv_read_peer_sec(const ble_addr_t *addr, uint8_t idx,
                             struct ble_store_value_sec *value_sec) {
  union ble_store_key key = {};
  union ble_store_value value;
  int rc;

  key.sec.peer_addr = (addr != nullptr) ? *addr : *BLE_ADDR_ANY;
  key.sec.idx = idx;

  rc = prv_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &value);
  if (rc != 0) {
    return -ENOENT;
  }

  *value_sec = value.sec;

  return 0;
}

void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data) {
  struct ble_store_value_sec value_sec;
  struct pbl_bt_bond_info info;

  for (uint8_t idx = 0U; prv_read_peer_sec(nullptr, idx, &value_sec) == 0; idx++) {
    prv_value_to_info(&value_sec, &info);
    func(&info, user_data);
  }
}

size_t pbl_bt_bond_count(void) {
  struct ble_store_value_sec value_sec;
  size_t count = 0U;

  while (prv_read_peer_sec(nullptr, (uint8_t)count, &value_sec) == 0) {
    count++;
  }

  return count;
}

int pbl_bt_bond_get(const ble_addr_t *addr, struct pbl_bt_bond_info *info) {
  struct ble_store_value_sec value_sec;
  int ret;

  ret = prv_read_peer_sec(addr, 0U, &value_sec);
  if (ret != 0) {
    return ret;
  }

  if (info != nullptr) {
    prv_value_to_info(&value_sec, info);
  }

  return 0;
}

int pbl_bt_bond_get_keys(const ble_addr_t *addr, struct pbl_bt_bond_keys *keys) {
  union ble_store_value our;
  union ble_store_key key;
  int ret;

  memset(keys, 0, sizeof(*keys));

  ret = prv_read_peer_sec(addr, 0U, &keys->peer);
  if (ret != 0) {
    return ret;
  }

  ble_store_key_from_value_sec(&key.sec, &keys->peer);
  if (prv_read(BLE_STORE_OBJ_TYPE_OUR_SEC, &key, &our) == 0) {
    keys->our = our.sec;
  }

  return 0;
}

int pbl_bt_bond_set_keys(const struct pbl_bt_bond_keys *keys) {
  union ble_store_value value;

  value.sec = keys->peer;
  if (prv_write_if_changed(BLE_STORE_OBJ_TYPE_PEER_SEC, &value) != 0) {
    return -EIO;
  }

  if (!keys->our.ltk_present) {
    return 0;
  }

  value.sec = keys->our;
  if (prv_write_if_changed(BLE_STORE_OBJ_TYPE_OUR_SEC, &value) != 0) {
    return -EIO;
  }

  return 0;
}

// Not ble_gap_unpair(): it also edits the resolving list, which preempts advertising.
static int prv_unpair(const ble_addr_t *peer_addr) {
  int rc;

  rc = ble_store_util_delete_peer(peer_addr);
  if (rc != 0) {
    PBL_LOG_ERR("Could not unpair: 0x%x", (unsigned int)rc);
    return -EIO;
  }

  return 0;
}

int pbl_bt_unpair(const ble_addr_t *addr) {
  struct ble_store_value_sec value_sec;
  ble_addr_t peers[MAX_BONDS];
  size_t count = 0U;
  int ret = 0;

  if (addr != nullptr) {
    ret = prv_read_peer_sec(addr, 0U, &value_sec);
    if (ret != 0) {
      return ret;
    }

    return prv_unpair(&value_sec.peer_addr);
  }

  while (count < MAX_BONDS && prv_read_peer_sec(nullptr, (uint8_t)count, &value_sec) == 0) {
    peers[count++] = value_sec.peer_addr;
  }

  for (size_t i = 0U; i < count; i++) {
    int rc = prv_unpair(&peers[i]);

    if (rc != 0) {
      ret = rc;
    }
  }

  return ret;
}
