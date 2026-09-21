/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"

void fake_bt_persistent_storage_reset(void);

pbl_bt_bonding_id_t fake_bt_persistent_storage_add(const struct pbl_bt_sm_key *irk,
                                                   const struct pbl_bt_device_internal *device,
                                                   const char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE],
                                                   bool is_gateway);
