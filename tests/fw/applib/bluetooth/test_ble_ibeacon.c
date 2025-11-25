/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/bluetooth/ble_ibeacon.h"
#include <btutil/bt_device.h>

#include "clar.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_rand_ptr.h"
#include "stubs_ble_syscalls.h"

// The test data and descriptions in this file are captured using the FrontLine
// Bluetooth sniffer.

//  AD Element, Length: 26, AD Type: Manufacturer Specific, Manufacturer ID:
//    Apple, Inc. (0x004c) Additional Data: 0x 02 15 97 6e bb 18 d3 e9 43 c0 8a
//    63 8d 2b 60 d9 04 2a 00 0c 00 22 c5

// This excludes the "Manufacturer Specific" opcode (0xff) but it
// includes the Apple Inc company ID:
static BLEAdData *create_apple_ibeacon_ad_data(void) {
  static const uint8_t apple_ibeacon_ad_element[] =
  {
    0x1a, // 26 bytes
    0xff, // Manufacturer Specific AD Type
    0x4c, 0x00, // Apple
    0x02, // iBeacon
    0x15, // Number of bytes to follow
    0x97, 0x6e, 0xbb, 0x18, 0xd3, 0xe9, 0x43, 0xc0, // Proximity UUID
    0x8a, 0x63, 0x8d, 0x2b, 0x60, 0xd9, 0x04, 0x2a,
    0x00, 0x0c, // Minor (BE)
    0x00, 0x22, // Major (BE)
    0xc5 // TX Power
  };
  const size_t ad_data_length = sizeof(apple_ibeacon_ad_element);
  BLEAdData *ad_data = (BLEAdData *) malloc(sizeof(BLEAdData) + ad_data_length);
  ad_data->ad_data_length = ad_data_length;
  ad_data->scan_resp_data_length = 0;
  memcpy(ad_data->data, apple_ibeacon_ad_element, ad_data_length);
  return ad_data;
}

void test_ble_ibeacon__parse_ibeacon_data(void) {
  BLEAdData *apple_ibeacon_ad_data = create_apple_ibeacon_ad_data();
  BLEiBeacon ibeacon;
  const int8_t rssi = -60;
  bool is_ibeacon = ble_ibeacon_parse(apple_ibeacon_ad_data, rssi, &ibeacon);
  cl_assert(is_ibeacon);

  uint8_t uuid_bytes[] = "\x97\x6e\xbb\x18\xd3\xe9\x43\xc0\x8a\x63\x8d\x2b" \
                         "\x60\xd9\x04\x2a";
  Uuid uuid = UuidMakeFromBEBytes(uuid_bytes);
  cl_assert(uuid_equal(&ibeacon.uuid, &uuid));

  cl_assert_equal_i(ibeacon.major, 12);
  cl_assert_equal_i(ibeacon.minor, 34);
  cl_assert_equal_i(ibeacon.rssi, -60);
  cl_assert_equal_i(ibeacon.calibrated_tx_power, -59);
//  cl_assert_equal_i(ibeacon.distance_cm, 110);

  free(apple_ibeacon_ad_data);
}

void test_ble_ibeacon__ibeacon_compose(void) {
  BLEAdData *apple_ibeacon_ad_data = create_apple_ibeacon_ad_data();
  BLEiBeacon ibeacon;
  const int8_t rssi = -60;
  ble_ibeacon_parse(apple_ibeacon_ad_data, rssi, &ibeacon);

  BLEAdData *new_ibeacon_ad_data = ble_ad_create();
  cl_assert_equal_b(ble_ibeacon_compose(&ibeacon, new_ibeacon_ad_data), true);

  const size_t ad_data_size = apple_ibeacon_ad_data->ad_data_length +
                              apple_ibeacon_ad_data->scan_resp_data_length;
  cl_assert_equal_i(new_ibeacon_ad_data->ad_data_length,
                    apple_ibeacon_ad_data->ad_data_length);
  cl_assert_equal_i(new_ibeacon_ad_data->scan_resp_data_length,
                    apple_ibeacon_ad_data->scan_resp_data_length);
  cl_assert_equal_i(memcmp(new_ibeacon_ad_data->data,
                           apple_ibeacon_ad_data->data,
                           ad_data_size), 0);
  free(apple_ibeacon_ad_data);
  ble_ad_destroy(new_ibeacon_ad_data);
}

static BLEAdData *create_too_short_ad_data(void) {
  static const uint8_t too_short_ad_element[] =
  { 0x1a, // 26 bytes
    0xff, // Manufacturer Specific AD Type
    0x4c, 0x00, // Apple
    0x02, // iBeacon
    0x14, // Number of bytes to follow -- Internally inconsistent!
    0x97, 0x6e, 0xbb, 0x18, 0xd3, 0xe9, 0x43, 0xc0, // Proximity UUID
    0x8a, 0x63, 0x8d, 0x2b, 0x60, 0xd9, 0x04, 0x2a,
    0x00, 0x0c, // Minor (BE)
    0x00, 0x22, // Major (BE)
    0xc5 // TX Power
  };

  BLEAdData *ad_data = (BLEAdData *) malloc(sizeof(BLEAdData) +
                                            sizeof(too_short_ad_element));
  ad_data->ad_data_length = sizeof(too_short_ad_element);
  ad_data->scan_resp_data_length = 0;
  return ad_data;
}

void test_ble_ibeacon__ibeacon_data_too_short(void) {
  BLEAdData *too_short_to_ibeacon = create_too_short_ad_data();
  bool is_ibeacon = ble_ibeacon_parse(too_short_to_ibeacon, 0, NULL);
  cl_assert(!is_ibeacon);
  free(too_short_to_ibeacon);
}
