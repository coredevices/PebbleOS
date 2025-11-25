/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/kernel_le_client/ancs/ancs.h"


// -------------------------------------------------------------------------------------------------
// Stub for PRF

void ancs_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                      size_t value_length, BLEGATTError error) {
  return;
}

void ancs_handle_write_response(BLECharacteristic characteristic, BLEGATTError error) {
  return;
}

void ancs_perform_action(uint32_t notification_uid, uint8_t action_id) {
  return;
}

void ancs_handle_service_discovered(BLECharacteristic *characteristics) {
  return;
}

bool ancs_can_handle_characteristic(BLECharacteristic characteristic) {
  return false;
}

void ancs_handle_subscribe(BLECharacteristic subscribed_characteristic,
                           BLESubscription subscription_type, BLEGATTError error) {
  return;
}

void ancs_invalidate_all_references(void) {
}

void ancs_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics) {
}

void ancs_create(void) {
  return;
}

void ancs_destroy(void) {
  return;
}

void ancs_handle_ios9_or_newer_detected(void) {
}

// -------------------------------------------------------------------------------------------------
// Analytics

void analytics_external_collect_ancs_info(void) {
  return;
}
