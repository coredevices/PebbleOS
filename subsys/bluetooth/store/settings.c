/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/util/units.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// Records are the stack's own values, as its config persistence backend stores them. The bond is
// mirrored to shared PRF storage, so recovery firmware can reconnect.

#define STORE_FILE_NAME "bt_store"
#define STORE_FILE_SIZE PBL_KIB(4)

static_assert(sizeof(struct ble_store_value_sec) == 80, "Persisted layout changed");
static_assert(sizeof(struct ble_store_value_cccd) == 16, "Persisted layout changed");

struct PBL_PACKED pbl_bt__store_key {
  uint8_t obj_type;
  ble_addr_t peer_addr;
  uint16_t chr_val_handle;
};

static const char IRK_KEY[] = "irk";

static PBL_MUTEX_DEFINE(s_store_mutex);

static struct pbl_bt__store_key prv_key(int obj_type, const ble_addr_t *peer_addr,
                                        uint16_t chr_val_handle) {
  return (struct pbl_bt__store_key){
    .obj_type = obj_type,
    .peer_addr = *peer_addr,
    .chr_val_handle = chr_val_handle,
  };
}

static int prv_file_open(SettingsFile *fd) {
  pbl_mutex_lock(&s_store_mutex, PBL_FOREVER);

  if (settings_file_open(fd, STORE_FILE_NAME, STORE_FILE_SIZE) != S_SUCCESS) {
    pbl_mutex_unlock(&s_store_mutex);
    return BLE_HS_ESTORE_FAIL;
  }

  return 0;
}

static void prv_file_close(SettingsFile *fd) {
  settings_file_close(fd);
  pbl_mutex_unlock(&s_store_mutex);
}

static int prv_file_get(const void *key, size_t key_len, void *val, size_t val_len) {
  SettingsFile fd;
  int ret;

  ret = prv_file_open(&fd);
  if (ret != 0) {
    return ret;
  }

  if (settings_file_get_len(&fd, key, key_len) != (int)val_len) {
    ret = BLE_HS_ENOENT;
  } else if (settings_file_get(&fd, key, key_len, val, val_len) != S_SUCCESS) {
    ret = BLE_HS_ESTORE_FAIL;
  }

  prv_file_close(&fd);

  return ret;
}

static int prv_file_set(const void *key, size_t key_len, const void *val, size_t val_len) {
  SettingsFile fd;
  uint8_t cur[val_len];
  status_t status;
  int ret;

  ret = prv_file_open(&fd);
  if (ret != 0) {
    return ret;
  }

  if (settings_file_get_len(&fd, key, key_len) == (int)val_len &&
      settings_file_get(&fd, key, key_len, cur, val_len) == S_SUCCESS &&
      memcmp(cur, val, val_len) == 0) {
    goto close;
  }

  status = settings_file_set(&fd, key, key_len, val, val_len);
  if (status == E_OUT_OF_STORAGE) {
    ret = BLE_HS_ESTORE_CAP;
  } else if (status != S_SUCCESS) {
    ret = BLE_HS_ESTORE_FAIL;
  }

close:
  prv_file_close(&fd);

  return ret;
}

static int prv_file_delete(const struct pbl_bt__store_key *key) {
  SettingsFile fd;
  status_t status;
  int ret;

  ret = prv_file_open(&fd);
  if (ret != 0) {
    return ret;
  }

  status = settings_file_delete(&fd, key, sizeof(*key));
  if (status == E_DOES_NOT_EXIST) {
    ret = BLE_HS_ENOENT;
  } else if (status != S_SUCCESS) {
    ret = BLE_HS_ESTORE_FAIL;
  }

  prv_file_close(&fd);

  return ret;
}

typedef bool (*pbl_bt__store_match_cb_t)(const struct pbl_bt__store_key *key, const void *ctx);

struct pbl_bt__store_find_ctx {
  pbl_bt__store_match_cb_t match;
  const void *match_ctx;
  unsigned int idx;
  void *val;
  size_t val_len;
  struct pbl_bt__store_key key;
  bool found;
};

