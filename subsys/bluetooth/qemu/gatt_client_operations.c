/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/ppog_reversed.h>
#include <pbl/bluetooth/gatt.h>

enum pbl_bt_errno pbl_bt_gatt_write_without_response(GAPLEConnection *connection,
                                                     const uint8_t *value, size_t value_length,
                                                     uint16_t att_handle) {
  return 0;
}

enum pbl_bt_errno pbl_bt_gatt_write(GAPLEConnection *connection, const uint8_t *value,
                                    size_t value_length, uint16_t att_handle, void *context) {
  return 0;
}

enum pbl_bt_errno pbl_bt_gatt_read(GAPLEConnection *connection, uint16_t att_handle,
                                   void *context) {
  return 0;
}

enum pbl_bt_errno pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf,
                                              uint16_t len) {
  return PBL_BT_ERRNO_INVALID_STATE;
}
