/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/classic_connect.h>

void bt_driver_classic_disconnect(const BTDeviceAddress* address) {
}

bool bt_driver_classic_is_connected(void) {
  return false;
}

bool bt_driver_classic_copy_connected_address(BTDeviceAddress* address) {
  return false;
}

bool bt_driver_classic_copy_connected_device_name(char nm[BT_DEVICE_NAME_BUFFER_SIZE]) {
  return false;
}
