/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

#include "kernel/events.h"
#include "menu.h"
#include "pbl/util/list.h"

typedef struct GAPLEConnection GAPLEConnection;

typedef struct StoredRemoteBLE {
  pbl_bt_bonding_id_t bonding;
  GAPLEConnection *connection;
#ifdef CONFIG_HRM
  bool is_sharing_heart_rate;
#endif
} StoredRemoteBLE;

typedef struct StoredRemote {
  ListNode list_node;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  StoredRemoteBLE ble;
} StoredRemote;

struct SettingsBluetoothData;

void settings_bluetooth_update_remotes(struct SettingsBluetoothData *data);

const SettingsModuleMetadata *settings_bluetooth_get_info(void);

bool settings_bluetooth_is_sharing_heart_rate_for_stored_remote(StoredRemote *remote);

#define BT_FORGET_PAIRING_STR \
  i18n_noop("Remember to also forget your Pebble's Bluetooth connection from your phone.")
