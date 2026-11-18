/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include "nimble_type_conversions.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <kernel/event_loop.h>
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
    .irk_valid = value_sec->irk_present,
  };

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &info->addr);
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

  if (ctx->info.irk_valid) {
    struct pbl_bt_irk_change irk_change = {
      .device = ctx->info.addr,
      .irk = ctx->info.irk,
      .irk_valid = true,
    };

    pbl_bt_handle_le_connection_handle_update_irk(&irk_change);
  }

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
    PBL_LOG_INFO("Bonded with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(ctx->info.addr.address));
  }
  launcher_task_add_callback(prv_sec_written_cb, ctx);

  return 0;
}

static void prv_bond_deleted_cb(void *data) {
  struct pbl_bt_device_internal *addr = data;

  pbl_bt_cb_bond_deleted(addr);

  pbl_free(addr);
}

static int prv_store_delete(int obj_type, const union ble_store_key *key) {
  struct pbl_bt_device_internal *addr;
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
  nimble_addr_to_pebble_device(&value.sec.peer_addr, addr);
  PBL_LOG_INFO("Deleted bond with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(addr->address));
  launcher_task_add_callback(prv_bond_deleted_cb, addr);

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

static int prv_read_peer_sec(const struct pbl_bt_device_internal *addr, uint8_t idx,
                             struct ble_store_value_sec *value_sec) {
  union ble_store_key key = {};
  union ble_store_value value;
  int rc;

  if (addr != nullptr) {
    pebble_device_to_nimble_addr(addr, &key.sec.peer_addr);
  } else {
    key.sec.peer_addr = *BLE_ADDR_ANY;
  }
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

int pbl_bt_bond_get(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_info *info) {
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

int pbl_bt_bond_get_keys(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_keys *keys) {
  struct pbl_bt_sm_pairing_info *info = &keys->info;
  struct ble_store_value_sec peer;
  union ble_store_value our;
  union ble_store_key key;
  int ret;

  ret = prv_read_peer_sec(addr, 0U, &peer);
  if (ret != 0) {
    return ret;
  }

  memset(keys, 0, sizeof(*keys));
  nimble_addr_to_pebble_device(&peer.peer_addr, &info->identity);

  if (peer.ltk_present) {
    info->is_remote_encryption_info_valid = true;
    info->remote_encryption_info.ediv = peer.ediv;
    info->remote_encryption_info.rand = peer.rand_num;
    memcpy(info->remote_encryption_info.ltk.data, peer.ltk, KEY_SIZE);
  }

  if (peer.irk_present) {
    info->is_remote_identity_info_valid = true;
    memcpy(info->irk.data, peer.irk, KEY_SIZE);
  }

  ble_store_key_from_value_sec(&key.sec, &peer);
  if (prv_read(BLE_STORE_OBJ_TYPE_OUR_SEC, &key, &our) == 0 && our.sec.ltk_present) {
    info->is_local_encryption_info_valid = true;
    info->local_encryption_info.ediv = our.sec.ediv;
    info->local_encryption_info.rand = our.sec.rand_num;
    memcpy(info->local_encryption_info.ltk.data, our.sec.ltk, KEY_SIZE);
  }

  if (peer.sc) {
    keys->flags |= PBL_BT_BOND_FLAG_SECURE_CONNECTIONS;
  }
  if (peer.authenticated) {
    keys->flags |= PBL_BT_BOND_FLAG_AUTHENTICATED;
  }

  return 0;
}

static void prv_keys_to_value_sec(const struct pbl_bt_bond_keys *keys, bool local,
                                  struct ble_store_value_sec *value_sec) {
  const struct pbl_bt_sm_pairing_info *info = &keys->info;

  memset(value_sec, 0, sizeof(*value_sec));
  value_sec->key_size = KEY_SIZE;
  value_sec->sc = (keys->flags & PBL_BT_BOND_FLAG_SECURE_CONNECTIONS) != 0U;
  value_sec->authenticated = (keys->flags & PBL_BT_BOND_FLAG_AUTHENTICATED) != 0U;
  pebble_device_to_nimble_addr(&info->identity, &value_sec->peer_addr);

  if (local && info->is_local_encryption_info_valid) {
    value_sec->ediv = info->local_encryption_info.ediv;
    value_sec->rand_num = info->local_encryption_info.rand;
    value_sec->ltk_present = 1;
    memcpy(value_sec->ltk, info->local_encryption_info.ltk.data, KEY_SIZE);
  }

  if (!local && info->is_remote_encryption_info_valid) {
    value_sec->ediv = info->remote_encryption_info.ediv;
    value_sec->rand_num = info->remote_encryption_info.rand;
    value_sec->ltk_present = 1;
    memcpy(value_sec->ltk, info->remote_encryption_info.ltk.data, KEY_SIZE);
  }

  if (!local && info->is_remote_identity_info_valid) {
    value_sec->irk_present = 1;
    memcpy(value_sec->irk, info->irk.data, KEY_SIZE);
  }
}

int pbl_bt_bond_set_keys(const struct pbl_bt_bond_keys *keys) {
  union ble_store_value value;

  prv_keys_to_value_sec(keys, false, &value.sec);
  if (prv_write_if_changed(BLE_STORE_OBJ_TYPE_PEER_SEC, &value) != 0) {
    return -EIO;
  }

  prv_keys_to_value_sec(keys, true, &value.sec);
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

int pbl_bt_unpair(const struct pbl_bt_device_internal *addr) {
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
