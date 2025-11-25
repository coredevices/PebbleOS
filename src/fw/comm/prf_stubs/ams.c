/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/kernel_le_client/ams/ams.h"

void ams_create(void) {
}

void ams_handle_service_discovered(BLECharacteristic *characteristics) {
}

bool ams_can_handle_characteristic(BLECharacteristic characteristic) {
  return false;
}

void ams_handle_subscribe(BLECharacteristic subscribed_characteristic,
                          BLESubscription subscription_type, BLEGATTError error) {
}

void ams_handle_write_response(BLECharacteristic characteristic, BLEGATTError error) {
}

void ams_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                     size_t value_length, BLEGATTError error) {
}

void ams_invalidate_all_references(void) {
}

void ams_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics) {
}

void ams_destroy(void) {
}

void ams_send_command(AMSRemoteCommandID command_id) {
}
