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

#include <bluetooth/gap_le_connect.h>
#include <bluetooth/responsiveness.h>

#include "host/ble_gap.h"

void nimble_addr_to_pebble_addr(ble_addr_t *addr, BTDeviceAddress *addr_out) {
  memcpy(&addr_out->octets, &addr->val, BLE_DEV_ADDR_LEN);
}

void pebble_device_to_nimble_addr(const BTDeviceInternal *device, ble_addr_t *addr_out) {
  addr_out->type = device->is_random_address ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
  memcpy(&addr_out->val, &device->address.octets, BLE_DEV_ADDR_LEN);
}

void nimble_addr_to_pebble_device(ble_addr_t *stack_addr, BTDeviceInternal *host_addr) {
  nimble_addr_to_pebble_addr(stack_addr, &host_addr->address);
  host_addr->is_random_address = stack_addr->type == BLE_ADDR_RANDOM;
  host_addr->is_classic = false;
}

bool pebble_device_to_nimble_conn_handle(const BTDeviceInternal *device, uint16_t *handle) {
  ble_addr_t addr;
  struct ble_gap_conn_desc desc;

  pebble_device_to_nimble_addr(device, &addr);

  int rc = ble_gap_conn_find_by_addr(&addr, &desc);
  if (rc == 0) {
    *handle = desc.conn_handle;
  }

  return rc == 0;
}
