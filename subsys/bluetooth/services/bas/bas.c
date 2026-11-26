/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bas.h>
#include <pbl/bluetooth/gatts.h>

#include <services/bas/ble_svc_bas.h>

static int prv_add(void) {
  ble_svc_bas_init();

  return 0;
}

PBL_BT_GATTS_SERVICE_DEFINE(bas, 30, prv_add);

void pbl_bt_bas_handle_update(uint8_t percent) {
  ble_svc_bas_battery_level_set(percent);
}
