/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/bluetooth/ble_client.h"

struct Transport;

typedef enum {
  PPoGATTCharacteristicData,
  PPoGATTCharacteristicMeta,
  PPoGATTCharacteristicNum
} PPoGATTCharacteristic;

void ppogatt_create(void);

void ppogatt_handle_service_discovered(pbl_bt_characteristic_t *characteristics);

bool ppogatt_can_handle_characteristic(pbl_bt_characteristic_t characteristic);

void ppogatt_handle_subscribe(pbl_bt_characteristic_t subscribed_characteristic,
                              BLESubscription subscription_type, enum pbl_bt_gatt_error error);

void ppogatt_handle_read_or_notification(pbl_bt_characteristic_t characteristic,
                                         const uint8_t *value, size_t value_length,
                                         enum pbl_bt_gatt_error error);

void ppogatt_handle_service_removed(pbl_bt_characteristic_t *characteristics,
                                    uint8_t num_characteristics);

void ppogatt_invalidate_all_references(void);

//! Interface for kernel_le_client, to handle the event that the Bluetooth stack has space available
//! again in its outbound queue. It will trigger the PPoGATT module to send out the next packet(s).
void ppogatt_handle_buffer_empty(void);

//! Interface for CommSession, to let it signal the PPoGATT transport that data has been written
//! into the SendBuffer and can be sent out.
void ppogatt_send_next(struct Transport *transport);

void ppogatt_close(struct Transport *transport);

void ppogatt_reset(struct Transport *transport);

void ppogatt_destroy(void);

//! Interface for analytics
void ppogatt_reset_disconnect_counter(void);
