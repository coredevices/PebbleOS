/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/id.h>

/**
 * @defgroup services_bluetooth_local_id Local identity
 * @ingroup services_bluetooth
 * @brief Local device name and identity address.
 * @{
 */

/**
 * @brief Configure the local name and cache the identity address, right after the stack starts.
 *
 * Uses the stored device name, or "Pebble XXXX" from the last two address bytes.
 */
void bt_local_id_configure_driver(void);

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
 * @param is_le Meant to select the LE name on dual mode devices; currently ignored.
 */
void bt_local_id_copy_device_name(char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE], bool is_le);

/**
 * @brief Copy the local identity address.
 *
 * @param[out] addr_out Address.
 */
void bt_local_id_copy_address(struct pbl_bt_addr *addr_out);

/**
 * @brief Format the local address as a hex string ("0x000000000000").
 *
 * Writes "Unknown" if the address is not known.
 *
 * @param[out] addr_hex_str_out Destination buffer.
 */
void bt_local_id_copy_address_hex_string(char addr_hex_str_out[PBL_BT_BD_ADDR_FMT_BUFFER_SIZE]);

/**
 * @brief Format the local address as a MAC string ("00:00:00:00:00:00").
 *
 * @param[out] addr_mac_str_out Destination buffer.
 */
void bt_local_id_copy_address_mac_string(char addr_mac_str_out[PBL_BT_ADDR_FMT_BUFFER_SIZE]);

/**
 * @brief Derive a static random address from the watch serial number.
 *
 * @param[out] addr_out Generated address.
 */
void bt_local_id_generate_address_from_serial(struct pbl_bt_addr *addr_out);

/** @} */
