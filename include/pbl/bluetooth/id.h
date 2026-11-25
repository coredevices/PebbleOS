/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_id Local identity
 * @ingroup bluetooth
 * @brief Local device name and addresses.
 * @{
 */

/**
 * @brief Set the local device name, served by the GAP service.
 *
 * @param device_name NUL-terminated name.
 */
void pbl_bt_id_set_local_device_name(const char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

/**
 * @brief Get the local identity address.
 *
 * @param[out] addr_out The address.
 */
void pbl_bt_id_copy_local_identity_address(struct pbl_bt_addr *addr_out);

/** @} */
