/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <bluetooth/sm_types.h>
#include <bluetooth/dis.h>

#include <stdbool.h>

typedef struct PACKED BTDriverConfig {
  SM128BitKey root_keys[SMRootKeyTypeNum];
  DisInfo dis_info;
  BTDeviceAddress identity_addr;
  bool is_hrm_supported_and_enabled;
} BTDriverConfig;

//! Function that performs one-time initialization of the BT Driver.
//! The main FW is expected to call this once at boot.
void bt_driver_init(void);

//! Starts the Bluetooth stack.
//! @return True if the stack started successfully.
bool bt_driver_start(BTDriverConfig *config);

//! Stops the Bluetooth stack.
//! @return True if the stack stopped successfully.
void bt_driver_stop(void);

//! Powers down the BT controller if has yet to be used
void bt_driver_power_down_controller_on_boot(void);
