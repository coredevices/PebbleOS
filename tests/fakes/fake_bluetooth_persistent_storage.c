/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bluetooth_persistent_storage.h"

#include "pbl/util/list.h"

#include <pbl/btutil/bt_device.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
  ListNode node;
  pbl_bt_bonding_id_t id;
  struct pbl_bt_sm_key irk;
  bool is_public_address;
  struct pbl_bt_device_internal device;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  bool is_gateway;
} FakeBonding;

static FakeBonding *s_head;
static pbl_bt_bonding_id_t s_next_id = 1;

bool bt_persistent_storage_is_gateway(const pbl_bt_bonding_id_t bonding) {
  return true;
}

static pbl_bt_bonding_id_t prv_next_id(void) {
  return s_next_id++;
}

static bool prv_find_by_id(ListNode *found_node, void *data) {
  pbl_bt_bonding_id_t bonding_id = (pbl_bt_bonding_id_t)data;
  const FakeBonding *bonding = (const FakeBonding *)found_node;
  return (bonding->id == bonding_id);
}

bool bt_persistent_storage_get_ble_pairing_by_id(pbl_bt_bonding_id_t id,
                                                 struct pbl_bt_sm_key *IRK_out,
                                                 struct pbl_bt_device_internal *device_out,
                                                 char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  FakeBonding *bonding =
      (FakeBonding *)list_find(&s_head->node, prv_find_by_id, (void *)(uintptr_t)id);
  if (!bonding) {
    return false;
  }
  if (IRK_out) {
    *IRK_out = bonding->irk;
  }
  if (device_out) {
    *device_out = bonding->device;
  }
  if (name) {
    strncpy(name, bonding->name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
  }
  return true;
}

pbl_bt_bonding_id_t fake_bt_persistent_storage_add(const struct pbl_bt_sm_key *irk,
                                                   const struct pbl_bt_device_internal *device,
                                                   const char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE],
                                                   bool is_gateway) {
  FakeBonding *bonding = (FakeBonding *)malloc(sizeof(FakeBonding));
  *bonding = (const FakeBonding){
    .id = prv_next_id(),
    .irk = *irk,
    .device = *device,
    .is_gateway = is_gateway,
  };
  strncpy(bonding->name, name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
  s_head = (FakeBonding *)list_prepend(&s_head->node, &bonding->node);

  return bonding->id;
}

pbl_bt_bonding_id_t bt_persistent_storage_store_ble_pairing(
    const struct pbl_bt_sm_pairing_info *pairing_info, bool is_gateway, const char *device_name,
    bool requires_address_pinning, uint8_t flags) {
  const struct pbl_bt_sm_key *IRK =
      pairing_info->is_remote_identity_info_valid ? &pairing_info->irk : NULL;
  const struct pbl_bt_device_internal *device =
      pairing_info->is_remote_identity_info_valid ? &pairing_info->identity : NULL;
  if (!device_name) {
    device_name = "Device";
  }
  return fake_bt_persistent_storage_add(IRK, device, device_name, is_gateway);
}

void fake_bt_persistent_storage_reset(void) {
  FakeBonding *bonding = s_head;
  while (bonding) {
    FakeBonding *next = (FakeBonding *)bonding->node.next;
    free(bonding);
    bonding = next;
  }
  s_head = NULL;
  s_next_id = 1;
}

bool bt_persistent_storage_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                        struct pbl_bt_sm_key *key_out) {
  return true;
}

void bt_persistent_storage_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  return;
}