static bool prv_find_itr(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  struct pbl_bt__store_find_ctx *ctx = context;
  struct pbl_bt__store_key key;

  if (info->key_len != sizeof(key) || info->val_len != (int)ctx->val_len) {
    return true;
  }

  info->get_key(file, &key, sizeof(key));
  if (!ctx->match(&key, ctx->match_ctx)) {
    return true;
  }

  if (ctx->idx > 0U) {
    ctx->idx--;
    return true;
  }

  info->get_val(file, ctx->val, ctx->val_len);
  ctx->key = key;
  ctx->found = true;

  return false;
}

//! Finds the idx-th record accepted by match.
static int prv_file_find(struct pbl_bt__store_find_ctx *ctx) {
  SettingsFile fd;
  int ret;

  ret = prv_file_open(&fd);
  if (ret != 0) {
    return ret;
  }

  settings_file_each(&fd, prv_find_itr, ctx);
  prv_file_close(&fd);

  return ctx->found ? 0 : BLE_HS_ENOENT;
}

struct pbl_bt__sec_match {
  int obj_type;
  const ble_addr_t *peer_addr;
};

static bool prv_match_sec(const struct pbl_bt__store_key *key, const void *ctx) {
  const struct pbl_bt__sec_match *match = ctx;

  return key->obj_type == match->obj_type && (ble_addr_cmp(match->peer_addr, BLE_ADDR_ANY) == 0 ||
                                              ble_addr_cmp(&key->peer_addr, match->peer_addr) == 0);
}

static int prv_read_sec(int obj_type, const struct ble_store_key_sec *key_sec,
                        struct ble_store_value_sec *value_sec) {
  const struct pbl_bt__sec_match match = {
    .obj_type = obj_type,
    .peer_addr = &key_sec->peer_addr,
  };
  struct pbl_bt__store_find_ctx ctx = {
    .match = prv_match_sec,
    .match_ctx = &match,
    .idx = key_sec->idx,
    .val = value_sec,
    .val_len = sizeof(*value_sec),
  };

  return prv_file_find(&ctx);
}

static bool prv_has_sec(int obj_type, const ble_addr_t *peer_addr) {
  const struct ble_store_key_sec key_sec = {
    .peer_addr = *peer_addr,
  };
  struct ble_store_value_sec value_sec;

  return prv_read_sec(obj_type, &key_sec, &value_sec) == 0;
}

static bool prv_match_cccd(const struct pbl_bt__store_key *key, const void *ctx) {
  const struct ble_store_key_cccd *key_cccd = ctx;

  return key->obj_type == BLE_STORE_OBJ_TYPE_CCCD &&
         (ble_addr_cmp(&key_cccd->peer_addr, BLE_ADDR_ANY) == 0 ||
          ble_addr_cmp(&key->peer_addr, &key_cccd->peer_addr) == 0) &&
         (key_cccd->chr_val_handle == 0U || key->chr_val_handle == key_cccd->chr_val_handle);
}

static int prv_read_cccd(const struct ble_store_key_cccd *key_cccd,
                         struct ble_store_value_cccd *value_cccd) {
  struct pbl_bt__store_find_ctx ctx = {
    .match = prv_match_cccd,
    .match_ctx = key_cccd,
    .idx = key_cccd->idx,
    .val = value_cccd,
    .val_len = sizeof(*value_cccd),
  };

  return prv_file_find(&ctx);
}

static int prv_write_cccd(const struct ble_store_value_cccd *value_cccd) {
  const struct pbl_bt__store_key key =
      prv_key(BLE_STORE_OBJ_TYPE_CCCD, &value_cccd->peer_addr, value_cccd->chr_val_handle);

  return prv_file_set(&key, sizeof(key), value_cccd, sizeof(*value_cccd));
}

static int prv_delete_cccd(const struct ble_store_key_cccd *key_cccd) {
  struct ble_store_value_cccd value_cccd;
  struct pbl_bt__store_key key;
  int ret;

  ret = prv_read_cccd(key_cccd, &value_cccd);
  if (ret != 0) {
    return ret;
  }

  key = prv_key(BLE_STORE_OBJ_TYPE_CCCD, &value_cccd.peer_addr, value_cccd.chr_val_handle);

  return prv_file_delete(&key);
}

