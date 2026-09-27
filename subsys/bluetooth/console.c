/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <console/prompt.h>
#include <host/ble_hs.h>

void command_ble_host_reset(void) {
  ble_hs_sched_reset(BLE_HS_EAPP);
  prompt_send_response("OK");
}
