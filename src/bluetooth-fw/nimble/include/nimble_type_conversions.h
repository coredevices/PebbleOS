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

#pragma once

#include <bluetooth/gap_le_connect.h>
#include <bluetooth/responsiveness.h>

#include "host/ble_gap.h"

void nimble_addr_to_pebble_addr(ble_addr_t *addr, BTDeviceAddress *addr_out);

void pebble_device_to_nimble_addr(const BTDeviceInternal *device, ble_addr_t *addr_out);

void nimble_addr_to_pebble_device(ble_addr_t *stack_addr, BTDeviceInternal *host_addr);

bool pebble_device_to_nimble_conn_handle(const BTDeviceInternal *device, uint16_t *handle);
