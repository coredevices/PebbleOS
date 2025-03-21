/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/bonding_sync.h>
#include <bluetooth/gap_le_connect.h>
#include <comm/bt_lock.h>
#include <host/ble_hs.h>
#include <string.h>
#include <system/logging.h>
#include <system/passert.h>
#include <util/list.h>

#include "nimble_type_conversions.h"

#define KEY_SIZE 16

#define BLE_FLAG_SECURE_CONNECTIONS 0x01

typedef struct {
  ListNode node;
  struct ble_store_value_sec value_sec;
} BleStoreValue;

static BleStoreValue *s_value_secs;

static bool prv_nimble_store_find_sec_cb(ListNode *node, void *data) {
  BleStoreValue *s = (BleStoreValue *)node;
  struct ble_store_key_sec *key_sec = (struct ble_store_key_sec *)data;

  return ble_addr_cmp(&s->value_sec.peer_addr, &key_sec->peer_addr) == 0;
}

static BleStoreValue *prv_nimble_store_find_sec(const struct ble_store_key_sec *key_sec) {
  if (!ble_addr_cmp(&key_sec->peer_addr, BLE_ADDR_ANY)) {
    return (BleStoreValue *)list_get_at((ListNode *)s_value_secs, key_sec->idx);
  } else if (key_sec->idx == 0) {
    return (BleStoreValue *)list_find((ListNode *)s_value_secs, prv_nimble_store_find_sec_cb,
                                      (void *)&key_sec->peer_addr);
  }

  return NULL;
}

static int prv_nimble_store_read_our_sec(const struct ble_store_key_sec *key_sec,
                                         struct ble_store_value_sec *value_sec) {
  int ret = 0;
  BleStoreValue *s;

  bt_lock();

  s = prv_nimble_store_find_sec(key_sec);
  if (s == NULL) {
    ret = BLE_HS_ENOENT;
    goto unlock;
  }

  memset(value_sec, 0, sizeof(*value_sec));

  value_sec->peer_addr = key_sec->peer_addr;
  value_sec->key_size = KEY_SIZE;

  if (s->value_sec.ltk_present) {
    value_sec->ediv = s->value_sec.ediv;
    value_sec->rand_num = s->value_sec.rand_num;
    value_sec->ltk_present = true;
    memcpy(value_sec->ltk, s->value_sec.ltk, KEY_SIZE);
  }

unlock:
  bt_unlock();

  return ret;
}

static int prv_nimble_store_read_peer_sec(const struct ble_store_key_sec *key_sec,
                                          struct ble_store_value_sec *value_sec) {
  int ret = 0;
  BleStoreValue *s;

  bt_lock();

  s = prv_nimble_store_find_sec(key_sec);
  if (s == NULL) {
    ret = BLE_HS_ENOENT;
    goto unlock;
  }

  *value_sec = s->value_sec;

unlock:
  bt_unlock();

  return ret;
}

static int prv_nimble_store_write_peer_sec(const struct ble_store_value_sec *value_sec) {
  BleStoreValue *s;
  struct ble_store_key_sec key_sec;
  BleBonding bonding;
  BTDeviceAddress addr;

  if (value_sec->key_size != KEY_SIZE || value_sec->authenticated || value_sec->csrk_present) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Unsupported security parameters");
    return BLE_HS_ENOTSUP;
  }

  ble_store_key_from_value_sec(&key_sec, value_sec);

  bt_lock();

  s = prv_nimble_store_find_sec(&key_sec);
  if (s == NULL) {
    s = kernel_zalloc_check(sizeof(BleStoreValue));
    if (s_value_secs == NULL) {
      s_value_secs = s;
    } else {
      list_append((ListNode *)s_value_secs, (ListNode *)s);
    }
  }

  s->value_sec = *value_sec;

  bt_unlock();

  // inform about new IRK
  if (value_sec->irk_present) {
    BleIRKChange irk_change_event;

    irk_change_event.irk_valid = true;
    memcpy(irk_change_event.irk.data, value_sec->irk, KEY_SIZE);

    nimble_addr_to_pebble_device(&value_sec->peer_addr, &irk_change_event.device);

    bt_driver_handle_le_connection_handle_update_irk(&irk_change_event);
  }

  // persist bonding
  memset(&bonding, 0, sizeof(bonding));

  bonding.is_gateway = true;

  if (value_sec->ltk_present) {
    bonding.pairing_info.is_remote_encryption_info_valid = true;
    bonding.pairing_info.remote_encryption_info.ediv = value_sec->ediv;
    bonding.pairing_info.remote_encryption_info.rand = value_sec->rand_num;
    memcpy(bonding.pairing_info.remote_encryption_info.ltk.data, value_sec->ltk, KEY_SIZE);
  }

  if (value_sec->irk_present) {
    bonding.pairing_info.is_remote_identity_info_valid = true;
    memcpy(bonding.pairing_info.irk.data, value_sec->irk, KEY_SIZE);
  }

  if (value_sec->sc) {
    bonding.flags |= BLE_FLAG_SECURE_CONNECTIONS;
  }

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &bonding.pairing_info.identity);

  nimble_addr_to_pebble_addr(&value_sec->peer_addr, &addr);

  bt_driver_cb_handle_create_bonding(&bonding, &addr);

  return 0;
}

