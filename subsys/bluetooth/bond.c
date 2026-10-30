/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "nimble_type_conversions.h"
#include "store/store.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/logging/logging.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <kernel/event_loop.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define MAX_BONDS 4

static void prv_value_to_info(const struct ble_store_value_sec *value_sec,
                              struct pbl_bt_bond_info *info) {
  *info = (struct pbl_bt_bond_info){
    .irk_valid = value_sec->irk_present,
  };

  nimble_addr_to_pebble_device(&value_sec->peer_addr, &info->addr);
  if (value_sec->irk_present) {
    memcpy(info->irk.data, value_sec->irk, PBL_BT__STORE_KEY_SIZE);
  }
}

struct pbl_bt__sec_written_ctx {
  struct ble_store_value_sec value_sec;
  bool is_new_bond;
};

static void prv_sec_written_cb(void *data) {
  struct pbl_bt__sec_written_ctx *ctx = data;
  struct pbl_bt_bond_info info;

  prv_value_to_info(&ctx->value_sec, &info);

  if (info.irk_valid) {
    struct pbl_bt_irk_change irk_change = {
      .device = info.addr,
      .irk = info.irk,
      .irk_valid = true,
    };
    pbl_bt_handle_le_connection_handle_update_irk(&irk_change);
  }

  if (ctx->is_new_bond) {
    PBL_LOG_INFO("Bonded with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(info.addr.address));
    pbl_bt_cb_bond_added(&info);
  }

  kernel_free(ctx);
}

void pbl_bt__store_sec_written(int obj_type, const struct ble_store_value_sec *value_sec,
                               bool is_new_bond) {
  struct pbl_bt__sec_written_ctx *ctx;

  if (obj_type != BLE_STORE_OBJ_TYPE_PEER_SEC) {
    return;
  }

  ctx = kernel_malloc_check(sizeof(*ctx));
  *ctx = (struct pbl_bt__sec_written_ctx){
    .value_sec = *value_sec,
    .is_new_bond = is_new_bond,
  };
  launcher_task_add_callback(prv_sec_written_cb, ctx);
}

static void prv_bond_deleted_cb(void *data) {
  struct pbl_bt_device_internal *addr = data;

  PBL_LOG_INFO("Deleted bond with " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(addr->address));
  pbl_bt_cb_bond_deleted(addr);

  kernel_free(addr);
}

void pbl_bt__store_bond_deleted(const ble_addr_t *peer_addr) {
  struct pbl_bt_device_internal *addr = kernel_malloc_check(sizeof(*addr));

  nimble_addr_to_pebble_device(peer_addr, addr);
  launcher_task_add_callback(prv_bond_deleted_cb, addr);
}

void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data) {
  for (uint8_t idx = 0;; idx++) {
    union ble_store_key key = {
      .sec = {
        .peer_addr = *BLE_ADDR_ANY,
        .idx = idx,
      },
    };
    union ble_store_value value;
    struct pbl_bt_bond_info info;

    if (ble_store_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &value) != 0) {
      return;
    }

    prv_value_to_info(&value.sec, &info);
    func(&info, user_data);
  }
}

static void prv_count_cb(const struct pbl_bt_bond_info *info, void *user_data) {
  size_t *count = user_data;

  (*count)++;
}

size_t pbl_bt_bond_count(void) {
  size_t count = 0;

  pbl_bt_foreach_bond(prv_count_cb, &count);

  return count;
}

int pbl_bt_bond_get(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_info *info) {
  union ble_store_key key = {};
  union ble_store_value value;

  pebble_device_to_nimble_addr(addr, &key.sec.peer_addr);

  if (ble_store_read(BLE_STORE_OBJ_TYPE_PEER_SEC, &key, &value) != 0) {
    return -ENOENT;
  }

  if (info != nullptr) {
    prv_value_to_info(&value.sec, info);
  }

  return 0;
}

static int prv_unpair(const ble_addr_t *peer_addr) {
  // ble_gap_unpair() would also edit the resolving list, preempting advertising.
  int ret = ble_store_util_delete_peer(peer_addr);

  if (ret != 0) {
    PBL_LOG_ERR("Could not unpair: 0x%x", ret);
    return -EIO;
  }

  return 0;
}

struct pbl_bt__collect_ctx {
  ble_addr_t peers[MAX_BONDS];
  size_t count;
};

static void prv_collect_cb(const struct pbl_bt_bond_info *info, void *user_data) {
  struct pbl_bt__collect_ctx *ctx = user_data;

  if (ctx->count < MAX_BONDS) {
    pebble_device_to_nimble_addr(&info->addr, &ctx->peers[ctx->count++]);
  }
}

int pbl_bt_unpair(const struct pbl_bt_device_internal *addr) {
  struct pbl_bt__collect_ctx ctx = {};
  ble_addr_t peer_addr;
  int ret = 0;

  if (addr != nullptr) {
    if (pbl_bt_bond_get(addr, nullptr) != 0) {
      return -ENOENT;
    }
    pebble_device_to_nimble_addr(addr, &peer_addr);
    return prv_unpair(&peer_addr);
  }

  pbl_bt_foreach_bond(prv_collect_cb, &ctx);
  for (size_t i = 0; i < ctx.count; i++) {
    int rc = prv_unpair(&ctx.peers[i]);
    if (rc != 0) {
      ret = rc;
    }
  }

  return ret;
}
