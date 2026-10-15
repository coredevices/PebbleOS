/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

void fake_gatt_client_op_init(void);

void fake_gatt_client_op_deinit(void);

void fake_gatt_client_op_set_read_return_value(enum pbl_bt_errno e);

void fake_gatt_client_op_assert_read(pbl_bt_characteristic_t characteristic);

void fake_gatt_client_op_set_write_return_value(enum pbl_bt_errno e);

void fake_gatt_client_op_clear_write_list(void);

void fake_gatt_client_op_assert_no_write(void);

void fake_gatt_client_op_assert_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                      size_t value_length, bool is_response_required);
