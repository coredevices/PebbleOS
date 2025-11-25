/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/id.h>
#include <bluetooth/bluetooth_types.h>

#include <string.h>

void bt_driver_id_set_local_device_name(const char device_name[BT_DEVICE_NAME_BUFFER_SIZE]) {
}

void bt_driver_id_copy_local_identity_address(BTDeviceAddress *addr_out) {
  memset(addr_out, 0xAA, sizeof(*addr_out));
}

void bt_driver_set_local_address(bool allow_cycling,
                                 const BTDeviceAddress *pinned_address) {
}

void bt_driver_id_copy_chip_info_string(char *dest, size_t dest_size) {
  strncpy(dest, "QEMU", dest_size);
}

bool bt_driver_id_generate_private_resolvable_address(BTDeviceAddress *address_out) {
  *address_out = (BTDeviceAddress) {};
  return true;
}
