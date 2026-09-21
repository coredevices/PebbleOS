/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/list.h"

#include "kernel/pbl_malloc.h"
#include "system/passert.h"

#include <pbl/bluetooth/bonding_sync.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/btutil/sm_util.h>

typedef struct {
  ListNode node;
  struct pbl_bt_bonding bonding;
} BLEBondingNode;

static BLEBondingNode *s_ble_bonding_head;

void bonding_sync_add_bonding(const struct pbl_bt_bonding *bonding) {
  BLEBondingNode *node = (BLEBondingNode *)kernel_malloc_check(sizeof(BLEBondingNode));
  *node = (BLEBondingNode){
    .bonding = *bonding,
  };
  s_ble_bonding_head =
      (BLEBondingNode *)list_prepend((ListNode *)s_ble_bonding_head, (ListNode *)node);
}

void pbl_bt_handle_host_added_bonding(const struct pbl_bt_bonding *bonding) {
  bonding_sync_add_bonding(bonding);
}

static bool prv_list_find_cb(ListNode *found_list_node, void *data) {
  const struct pbl_bt_bonding *bonding = (const struct pbl_bt_bonding *)data;
  const BLEBondingNode *found_node = (const BLEBondingNode *)found_list_node;
  const struct pbl_bt_bonding *found_bonding = &found_node->bonding;
  return sm_is_pairing_info_equal_identity(&bonding->pairing_info, &found_bonding->pairing_info);
}

static void prv_remove_node(BLEBondingNode *node) {
  list_remove((ListNode *)node, (ListNode **)&s_ble_bonding_head, NULL);
  kernel_free(node);
}

bool bonding_sync_contains_pairing_info(const struct pbl_bt_sm_pairing_info *pairing_info,
                                        bool is_gateway) {
  struct pbl_bt_bonding bonding = {
    .is_gateway = is_gateway,
    .pairing_info = *pairing_info,
  };
  BLEBondingNode *found_node = (BLEBondingNode *)list_find((ListNode *)s_ble_bonding_head,
                                                           prv_list_find_cb, (void *)&bonding);
  return (found_node != NULL);
}

void pbl_bt_handle_host_removed_bonding(const struct pbl_bt_bonding *bonding) {
  // Match the qemu/stub driver behavior: removing a bonding the driver was never told about is a
  // no-op rather than a fault. Production code may issue a remove even when the driver-side state
  // was never populated (e.g. cleaning up stale entries at boot).
  BLEBondingNode *found_node = (BLEBondingNode *)list_find((ListNode *)s_ble_bonding_head,
                                                           prv_list_find_cb, (void *)bonding);
  if (found_node) {
    prv_remove_node(found_node);
  }
}

void bonding_sync_init(void) {
  PBL_ASSERTN(!s_ble_bonding_head);
}

void bonding_sync_deinit(void) {
  while (s_ble_bonding_head) {
    prv_remove_node(s_ble_bonding_head);
  }
}
