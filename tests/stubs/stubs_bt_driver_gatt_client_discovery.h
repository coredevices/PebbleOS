/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/gatt.h>
#include "fake_GATTAPI.h"

// TODO: Rethink how we want to stub out these new driver wrapper calls.

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection, const ATTHandleRange *data) {
  GATT_Attribute_Handle_Group_t hdl = {
    .Starting_Handle = data->start,
    .Ending_Handle = data->end,
  };

  int rv = GATT_Start_Service_Discovery_Handle_Range(bt_stack_id(), connection->gatt_connection_id,
                                                     &hdl, 0, NULL, NULL, 0);
  return 0;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  GATT_Stop_Service_Discovery(bt_stack_id(), connection->gatt_connection_id);
  return 0;
}

void bt_driver_gatt_handle_finalize_discovery(GAPLEConnection *connection) {
}

void bt_driver_gatt_handle_discovery_abandoned(void) {}
