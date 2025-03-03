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

#include <bluetooth/gatt.h>
#include <btutil/bt_device.h>

BTErrno bt_driver_gatt_write_without_response(GAPLEConnection *connection, const uint8_t *value,
                                              size_t value_length, uint16_t att_handle) {
  return 0;
}

BTErrno bt_driver_gatt_write(GAPLEConnection *connection, const uint8_t *value, size_t value_length,
                             uint16_t att_handle, void *context) {
  return 0;
}

BTErrno bt_driver_gatt_read(GAPLEConnection *connection, uint16_t att_handle, void *context) {
  return 0;
}
