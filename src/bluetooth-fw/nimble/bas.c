/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/bas.h"
#include "services/bas/ble_svc_bas.h"

void bt_driver_bas_handle_update(uint8_t percent) {
  ble_svc_bas_battery_level_set(percent);
}