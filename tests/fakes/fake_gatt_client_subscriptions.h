/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gatt_client_subscriptions.h"

void fake_gatt_client_subscriptions_init(void);

void fake_gatt_client_subscriptions_deinit(void);

void fake_gatt_client_subscriptions_set_subscribe_return_value(BTErrno e);

void fake_gatt_client_subscriptions_assert_subscribe(BLECharacteristic characteristic,
                                                     BLESubscription subscription_type,
                                                     GAPLEClient client);
