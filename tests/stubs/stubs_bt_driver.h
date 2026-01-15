/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bluetooth/gatt.h"

void bt_driver_classic_update_connectability(void) {
}

bool bt_driver_supports_bt_classic(void) {
  return true;
}

void bt_driver_handle_host_added_cccd(const BleCCCD *cccd) {
}

void bt_driver_handle_host_removed_cccd(const BleCCCD *cccd) {
}
