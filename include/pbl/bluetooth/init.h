/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/dis.h>

#include <stdbool.h>

typedef struct PBL_PACKED BTDriverConfig {
  SM128BitKey root_keys[SMRootKeyTypeNum];
  DisInfo dis_info;
  BTDeviceAddress identity_addr;
  bool is_hrm_supported_and_enabled;
} BTDriverConfig;

//! Function that performs one-time initialization of the BT Driver.
//! The main FW is expected to call this once at boot.
void pbl_bt_init(void);

//! Starts the Bluetooth stack.
//! @return True if the stack started successfully.
bool pbl_bt_start(BTDriverConfig *config);

//! Stops the Bluetooth stack.
void pbl_bt_stop(void);

//! Powers down the BT controller if has yet to be used
void pbl_bt_power_down_controller_on_boot(void);

//! Invoked by the BT driver each time the host (re-)synchronizes with the controller.
//! Consumers can use this to refresh controller state that gets wiped on a host reset
//! (e.g. advertising data and parameters).
extern void pbl_bt_handle_host_resynced(void);
