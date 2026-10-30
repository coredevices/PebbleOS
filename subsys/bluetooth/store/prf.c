/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/list.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// Shared PRF storage holds the bond. CCCDs are not persisted.

struct pbl_bt__store_cccd {
  ListNode node;
  struct ble_store_value_cccd value_cccd;
};

struct pbl_bt__store_cccd_find_ctx {
  const struct ble_store_key_cccd *key;
  unsigned int skipped;
};

static struct pbl_bt__store_cccd *s_cccds;

static PBL_MUTEX_DEFINE(s_store_mutex);

static bool prv_find_cccd_cb(ListNode *node, void *data) {
  struct pbl_bt__store_cccd *s = (struct pbl_bt__store_cccd *)node;
  struct pbl_bt__store_cccd_find_ctx *ctx = data;

  if ((ble_addr_cmp(&ctx->key->peer_addr, BLE_ADDR_ANY) != 0) &&
      (ble_addr_cmp(&s->value_cccd.peer_addr, &ctx->key->peer_addr) != 0)) {
    return false;
  }

  if ((ctx->key->chr_val_handle != 0U) &&
      (s->value_cccd.chr_val_handle != ctx->key->chr_val_handle)) {
    return false;
  }

  if (ctx->key->idx > ctx->skipped) {
    ctx->skipped++;
    return false;
  }

  return true;
}

static struct pbl_bt__store_cccd *prv_find_cccd(const struct ble_store_key_cccd *key_cccd) {
  struct pbl_bt__store_cccd_find_ctx ctx = {
    .key = key_cccd,
  };

  return (struct pbl_bt__store_cccd *)list_find((ListNode *)s_cccds, prv_find_cccd_cb, &ctx);
}

static int prv_read_cccd(const struct ble_store_key_cccd *key_cccd,
                         struct ble_store_value_cccd *value_cccd) {
  struct pbl_bt__store_cccd *s;
  int ret = 0;

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);

  s = prv_find_cccd(key_cccd);
  if (s == nullptr) {
    ret = BLE_HS_ENOENT;
  } else {
    *value_cccd = s->value_cccd;
  }

  pbl_mutex_unlock(&s_store_mutex);

  return ret;
}

static int prv_write_cccd(const struct ble_store_value_cccd *value_cccd) {
  struct ble_store_key_cccd key_cccd;
  struct pbl_bt__store_cccd *s;

  ble_store_key_from_value_cccd(&key_cccd, value_cccd);

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);

  s = prv_find_cccd(&key_cccd);
  if (s == nullptr) {
    s = kernel_zalloc_check(sizeof(*s));
    s_cccds = (struct pbl_bt__store_cccd *)list_prepend((ListNode *)s_cccds, (ListNode *)s);
  }
  s->value_cccd = *value_cccd;

  pbl_mutex_unlock(&s_store_mutex);

  return 0;
}

static int prv_delete_cccd(const struct ble_store_key_cccd *key_cccd) {
  struct pbl_bt__store_cccd *s;
  int ret = 0;

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);

  s = prv_find_cccd(key_cccd);
  if (s == nullptr) {
    ret = BLE_HS_ENOENT;
  } else {
    list_remove((ListNode *)s, (ListNode **)&s_cccds, nullptr);
    kernel_free(s);
  }

  pbl_mutex_unlock(&s_store_mutex);

  return ret;
}

static void prv_delete_cccds(const ble_addr_t *peer_addr) {
  struct ble_store_key_cccd key_cccd = {
    .peer_addr = *peer_addr,
  };

  while (prv_delete_cccd(&key_cccd) == 0) {
  }
}

static int prv_read_sec(int obj_type, const struct ble_store_key_sec *key_sec,
                        struct ble_store_value_sec *value_sec) {
  int ret;

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);
  ret = pbl_bt__sprf_read_sec(obj_type, key_sec, value_sec);
  pbl_mutex_unlock(&s_store_mutex);

  return ret;
}

