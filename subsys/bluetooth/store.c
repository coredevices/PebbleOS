/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "store.h"

#include "nimble_type_conversions.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bonding_sync.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <kernel/event_loop.h>
#include <nimble/nimble_port_pebble.h>
#include <store/config/ble_store_config.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define KEY_SIZE 16

#define BLE_FLAG_SECURE_CONNECTIONS 0x01
#define BLE_FLAG_AUTHENTICATED      0x02

// NimBLE's store is not thread-safe, and the firmware adds and removes bondings from its own
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

static bool prv_cccd_equal(const struct ble_store_value_cccd *a,
                           const struct ble_store_value_cccd *b) {
  return ble_addr_cmp(&a->peer_addr, &b->peer_addr) == 0 &&
         a->chr_val_handle == b->chr_val_handle && a->flags == b->flags &&
         a->value_changed == b->value_changed;
}

// Writes the value unless the store already has it, so that registering the firmware's bondings
// on every start does not rewrite the store.
static int prv_write_if_changed(int obj_type, const union ble_store_value *value) {
  union ble_store_key key;
  union ble_store_value stored;
  bool equal;
  int rc;

  ble_store_key_from_value(obj_type, &key, value);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_read(obj_type, &key, &stored);
  if (rc == 0) {
    equal = obj_type == BLE_STORE_OBJ_TYPE_CCCD ? prv_cccd_equal(&stored.cccd, &value->cccd)
                                                : prv_sec_equal(&stored.sec, &value->sec);
  } else {
    equal = false;
  }
  if (!equal) {
    rc = ble_store_config_write(obj_type, value);
  }
  pbl_mutex_unlock(&s_lock);

  return rc;
}

static void prv_convert_peer_sec_to_bonding(const struct ble_store_value_sec *value_sec,
                                            struct pbl_bt_bonding *bonding) {
  if (value_sec->ltk_present) {
    bonding->pairing_info.is_remote_encryption_info_valid = true;
    bonding->pairing_info.remote_encryption_info.ediv = value_sec->ediv;
    bonding->pairing_info.remote_encryption_info.rand = value_sec->rand_num;
    memcpy(bonding->pairing_info.remote_encryption_info.ltk.data, value_sec->ltk, KEY_SIZE);
  }

  if (value_sec->irk_present) {
    bonding->pairing_info.is_remote_identity_info_valid = true;
    memcpy(bonding->pairing_info.irk.data, value_sec->irk, KEY_SIZE);
  }
}

static void prv_convert_our_sec_to_bonding(const struct ble_store_value_sec *value_sec,
                                           struct pbl_bt_bonding *bonding) {
  if (value_sec->ltk_present) {
    bonding->pairing_info.is_local_encryption_info_valid = true;
    bonding->pairing_info.local_encryption_info.ediv = value_sec->ediv;
    bonding->pairing_info.local_encryption_info.rand = value_sec->rand_num;
    memcpy(bonding->pairing_info.local_encryption_info.ltk.data, value_sec->ltk, KEY_SIZE);
  }
}

static void prv_notify_irk_updated(const struct ble_store_value_sec *value_sec) {
  struct pbl_bt_irk_change irk_change_event;

  irk_change_event.irk_valid = true;
  memcpy(irk_change_event.irk.data, value_sec->irk, KEY_SIZE);

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &irk_change_event.device);

  pbl_bt_handle_le_connection_handle_update_irk(&irk_change_event);
}

