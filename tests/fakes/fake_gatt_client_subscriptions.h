/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "fake_gatt_client_types.h"

#include <pbl/util/list.h>

void fake_gatt_client_subscriptions_init(void);

void fake_gatt_client_subscriptions_deinit(void);

void fake_gatt_client_subscriptions_set_subscribe_return_value(int ret);

void fake_gatt_client_subscriptions_assert_subscribe(pbl_bt_characteristic_t characteristic,
                                                     BLESubscription subscription_type);
