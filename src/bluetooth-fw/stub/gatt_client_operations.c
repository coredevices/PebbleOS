/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/gatt.h>

#include <btutil/bt_device.h>

BTErrno bt_driver_gatt_write_without_response(GAPLEConnection *connection,
                                              const uint8_t *value,
                                              size_t value_length,
                                              uint16_t att_handle) {
  return 0;
}

BTErrno bt_driver_gatt_write(GAPLEConnection *connection,
                             const uint8_t *value,
                             size_t value_length,
                             uint16_t att_handle,
                             void *context) {
  return 0;
}

BTErrno bt_driver_gatt_read(GAPLEConnection *connection,
                            uint16_t att_handle,
                            void *context) {
  return 0;
}
