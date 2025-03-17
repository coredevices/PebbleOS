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

#include <bluetooth/id.h>
#include <system/passert.h>

#include <host/ble_hs_id.h>
#include <services/gap/ble_svc_gap.h>

void bt_driver_id_set_local_device_name(const char device_name[BT_DEVICE_NAME_BUFFER_SIZE]) {
  int rc = ble_svc_gap_device_name_set(device_name);
  PBL_ASSERTN(rc == 0);
}

void bt_driver_id_copy_local_identity_address(BTDeviceAddress *addr_out) {
  int rc;
  uint8_t own_addr_type;

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  PBL_ASSERTN(rc == 0);

  rc = ble_hs_id_copy_addr(own_addr_type, (uint8_t *)&addr_out->octets, NULL);
  PBL_ASSERTN(rc == 0);
}

void bt_driver_set_local_address(bool allow_cycling, const BTDeviceAddress *pinned_address) {}

void bt_driver_id_copy_chip_info_string(char *dest, size_t dest_size) {
  strncpy(dest, "NimBLE", dest_size);
}

bool bt_driver_id_generate_private_resolvable_address(BTDeviceAddress *address_out) {
  *address_out = (BTDeviceAddress){};
  return true;
}
