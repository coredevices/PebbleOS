/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "menu.h"

#include <pbl/bluetooth/gap.h>
#include <pbl/util/list.h>

#include <kernel/events.h>
#include <nimble/ble.h>

typedef struct StoredRemoteBLE {
  ble_addr_t identity;
  bool connected;
} StoredRemoteBLE;

typedef struct StoredRemote {
  ListNode list_node;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  StoredRemoteBLE ble;
} StoredRemote;

struct SettingsBluetoothData;

void settings_bluetooth_update_remotes(struct SettingsBluetoothData *data);

const SettingsModuleMetadata *settings_bluetooth_get_info(void);

#define BT_FORGET_PAIRING_STR \
  pbl_i18n_noop("Remember to also forget your Pebble's Bluetooth connection from your phone.")
