/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/types.h>

//! @file fake_gatt_client_types.h
//! What the fakes of GATT client operations and subscriptions record: tests of the clients of a
//! peer's services implement the client they use on these, and assert on what was read, written
//! and subscribed to, by a reference that stands for a characteristic.

typedef uintptr_t pbl_bt_characteristic_t;

typedef enum {
  BLESubscriptionNone = 0,
  BLESubscriptionNotifications = (1 << 0),
  BLESubscriptionIndications = (1 << 1),
  BLESubscriptionAny = BLESubscriptionNotifications | BLESubscriptionIndications,
} BLESubscription;

int gatt_client_op_read(pbl_bt_characteristic_t characteristic);

int gatt_client_op_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                         size_t value_length);

int gatt_client_op_write_without_response(pbl_bt_characteristic_t characteristic,
                                          const uint8_t *value, size_t value_length);

int gatt_client_subscriptions_subscribe(pbl_bt_characteristic_t characteristic,
                                        BLESubscription subscription_type);
