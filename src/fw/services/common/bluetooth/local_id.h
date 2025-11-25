/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/id.h>

//! Called by bl_ctl right after the stack starts, to configure the local device name and address.
void bt_local_id_configure_driver(void);

//! Sets a new device name, overriding the existing (default) one.
//! The name will be truncated to BT_DEVICE_NAME_BUFFER_SIZE - 1 characters.
void bt_local_id_set_device_name(const char *device_name);

//! Copies the name of the local device into the given buffer.
//! @param is_le Only consumed if the device used is dual mode. If so,
//               this changes the name returned
void bt_local_id_copy_device_name(char name_out[BT_DEVICE_NAME_BUFFER_SIZE], bool is_le);

//! Copies the address of the local device.
void bt_local_id_copy_address(BTDeviceAddress *addr_out);

//! Copies a hex-formatted string representation ("0x000000000000") of the device address into the
//! given buffer. The buffer should be at least BT_ADDR_FMT_BUFFER_SIZE_BYTES bytes in size to fit
//! the address string.
//! If there is no local address known, the string "Unknown" will be copied into the buffer.
void bt_local_id_copy_address_hex_string(char addr_hex_str_out[BT_ADDR_FMT_BUFFER_SIZE_BYTES]);

//! Copies a MAC-formatted string representation ("00:00:00:00:00:00") of the device address into
//! the given buffer. The buffer should be at least BT_ADDR_FMT_BUFFER_SIZE_BYTES bytes in size to
//! fit the address string.
void bt_local_id_copy_address_mac_string(char addr_mac_str_out[BT_DEVICE_ADDRESS_FMT_BUFFER_SIZE]);

//! Generates a BTDeviceAddress from the serial number of the watch
void bt_local_id_generate_address_from_serial(BTDeviceAddress *addr_out);
