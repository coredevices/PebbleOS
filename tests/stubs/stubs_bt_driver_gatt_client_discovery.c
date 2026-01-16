/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "stubs_bt_driver_gatt_client_discovery.h"

#include "fake_GATTAPI.h"

// TODO: Rethink how we want to stub out these new driver wrapper calls.

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection, const ATTHandleRange *data) {
  // Call the fake GATT API so the test can properly track discovery state
  GATT_Attribute_Handle_Group_t range = {
    .Starting_Handle = data->start,
    .Ending_Handle = data->end,
  };
  int ret = GATT_Start_Service_Discovery_Handle_Range(0, 0, &range, 0, NULL, NULL, 0);
  return (ret == 0) ? BTErrnoOK : BTErrnoInternalErrorBegin;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  // Call the fake GATT API so the test can properly track discovery state
  GATT_Stop_Service_Discovery(0, 0);
  return BTErrnoOK;
}

void bt_driver_gatt_handle_finalize_discovery(GAPLEConnection *connection) {
}

void bt_driver_gatt_handle_discovery_abandoned(void) {
}

uint32_t bt_driver_gatt_get_watchdog_timer_id(void) {
  return 0;
}