static bool prv_match_other_peer(const struct pbl_bt__store_key *key, const void *ctx) {
  const ble_addr_t *keep = ctx;

  return (key->obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC ||
          key->obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) &&
         ble_addr_cmp(&key->peer_addr, keep) != 0;
}

//! Only one bond is kept: drops the bonds of all peers but @p keep.
static void prv_delete_other_peers(const ble_addr_t *keep) {
  for (;;) {
    struct ble_store_value_sec value_sec;
    struct pbl_bt__store_find_ctx ctx = {
      .match = prv_match_other_peer,
      .match_ctx = keep,
      .val = &value_sec,
      .val_len = sizeof(value_sec),
    };
    struct ble_store_key_cccd key_cccd = {};
    struct pbl_bt__store_key key;
    bool had_peer_sec;

    if (prv_file_find(&ctx) != 0) {
      return;
    }

    had_peer_sec = prv_has_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &ctx.key.peer_addr);

    key = prv_key(BLE_STORE_OBJ_TYPE_OUR_SEC, &ctx.key.peer_addr, 0U);
    (void)prv_file_delete(&key);
    key = prv_key(BLE_STORE_OBJ_TYPE_PEER_SEC, &ctx.key.peer_addr, 0U);
    (void)prv_file_delete(&key);

    key_cccd.peer_addr = ctx.key.peer_addr;
    while (prv_delete_cccd(&key_cccd) == 0) {
    }

    if (had_peer_sec) {
      pbl_bt__store_bond_deleted(&ctx.key.peer_addr);
    }
  }
}

static int prv_store_sec(int obj_type, const struct ble_store_value_sec *value_sec) {
  const struct pbl_bt__store_key key = prv_key(obj_type, &value_sec->peer_addr, 0U);

  return prv_file_set(&key, sizeof(key), value_sec, sizeof(*value_sec));
}

static int prv_write_sec(int obj_type, const struct ble_store_value_sec *value_sec) {
  const bool is_new_bond = !prv_has_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &value_sec->peer_addr);
  int ret;

  if (is_new_bond && !prv_has_sec(BLE_STORE_OBJ_TYPE_OUR_SEC, &value_sec->peer_addr)) {
    prv_delete_other_peers(&value_sec->peer_addr);
  }

  ret = prv_store_sec(obj_type, value_sec);
  if (ret != 0) {
    PBL_LOG_ERR("Could not store the bond: 0x%x", ret);
    return ret;
  }

  ret = pbl_bt__sprf_write_sec(obj_type, value_sec, nullptr);
  if (ret != 0) {
    PBL_LOG_WRN("Bond not mirrored to shared PRF storage: 0x%x", ret);
  }

  pbl_bt__store_sec_written(obj_type, value_sec, is_new_bond);

  return 0;
}

