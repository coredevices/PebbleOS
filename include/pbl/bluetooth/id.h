/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

void pbl_bt_id_set_local_device_name(const char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

void pbl_bt_id_copy_local_identity_address(struct pbl_bt_addr *addr_out);

//! Configures the local address that the BT driver should use "on-air".
//! @note This address and the identity address are different things!
//! @note bt_lock() is held when this call is made.
//! @param allow_cycling True if the controller is allowed to cycle the address (implies address
//! pinning is *not* used!)
//! @param pinned_address The address to use, or NULL for "don't care".
void pbl_bt_set_local_address(bool allow_cycling, const struct pbl_bt_addr *pinned_address);

//! Copies a human-readable string of freeform info that uniquely identifies the Bluetooth chip.
//! Used by MFG for part tracking purposes.
//! @param[out] dest Buffer into which to copy the info.
//! @param[in] dest_size Size of dest in bytes.
void pbl_bt_id_copy_chip_info_string(char *dest, size_t dest_size);

//! Generates a new private resolvable address using the current IRK (as passed with the
//! pbl_bt_start() call when setting up the stack).
bool pbl_bt_id_generate_private_resolvable_address(struct pbl_bt_addr *address_out);
