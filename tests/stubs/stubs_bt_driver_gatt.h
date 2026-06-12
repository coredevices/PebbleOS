/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/gatt.h>
#include "fake_GATTAPI.h"

// TODO: Rethink how we want to stub out these new driver wrapper calls.

void bt_driver_gatt_send_changed_indication(const BTDeviceInternal *device,
                                            const ATTHandleRange *data) {
  GATT_Service_Changed_Data_t all_changed_range = {
    .Affected_Start_Handle = data->start,
    .Affected_End_Handle = data->end,
  };
  // The legacy Bluetopia test stub needs a connection ID to pass through to
  // GATT_Service_Changed_Indication. Look up the connection by device address.
  // If no match is found the indication is silently dropped (matches the stub's
  // no-op behaviour for unresolvable devices).
  uint16_t conn_id = 0;
  GAPLEConnection *connection = gap_le_connection_by_device(device);
  if (connection) {
    conn_id = connection->gatt_connection_id;
  }
  GATT_Service_Changed_Indication(bt_stack_id(), conn_id, &all_changed_range);
}

void bt_driver_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
  GATT_Service_Changed_CCCD_Read_Response(bt_stack_id(),
                                          transaction_id,
                                          response_code);
}
