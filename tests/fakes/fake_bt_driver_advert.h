/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>
#include "comm/ble/gap_le_advert.h"

#include <stdbool.h>
#include <stdint.h>

//! Byte buffers mirroring what the controller would hold. Sized to the maximum
//! advertising report length, matching GAP_LE_AD_REPORT_DATA_MAX_LENGTH.
typedef struct {
  uint8_t data[GAP_LE_AD_REPORT_DATA_MAX_LENGTH];
} Advertising_Data_t;

typedef struct {
  uint8_t data[GAP_LE_AD_REPORT_DATA_MAX_LENGTH];
} Scan_Response_Data_t;

//! Resets all simulated controller state. Call from test initialize().
void fake_bt_driver_advert_init(void);

//! Simulates the controller dropping advertising due to an inbound connection,
//! without clearing the configured ad/scan-response payload.
void gap_le_set_advertising_disabled(void);

//! True iff the simulated controller currently has advertising enabled.
bool gap_le_is_advertising_enabled(void);

//! Asserts the advertising interval last programmed into the controller matches
//! the ms value for the given GAPLEAdvertisingInterval preset.
void gap_le_assert_advertising_interval(GAPLEAdvertisingInterval expected);

//! Copies out the advertising payload last programmed into the controller.
//! @return the length, in bytes, of the advertising payload.
unsigned int gap_le_get_advertising_data(Advertising_Data_t *ad_data_out);

//! Copies out the scan-response payload last programmed into the controller.
//! @return the length, in bytes, of the scan-response payload.
unsigned int gap_le_get_scan_response_data(Scan_Response_Data_t *scan_resp_data_out);