static void prv_notify_host_bonding_changed(int obj_type,
                                            const struct ble_store_value_sec *value_sec) {
  struct pbl_bt_bonding bonding = {
    .is_gateway = true,
  };
  union ble_store_key key;
  union ble_store_value other;
  struct pbl_bt_addr addr;

  ble_store_key_from_value_sec(&key.sec, value_sec);

  // The firmware keeps both sides' keys in one bonding: combine them.
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      if (prv_read(BLE_STORE_OBJ_TYPE_OUR_SEC, &key, &other) == 0) {
        prv_convert_our_sec_to_bonding(&other.sec, &bonding);
      }
      prv_convert_peer_sec_to_bonding(value_sec, &bonding);
      break;
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
      if (prv_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &other) == 0) {
        prv_convert_peer_sec_to_bonding(&other.sec, &bonding);
      }
      prv_convert_our_sec_to_bonding(value_sec, &bonding);
      break;
  }

  if (value_sec->sc) {
    bonding.flags |= BLE_FLAG_SECURE_CONNECTIONS;
  }

  if (value_sec->authenticated) {
    bonding.flags |= BLE_FLAG_AUTHENTICATED;
  }

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &bonding.pairing_info.identity);
  nimble_addr_to_pebble_addr(&value_sec->peer_addr, &addr);

  if (bonding.pairing_info.is_remote_encryption_info_valid) {
    pbl_bt_cb_handle_create_bonding(&bonding, &addr);
  } else {
    PBL_LOG_DBG("Skipping notifying OS of our keys");
  }
}

struct sec_written {
  int obj_type;
  struct ble_store_value_sec value_sec;
};

static void prv_handle_sec_written_cb(void *data) {
  struct sec_written *ctx = data;

  if (ctx->obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC && ctx->value_sec.irk_present) {
    prv_notify_irk_updated(&ctx->value_sec);
  }

  prv_notify_host_bonding_changed(ctx->obj_type, &ctx->value_sec);

  pbl_free(ctx);
}

static bool prv_is_sec(int obj_type) {
  return obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC || obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC;
}

static int prv_store_write(int obj_type, const union ble_store_value *value) {
  struct sec_written *ctx;
  int rc;

  if (prv_is_sec(obj_type) && (value->sec.key_size != KEY_SIZE || value->sec.csrk_present)) {
    PBL_LOG_ERR("Unsupported security parameters, dropping bonding keys");
    return BLE_HS_ENOTSUP;
  }

  rc = prv_write(obj_type, value);
  if (rc != 0 || !prv_is_sec(obj_type)) {
    return rc;
  }

  ctx = pbl_malloc_check(sizeof(*ctx));
  *ctx = (struct sec_written){
    .obj_type = obj_type,
    .value_sec = value->sec,
  };
  launcher_task_add_callback(prv_handle_sec_written_cb, ctx);

  return 0;
}

static int prv_store_delete(int obj_type, const union ble_store_key *key) {
  struct pbl_bt_device_internal device;
  int rc;

  rc = prv_delete(obj_type, key);
  if (rc != 0 || obj_type != BLE_STORE_OBJ_TYPE_PEER_SEC) {
    return rc;
  }

  nimble_addr_to_pebble_device(&key->sec.peer_addr, &device);
  PBL_LOG_INFO("Bonding deleted: addr=" PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(device.address));
  bt_persistent_storage_delete_ble_pairing_by_addr(&device);

  return 0;
}

// The local IRK is the firmware's identity root key, which the recovery firmware shares, rather
// than one of the store's own. LTKs and CSRKs are generated by NimBLE.
static int prv_gen_key(uint8_t key, struct ble_store_gen_key *gen_key, uint16_t conn_handle) {
  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];

  if (key != BLE_STORE_GEN_KEY_IRK) {
    return BLE_HS_ENOTSUP;
  }

  if (!bt_persistent_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY,
                                          &keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY])) {
    int rc;

    rc = ble_hs_hci_rand(keys, sizeof(keys));
    if (rc != 0) {
      PBL_LOG_ERR("Could not generate root keys: %d", rc);
      return rc;
    }

    bt_persistent_storage_set_root_keys(keys);
  }

  memcpy(gen_key->irk, keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY].data, KEY_SIZE);

  return 0;
}