static int prv_write_sec(int obj_type, const struct ble_store_value_sec *value_sec) {
  struct ble_store_key_sec key_sec = {
    .peer_addr = value_sec->peer_addr,
  };
  struct ble_store_value_sec existing;
  ble_addr_t replaced;
  bool is_new_bond;
  int ret;

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);
  is_new_bond = pbl_bt__sprf_read_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &key_sec, &existing) != 0;
  ret = pbl_bt__sprf_write_sec(obj_type, value_sec, &replaced);
  pbl_mutex_unlock(&s_store_mutex);

  if (ret != 0) {
    PBL_LOG_ERR("Could not store the bond: 0x%x", ret);
    return ret;
  }

  if (ble_addr_cmp(&replaced, BLE_ADDR_ANY) != 0) {
    prv_delete_cccds(&replaced);
    pbl_bt__store_bond_deleted(&replaced);
  }

  pbl_bt__store_sec_written(obj_type, value_sec, is_new_bond);

  return 0;
}

static int prv_delete_sec(int obj_type, const struct ble_store_key_sec *key_sec) {
  struct ble_store_value_sec value_sec;
  int ret;

  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);
  ret = pbl_bt__sprf_read_sec(obj_type, key_sec, &value_sec);
  if (ret == 0) {
    ret = pbl_bt__sprf_delete_sec(obj_type, &value_sec.peer_addr);
  }
  pbl_mutex_unlock(&s_store_mutex);

  if (ret == 0 && obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
    pbl_bt__store_bond_deleted(&value_sec.peer_addr);
  }

  return ret;
}

static int prv_store_read(int obj_type, const union ble_store_key *key,
                          union ble_store_value *value) {
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      return prv_read_sec(obj_type, &key->sec, &value->sec);
    case BLE_STORE_OBJ_TYPE_CCCD:
      return prv_read_cccd(&key->cccd, &value->cccd);
    default:
      return BLE_HS_ENOTSUP;
  }
}

static int prv_store_write(int obj_type, const union ble_store_value *val) {
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      return prv_write_sec(obj_type, &val->sec);
    case BLE_STORE_OBJ_TYPE_CCCD:
      return prv_write_cccd(&val->cccd);
    default:
      return BLE_HS_ENOTSUP;
  }
}

static int prv_store_delete(int obj_type, const union ble_store_key *key) {
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      return prv_delete_sec(obj_type, &key->sec);
    case BLE_STORE_OBJ_TYPE_CCCD:
      return prv_delete_cccd(&key->cccd);
    default:
      return BLE_HS_ENOTSUP;
  }
}

static int prv_store_gen_key(uint8_t key, struct ble_store_gen_key *gen_key, uint16_t conn_handle) {
  int ret;

  if (key != BLE_STORE_GEN_KEY_IRK) {
    return BLE_HS_ENOTSUP;
  }

  if (pbl_bt__sprf_irk_get(gen_key->irk) == 0) {
    return 0;
  }

  ret = ble_hs_hci_rand(gen_key->irk, sizeof(gen_key->irk));
  if (ret != 0) {
    PBL_LOG_ERR("Could not generate the IRK: 0x%x", ret);
    return ret;
  }

  pbl_bt__sprf_irk_set(gen_key->irk);

  return 0;
}

void pbl_bt__store_init(void) {
  ble_hs_cfg.store_read_cb = prv_store_read;
  ble_hs_cfg.store_write_cb = prv_store_write;
  ble_hs_cfg.store_delete_cb = prv_store_delete;
  ble_hs_cfg.store_gen_key_cb = prv_store_gen_key;
}

void pbl_bt__store_start(void) {
}

static bool prv_free_cb(ListNode *node, void *context) {
  kernel_free(node);
  return true;
}

void pbl_bt__store_stop(void) {
  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);
  list_foreach((ListNode *)s_cccds, prv_free_cb, nullptr);
  s_cccds = nullptr;
  pbl_mutex_unlock(&s_store_mutex);
}
