/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_connection_info.h"

#include "syscall/syscall.h"

bool bluetooth_connection_get_rssi(int8_t *rssi_out) {
  return sys_bluetooth_connection_get_rssi(rssi_out);
}
