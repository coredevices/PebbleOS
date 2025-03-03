/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/id.h>
#include <string.h>

void bt_driver_id_set_local_device_name(const char device_name[BT_DEVICE_NAME_BUFFER_SIZE]) {}

void bt_driver_id_copy_local_identity_address(BTDeviceAddress *addr_out) {
  memset(addr_out, 0xAA, sizeof(*addr_out));
}

void bt_driver_set_local_address(bool allow_cycling, const BTDeviceAddress *pinned_address) {}

void bt_driver_id_copy_chip_info_string(char *dest, size_t dest_size) {
  strncpy(dest, "QEMU", dest_size);
}

bool bt_driver_id_generate_private_resolvable_address(BTDeviceAddress *address_out) {
  *address_out = (BTDeviceAddress){};
  return true;
}