static int prv_nimble_store_read(int obj_type, const union ble_store_key *key,
                                 union ble_store_value *value) {
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      return prv_nimble_store_read_peer_sec(&key->sec, &value->sec);
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
      return prv_nimble_store_read_our_sec(&key->sec, &value->sec);
    default:
      return BLE_HS_ENOTSUP;
  }
}

static int prv_nimble_store_write(int obj_type, const union ble_store_value *val) {
  switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
      return prv_nimble_store_write_peer_sec(&val->sec);
    default:
      return BLE_HS_ENOTSUP;
  }
}

void nimble_store_init(void) {
  ble_hs_cfg.store_read_cb = prv_nimble_store_read;
  ble_hs_cfg.store_write_cb = prv_nimble_store_write;
}

void bt_driver_handle_host_added_bonding(const BleBonding *bonding) {
  bool is_new = false;
  BleStoreValue *s;
  struct ble_store_key_sec key_sec;

  key_sec.idx = 0;
  pebble_device_to_nimble_addr(&bonding->pairing_info.identity, &key_sec.peer_addr);

  bt_lock();

  s = prv_nimble_store_find_sec(&key_sec);
  if (s == NULL) {
    s = kernel_zalloc_check(sizeof(BleStoreValue));
    is_new = true;
  }

  s->value_sec.key_size = KEY_SIZE;

  if (bonding->pairing_info.is_remote_encryption_info_valid) {
    s->value_sec.ediv = bonding->pairing_info.remote_encryption_info.ediv;
    s->value_sec.rand_num = bonding->pairing_info.remote_encryption_info.rand;
    s->value_sec.ltk_present = true;
    memcpy(s->value_sec.ltk, bonding->pairing_info.remote_encryption_info.ltk.data, KEY_SIZE);
  }

  if (bonding->pairing_info.is_remote_identity_info_valid) {
    s->value_sec.irk_present = true;
    memcpy(s->value_sec.irk, bonding->pairing_info.irk.data, KEY_SIZE);
  }

  s->value_sec.sc = !!(bonding->flags & BLE_FLAG_SECURE_CONNECTIONS);

  pebble_device_to_nimble_addr(&bonding->pairing_info.identity, &s->value_sec.peer_addr);

  if (is_new) {
    if (s_value_secs == NULL) {
      s_value_secs = s;
    } else {
      list_append((ListNode *)s_value_secs, (ListNode *)s);
    }
  }

  bt_unlock();
}

void bt_driver_handle_host_removed_bonding(const BleBonding *bonding) {
  BleStoreValue *s;
  struct ble_store_key_sec key_sec;

  key_sec.idx = 0;
  pebble_device_to_nimble_addr(&bonding->pairing_info.identity, &key_sec.peer_addr);

  bt_lock();

  s = prv_nimble_store_find_sec(&key_sec);
  list_remove((ListNode *)s, (ListNode **)&s_value_secs, NULL);

  bt_unlock();

  if (s != NULL) {
    kernel_free(s);
  }
}
