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

/**
 * @brief Get a human-readable string identifying the Bluetooth chip.
 *
 * Used by manufacturing for part tracking. The NimBLE backend returns @c "NimBLE".
 *
 * @param[out] dest Buffer to copy the string into.
 * @param dest_size Size of @p dest in bytes.
 */
void pbl_bt_id_copy_chip_info_string(char *dest, size_t dest_size);

/** @} */
