/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/bluetooth/bluetooth_persistent_storage.h"

typedef struct BTDeviceAddress BTDeviceAddress;

//! Pauses cycling of local Private Resolvable Address (ref counted).
//! As long as the cycling is paused, the address that is used "on air" will be stable for the
//! duration that the BT stack is up (so the address can be expected to have changed after rebooting
//! or resetting the stack).
//! In case the local address is currently pinned, this function will be a no-op.
void bt_local_addr_pause_cycling(void);

//! Resumes cycling of local Private Resolvable Address (ref counted).
//! In case the local address is currently pinned, this function will be a no-op.
void bt_local_addr_resume_cycling(void);

//! Called by BT driver to indicate what the local address was that was used during the pairing
//! and pinning was requested. See comment in the implementation for more details.
void bt_local_addr_pin(const BTDeviceAddress *addr);

//! Handler for bonding changes (deletions primarily).
void bt_local_addr_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op);

//! Called during the BT stack initialization.
void bt_local_addr_init(void);
