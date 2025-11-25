/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bluetooth_persistent_storage.h"

#include "util/list.h"

#include <btutil/bt_device.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
  ListNode node;
  BTBondingID id;
  SMIdentityResolvingKey irk;
  bool is_public_address;
  BTDeviceInternal device;
  char name[BT_DEVICE_NAME_BUFFER_SIZE];
  bool is_gateway;
} FakeBonding;

static FakeBonding *s_head;
static BTBondingID s_next_id = 1;

bool bt_persistent_storage_is_gateway(const BTBondingID bonding) {
  return true;
}

static BTBondingID prv_next_id(void) {
  return s_next_id++;
}

static bool prv_find_by_id(ListNode *found_node, void *data) {
  BTBondingID bonding_id = (BTBondingID) data;
  const FakeBonding *bonding = (const FakeBonding *) found_node;
  return (bonding->id == bonding_id);
}

bool bt_persistent_storage_get_ble_pairing_by_id(BTBondingID id,
                                          SMIdentityResolvingKey *IRK_out,
                                          BTDeviceInternal *device_out,
                                          char name[BT_DEVICE_NAME_BUFFER_SIZE]) {
  FakeBonding *bonding = (FakeBonding *) list_find(&s_head->node, prv_find_by_id,
                                                   (void *) (uintptr_t) id);
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
    strncpy(name, bonding->name, BT_DEVICE_NAME_BUFFER_SIZE);
  }
  return true;
}

BTBondingID fake_bt_persistent_storage_add(const SMIdentityResolvingKey *irk,
                                     const BTDeviceInternal *device,
				     const char name[BT_DEVICE_NAME_BUFFER_SIZE],
                                     bool is_gateway) {
  FakeBonding *bonding = (FakeBonding *) malloc(sizeof(FakeBonding));
  *bonding = (const FakeBonding) {
    .id = prv_next_id(),
    .irk = *irk,
    .device = *device,
    .is_gateway = is_gateway,
  };
  strncpy(bonding->name, name, BT_DEVICE_NAME_BUFFER_SIZE);
  s_head = (FakeBonding *) list_prepend(&s_head->node, &bonding->node);

  return bonding->id;
}

BTBondingID bt_persistent_storage_store_ble_pairing(const SMPairingInfo *pairing_info, bool is_gateway,
					     const char *device_name, bool requires_address_pinning, uint8_t flags) {
  const SMIdentityResolvingKey *IRK = pairing_info->is_remote_identity_info_valid ?
        &pairing_info->irk : NULL;
  const BTDeviceInternal *device = pairing_info->is_remote_identity_info_valid ?
        &pairing_info->identity : NULL;
  if (!device_name) {
    device_name = "Device";
  }
  return fake_bt_persistent_storage_add(IRK, device, device_name, is_gateway);
}

void fake_bt_persistent_storage_reset(void) {
  FakeBonding *bonding = s_head;
  while (bonding) {
    FakeBonding *next = (FakeBonding *) bonding->node.next;
    free(bonding);
    bonding = next;
  }
  s_head = NULL;
  s_next_id = 1;
}

bool bt_persistent_storage_get_root_key(SMRootKeyType key_type, SM128BitKey *key_out) {
  return true;
}

void bt_persistent_storage_set_root_keys(SM128BitKey *keys_in) {
  return;
}
