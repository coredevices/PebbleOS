/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/bluetooth/ble_client.h"

#include "comm/ble/gap_le_task.h"

void fake_gatt_client_op_init(void);

void fake_gatt_client_op_deinit(void);

void fake_gatt_client_op_set_read_return_value(BTErrno e);

void fake_gatt_client_op_assert_read(BLECharacteristic characteristic,
                                     GAPLEClient client);

void fake_gatt_client_op_set_write_return_value(BTErrno e);

void fake_gatt_client_op_clear_write_list(void);

void fake_gatt_client_op_assert_no_write(void);

void fake_gatt_client_op_assert_write(BLECharacteristic characteristic,
                                      const uint8_t *value, size_t value_length,
                                      GAPLEClient client, bool is_response_required);
