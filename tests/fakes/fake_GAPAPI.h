/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "GAPAPI.h"

#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>
#include <stdint.h>

//! Provided to simulate stopping advertising because of an inbound connection.
void gap_le_set_advertising_disabled(void);

bool gap_le_is_advertising_enabled(void);

void gap_le_assert_advertising_interval(uint16_t expected_min_slots, uint16_t expected_max_slots);

unsigned int gap_le_get_advertising_data(Advertising_Data_t *ad_data_out);
unsigned int gap_le_get_scan_response_data(Scan_Response_Data_t *scan_resp_data_out);

void fake_gap_put_connection_event(uint8_t status, bool is_master, const BTDeviceInternal *device);

void fake_gap_put_disconnection_event(uint8_t status, uint8_t reason, bool is_master,
                                      const BTDeviceInternal *device);

void fake_GAPAPI_put_encryption_change_event(bool encrypted, uint8_t status, bool is_master,
                                             const BTDeviceInternal *device);

void fake_gap_le_put_cancel_create_event(const BTDeviceInternal *device, bool is_master);

void fake_GAPAPI_set_encrypted_for_device(const BTDeviceInternal *device);

const Encryption_Key_t *fake_GAPAPI_get_fake_irk(void);

const BD_ADDR_t *fake_GAPAPI_get_bd_addr_not_resolving_to_fake_irk(void);

const BTDeviceInternal *fake_GAPAPI_get_device_not_resolving_to_fake_irk(void);

const BD_ADDR_t *fake_GAPAPI_get_bd_addr_resolving_to_fake_irk(void);

const BTDeviceInternal *fake_GAPAPI_get_device_resolving_to_fake_irk(void);

void fake_GAPAPI_init(void);
