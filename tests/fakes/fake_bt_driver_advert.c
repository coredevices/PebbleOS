/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bt_driver_advert.h"

#include <bluetooth/bt_driver_advert.h>

#include <string.h>

#include "clar_asserts.h"

// Simulated state of the BT controller, as driven through the bt_driver_advert
// contract by gap_le_advert.c.
static bool s_is_advertising_enabled;
static uint32_t s_min_advertising_interval_ms;
static uint32_t s_max_advertising_interval_ms;

static Advertising_Data_t s_ad_data;
static unsigned int s_ad_data_length;

static Scan_Response_Data_t s_scan_resp_data;
static unsigned int s_scan_resp_data_length;

void fake_bt_driver_advert_init(void) {
  s_is_advertising_enabled = false;
  s_min_advertising_interval_ms = 0;
  s_max_advertising_interval_ms = 0;
  memset(&s_ad_data, 0, sizeof(s_ad_data));
  s_ad_data_length = 0;
  memset(&s_scan_resp_data, 0, sizeof(s_scan_resp_data));
  s_scan_resp_data_length = 0;
}

void gap_le_set_advertising_disabled(void) {
  // Simulate the controller stopping advertising on an inbound connection,
  // without touching the configured payload.
  s_is_advertising_enabled = false;
  s_min_advertising_interval_ms = 0;
  s_max_advertising_interval_ms = 0;
}

// -- bt_driver_advert contract -------------------------------------------------

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms) {
  s_is_advertising_enabled = true;
  s_min_advertising_interval_ms = min_interval_ms;
  s_max_advertising_interval_ms = max_interval_ms;
  return true;
}

void bt_driver_advert_advertising_disable(void) {
  s_is_advertising_enabled = false;
  s_min_advertising_interval_ms = 0;
  s_max_advertising_interval_ms = 0;
}

bool bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
  if (!ad_data) {
    return false;
  }
  // The payload concatenates the ad data and the scan response data; split them
  // back out the way the controller would receive them separately.
  memcpy(&s_ad_data, ad_data->data, ad_data->ad_data_length);
  s_ad_data_length = ad_data->ad_data_length;
  memcpy(&s_scan_resp_data, ad_data->data + ad_data->ad_data_length,
         ad_data->scan_resp_data_length);
  s_scan_resp_data_length = ad_data->scan_resp_data_length;
  return true;
}

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) {
  // No tx-power source in the fake; gap_le_advert keeps its cached value.
  return false;
}

// -- test accessors ------------------------------------------------------------

bool gap_le_is_advertising_enabled(void) {
  return s_is_advertising_enabled;
}

// Expected ms values for each interval preset, matching s_interval_ms in
// gap_le_advert.c.
static const uint32_t s_expected_interval_ms[] = {
  [GAPLEAdvertisingInterval_Short] = 20,
  [GAPLEAdvertisingInterval_Long]  = 1022,
};

void gap_le_assert_advertising_interval(GAPLEAdvertisingInterval expected) {
  const uint32_t expected_ms = s_expected_interval_ms[expected];
  cl_assert_equal_i(s_min_advertising_interval_ms, expected_ms);
  cl_assert_equal_i(s_max_advertising_interval_ms, expected_ms);
}

unsigned int gap_le_get_advertising_data(Advertising_Data_t *ad_data_out) {
  *ad_data_out = s_ad_data;
  return s_ad_data_length;
}

unsigned int gap_le_get_scan_response_data(Scan_Response_Data_t *scan_resp_data_out) {
  *scan_resp_data_out = s_scan_resp_data;
  return s_scan_resp_data_length;
}
