/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/bluetooth/bluetooth_persistent_storage.h"

void fake_bt_persistent_storage_reset(void);

BTBondingID fake_bt_persistent_storage_add(const SMIdentityResolvingKey *irk,
				    const BTDeviceInternal *device,
				    const char name[BT_DEVICE_NAME_BUFFER_SIZE],
				    bool is_gateway);