static int prv_delete_sec(int obj_type, const struct ble_store_key_sec *key_sec) {
  struct ble_store_value_sec value_sec;
  struct pbl_bt__store_key key;
  int ret;

  ret = prv_read_sec(obj_type, key_sec, &value_sec);
  if (ret != 0) {
    return ret;
  }

  key = prv_key(obj_type, &value_sec.peer_addr, 0U);
  ret = prv_file_delete(&key);
  if (ret != 0) {
    return ret;
  }

  (void)pbl_bt__sprf_delete_sec(obj_type, &value_sec.peer_addr);

  if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
    pbl_bt__store_bond_deleted(&value_sec.peer_addr);
  }

  return 0;
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

  ret = prv_file_get(IRK_KEY, sizeof(IRK_KEY), gen_key->irk, sizeof(gen_key->irk));
  if (ret != BLE_HS_ENOENT) {
    return ret;
  }

  ret = ble_hs_hci_rand(gen_key->irk, sizeof(gen_key->irk));
  if (ret != 0) {
    PBL_LOG_ERR("Could not generate the IRK: 0x%x", ret);
    return ret;
  }

  ret = prv_file_set(IRK_KEY, sizeof(IRK_KEY), gen_key->irk, sizeof(gen_key->irk));
  if (ret != 0) {
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

//! Recovery firmware wins: it may have bonded or generated the IRK since the last boot.
static void prv_sync_irk(void) {
  uint8_t sprf_irk[PBL_BT__STORE_KEY_SIZE];
  uint8_t irk[PBL_BT__STORE_KEY_SIZE];
  const bool has_irk = prv_file_get(IRK_KEY, sizeof(IRK_KEY), irk, sizeof(irk)) == 0;

  if (pbl_bt__sprf_irk_get(sprf_irk) == 0) {
    if (!has_irk || memcmp(irk, sprf_irk, sizeof(irk)) != 0) {
      (void)prv_file_set(IRK_KEY, sizeof(IRK_KEY), sprf_irk, sizeof(sprf_irk));
    }
  } else if (has_irk) {
    pbl_bt__sprf_irk_set(irk);
  }
}

//! Compares what shared PRF storage holds of a record.
static bool prv_sec_matches(int obj_type, const struct ble_store_value_sec *a,
                            const struct ble_store_value_sec *b) {
  if (a->sc != b->sc || a->authenticated != b->authenticated || a->ltk_present != b->ltk_present) {
    return false;
  }

  if (a->ltk_present && (a->ediv != b->ediv || a->rand_num != b->rand_num ||
                         memcmp(a->ltk, b->ltk, sizeof(a->ltk)) != 0)) {
    return false;
  }

  if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
    return true;
  }

  return a->irk_present == b->irk_present && a->csrk_present == b->csrk_present &&
         (!a->irk_present || memcmp(a->irk, b->irk, sizeof(a->irk)) == 0) &&
         (!a->csrk_present || memcmp(a->csrk, b->csrk, sizeof(a->csrk)) == 0);
}

static bool prv_sync_sec_from_sprf(int obj_type, const ble_addr_t *peer_addr) {
  const struct ble_store_key_sec key_sec = {
    .peer_addr = *peer_addr,
  };
  struct ble_store_value_sec sprf_value;
  struct ble_store_value_sec value;

  if (pbl_bt__sprf_read_sec(obj_type, &key_sec, &sprf_value) != 0) {
    return false;
  }

  if (prv_read_sec(obj_type, &key_sec, &value) == 0 &&
      prv_sec_matches(obj_type, &value, &sprf_value)) {
    return false;
  }

  (void)prv_store_sec(obj_type, &sprf_value);

  return true;
}

static void prv_sync_bond(void) {
  const struct ble_store_key_sec any = {
    .peer_addr = *BLE_ADDR_ANY,
  };
  struct ble_store_value_sec value_sec;

  if (pbl_bt__sprf_read_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &any, &value_sec) == 0) {
    const bool is_new_bond = !prv_has_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &value_sec.peer_addr);

    if (is_new_bond) {
      prv_delete_other_peers(&value_sec.peer_addr);
    }

    (void)prv_sync_sec_from_sprf(BLE_STORE_OBJ_TYPE_OUR_SEC, &value_sec.peer_addr);
    if (prv_sync_sec_from_sprf(BLE_STORE_OBJ_TYPE_PEER_SEC, &value_sec.peer_addr)) {
      PBL_LOG_INFO("Imported the bond from shared PRF storage");
      pbl_bt__store_sec_written(BLE_STORE_OBJ_TYPE_PEER_SEC, &value_sec, is_new_bond);
    }
    return;
  }

  if (prv_read_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &any, &value_sec) != 0) {
    return;
  }

  PBL_LOG_INFO("Mirroring the bond to shared PRF storage");
  (void)pbl_bt__sprf_write_sec(BLE_STORE_OBJ_TYPE_PEER_SEC, &value_sec, nullptr);

  const struct ble_store_key_sec key_sec = {
    .peer_addr = value_sec.peer_addr,
  };
  if (prv_read_sec(BLE_STORE_OBJ_TYPE_OUR_SEC, &key_sec, &value_sec) == 0) {
    (void)pbl_bt__sprf_write_sec(BLE_STORE_OBJ_TYPE_OUR_SEC, &value_sec, nullptr);
  }
}

void pbl_bt__store_start(void) {
  prv_sync_irk();
  prv_sync_bond();
}

void pbl_bt__store_stop(void) {
}