void pbl_bt__store_init(void) {
  ble_store_config_init();

  ble_hs_cfg.store_gen_key_cb = prv_gen_key;
  ble_hs_cfg.store_read_cb = prv_read;
  ble_hs_cfg.store_write_cb = prv_store_write;
  ble_hs_cfg.store_delete_cb = prv_store_delete;
  // A new bonding replaces the oldest one when the store is full.
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
}

static void prv_bonding_to_value_sec(const struct pbl_bt_bonding *bonding, bool local,
                                     struct ble_store_value_sec *value_sec) {
  const struct pbl_bt_sm_pairing_info *info = &bonding->pairing_info;

  memset(value_sec, 0, sizeof(*value_sec));
  value_sec->key_size = KEY_SIZE;
  value_sec->sc = (bonding->flags & BLE_FLAG_SECURE_CONNECTIONS) != 0U;
  value_sec->authenticated = (bonding->flags & BLE_FLAG_AUTHENTICATED) != 0U;
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

// The firmware's record of a bonding, which the store takes when it does not have it: from the
// firmware's database, where bondings were kept before the store, or from shared PRF storage.
void pbl_bt_handle_host_added_bonding(const struct pbl_bt_bonding *bonding) {
  union ble_store_value value;

  PBL_LOG_DBG("Host added bonding: addr=" PBL_BT_ADDR_FMT " random=%u",
              PBL_BT_ADDR_XPLODE(bonding->pairing_info.identity.address),
              bonding->pairing_info.identity.is_random_address);

  prv_bonding_to_value_sec(bonding, false, &value.sec);
  (void)prv_write_if_changed(BLE_STORE_OBJ_TYPE_PEER_SEC, &value);

  prv_bonding_to_value_sec(bonding, true, &value.sec);
  (void)prv_write_if_changed(BLE_STORE_OBJ_TYPE_OUR_SEC, &value);
}

void pbl_bt_handle_host_removed_bonding(const struct pbl_bt_bonding *bonding) {
  union ble_store_key key;

  PBL_LOG_INFO("Host removed bonding: addr=" PBL_BT_ADDR_FMT " random=%u",
               PBL_BT_ADDR_XPLODE(bonding->pairing_info.identity.address),
               bonding->pairing_info.identity.is_random_address);

  memset(&key, 0, sizeof(key));
  pebble_device_to_nimble_addr(&bonding->pairing_info.identity, &key.sec.peer_addr);

  (void)prv_delete(BLE_STORE_OBJ_TYPE_OUR_SEC, &key);
  (void)prv_delete(BLE_STORE_OBJ_TYPE_PEER_SEC, &key);

  // With no handle, the key matches every CCCD of the peer.
  ble_addr_t peer_addr = key.sec.peer_addr;
  memset(&key, 0, sizeof(key));
  key.cccd.peer_addr = peer_addr;
  while (prv_delete(BLE_STORE_OBJ_TYPE_CCCD, &key) == 0) {
  }
}

int pbl_bt_handle_host_added_cccd(const struct pbl_bt_cccd *cccd) {
  union ble_store_value value;
  union ble_store_value bonding;
  union ble_store_key key;
  int rc;

  memset(&value, 0, sizeof(value));
  pebble_device_to_nimble_addr(&cccd->peer, &value.cccd.peer_addr);
  value.cccd.chr_val_handle = cccd->chr_val_handle;
  value.cccd.flags = cccd->flags;
  value.cccd.value_changed = cccd->value_changed;

  memset(&key, 0, sizeof(key));
  key.sec.peer_addr = value.cccd.peer_addr;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  rc = ble_store_config_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &bonding);
  pbl_mutex_unlock(&s_lock);
  if (rc != 0) {
    return -ENOENT;
  }

  rc = prv_write_if_changed(BLE_STORE_OBJ_TYPE_CCCD, &value);
  if (rc == BLE_HS_ESTORE_CAP) {
    return -ENOSPC;
  }

  return rc == 0 ? 0 : -EIO;
}
