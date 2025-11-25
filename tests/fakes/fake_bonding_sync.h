/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/list.h"

#include "kernel/pbl_malloc.h"
#include "system/passert.h"

#include <bluetooth/bonding_sync.h>
#include <bluetooth/sm_types.h>
#include <btutil/sm_util.h>

typedef struct {
  ListNode node;
  BleBonding bonding;
} BLEBondingNode;

static BLEBondingNode *s_ble_bonding_head;

void bonding_sync_add_bonding(const BleBonding *bonding) {
  BLEBondingNode *node = (BLEBondingNode *) kernel_malloc_check(sizeof(BLEBondingNode));
  *node = (BLEBondingNode) {
    .bonding = *bonding,
  };
  s_ble_bonding_head = (BLEBondingNode *) list_prepend((ListNode *)s_ble_bonding_head,
                                                       (ListNode *)node);
}

void bt_driver_handle_host_added_bonding(const BleBonding *bonding) {
  bonding_sync_add_bonding(bonding);
}

static bool prv_list_find_cb(ListNode *found_list_node, void *data) {
  const BleBonding *bonding = (const BleBonding *)data;
  const BLEBondingNode *found_node = (const BLEBondingNode *)found_list_node;
  const BleBonding *found_bonding = &found_node->bonding;
  return sm_is_pairing_info_equal_identity(&bonding->pairing_info, &found_bonding->pairing_info);
}

static void prv_remove_node(BLEBondingNode *node) {
  list_remove((ListNode *)node, (ListNode **)&s_ble_bonding_head, NULL);
  kernel_free(node);
}

bool bonding_sync_contains_pairing_info(const SMPairingInfo *pairing_info, bool is_gateway) {
  BleBonding bonding = {
    .is_gateway = is_gateway,
    .pairing_info = *pairing_info,
  };
  BLEBondingNode *found_node = (BLEBondingNode *)list_find((ListNode *)s_ble_bonding_head,
                                                           prv_list_find_cb, (void *)&bonding);
  return (found_node != NULL);
}

void bt_driver_handle_host_removed_bonding(const BleBonding *bonding) {
  BLEBondingNode *found_node = (BLEBondingNode *)list_find((ListNode *)s_ble_bonding_head,
                                                           prv_list_find_cb, (void *)bonding);
  PBL_ASSERTN(found_node);
  prv_remove_node(found_node);
}

void bonding_sync_init(void) {
  PBL_ASSERTN(!s_ble_bonding_head);
}

void bonding_sync_deinit(void) {
  while (s_ble_bonding_head) {
    prv_remove_node(s_ble_bonding_head);
  }
}
