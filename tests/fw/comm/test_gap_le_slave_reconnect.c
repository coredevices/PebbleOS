/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/bluetooth/ble_ad_parse.h"
#include "comm/ble/gap_le_advert.h"
#include "comm/ble/gap_le_slave_reconnect.h"
#include "pbl/services/regular_timer.h"

#include "clar.h"

#include <btutil/bt_uuid.h>

#include <string.h>

// Fakes
///////////////////////////////////////////////////////////

#include "fake_bt_driver_advert.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_analytics.h"
#include "stubs_ble_syscalls.h"
#include "stubs_bluetopia_interface.h"
#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"

bool gap_le_connect_is_connected_as_slave(void) {
  return false;
}

bool bt_persistent_storage_has_active_ble_gateway_bonding(void) {
  return false;
}

bool bt_persistent_storage_has_ble_ancs_bonding(void) {
  return false;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

static bool prv_get_ad_flags(const BLEAdData *ad, uint8_t *flags_out) {
  const uint8_t *data = ad->data;
  size_t remaining = ad->ad_data_length;

  while (remaining) {
    const uint8_t element_length = data[0];
    if (element_length == 0 || element_length >= remaining) {
      return false;
    }
    if (data[1] == 0x01 && element_length == 2) {
      *flags_out = data[2];
      return true;
    }
    data += element_length + 1;
    remaining -= element_length + 1;
  }

  return false;
}

// Tests
///////////////////////////////////////////////////////////

void test_gap_le_slave_reconnect__initialize(void) {
  fake_bt_driver_advert_init();
  regular_timer_init();
  gap_le_advert_init();
  gap_le_advert_handle_disconnect_as_slave();
}

void test_gap_le_slave_reconnect__cleanup(void) {
  gap_le_slave_reconnect_hrm_stop();
  gap_le_advert_deinit();
  regular_timer_deinit();
  fake_pbl_malloc_check_net_allocs();
}

void test_gap_le_slave_reconnect__hrm_advertises_heart_rate_service(void) {
  gap_le_slave_reconnect_hrm_start();

  cl_assert_equal_b(true, gap_le_is_advertising_enabled());
  gap_le_assert_advertising_interval(GAPLEAdvertisingInterval_Short);

  Advertising_Data_t ad_data_out;
  const unsigned int ad_data_length = gap_le_get_advertising_data(&ad_data_out);

  uint8_t buffer[sizeof(BLEAdData) + GAP_LE_AD_REPORT_DATA_MAX_LENGTH] = {};
  BLEAdData *ad = (BLEAdData *)buffer;
  ad->ad_data_length = ad_data_length;
  memcpy(ad->data, &ad_data_out, ad_data_length);

  const Uuid hrm_service_uuid = bt_uuid_expand_16bit(0x180D);
  cl_assert_equal_b(true, ble_ad_includes_service(ad, &hrm_service_uuid));

  uint8_t flags = 0;
  cl_assert_equal_b(true, prv_get_ad_flags(ad, &flags));
  cl_assert_equal_i(
      flags, GAP_LE_AD_FLAGS_GEN_DISCOVERABLE_MASK | GAP_LE_AD_FLAGS_BR_EDR_NOT_SUPPORTED_MASK);

  gap_le_set_advertising_disabled();
  gap_le_advert_handle_connect_as_slave();
  cl_assert_equal_b(true, gap_le_is_advertising_enabled());
  gap_le_assert_advertising_interval(GAPLEAdvertisingInterval_Short);

  Scan_Response_Data_t scan_resp_data_out;
  cl_assert_equal_i(0, gap_le_get_scan_response_data(&scan_resp_data_out));

  // The first advertising term lasts 30s; advance the 1-second cycle timer
  // 30 times so the job moves on to its second (Long-interval) term.
  for (int i = 0; i < 30; ++i) {
    regular_timer_fire_seconds(1);
  }
  gap_le_assert_advertising_interval(GAPLEAdvertisingInterval_Long);

  gap_le_slave_reconnect_hrm_stop();
  cl_assert_equal_b(false, gap_le_is_advertising_enabled());
}
