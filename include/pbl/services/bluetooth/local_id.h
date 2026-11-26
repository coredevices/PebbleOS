/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

/**
 * @defgroup services_bluetooth_local_id Local identity
 * @ingroup services_bluetooth
 * @brief Local device name and identity address.
 * @{
 */

/**
 * @brief Set the local name and read the identity address, right after the stack starts.
 *
 * The name is the one set last, or "Pebble XXXX" from the last two address bytes.
 */
void bt_local_id_handle_start(void);

/**
 * @brief Override the device name.
 *
 * Truncated to @c PBL_BT_DEVICE_NAME_BUFFER_SIZE - 1 characters. Not persisted.
 *
 * @param device_name New name.
 */
void bt_local_id_set_device_name(const char *device_name);

/**
 * @brief Copy the device name.
 *
 * @param[out] name_out Destination buffer.
 */
void bt_local_id_copy_device_name(char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

/**
 * @brief Copy the local identity address.
 *
 * @param[out] addr_out Its octets, least significant first.
 */
void bt_local_id_copy_address(uint8_t addr_out[6]);

/**
 * @brief Format the local identity address ("00:00:00:00:00:00").
 *
 * @param[out] addr_str_out Destination buffer.
 */
void bt_local_id_copy_address_string(char addr_str_out[PBL_BT_ADDR_FMT_BUFFER_SIZE]);

/** @} */
