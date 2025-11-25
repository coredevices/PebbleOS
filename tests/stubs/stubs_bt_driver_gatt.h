/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/gatt.h>
#include "fake_GATTAPI.h"

// TODO: Rethink how we want to stub out these new driver wrapper calls.

void bt_driver_gatt_send_changed_indication(uint32_t connection_id, const ATTHandleRange *data) {
  GATT_Service_Changed_Data_t all_changed_range = {
    .Affected_Start_Handle = data->start,
    .Affected_End_Handle = data->end,
  };
  GATT_Service_Changed_Indication(bt_stack_id(), connection_id, &all_changed_range);
}

void bt_driver_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
  GATT_Service_Changed_CCCD_Read_Response(bt_stack_id(),
                                          transaction_id,
                                          response_code);
}
