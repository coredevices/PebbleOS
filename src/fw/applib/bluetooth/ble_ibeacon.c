/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_ibeacon.h"

#include "applib/applib_malloc.auto.h"
#include "util/net.h"

#include <string.h>

// -----------------------------------------------------------------------------
//! Apple's iBeacon AD DATA format.
//! The byte-order of Apple's fields (uuid, major and minor) is Big Endian (!!!)
//! @see Apple's docs for more info: http://goo.gl/iOrnpj
//! @see StackOverflow distance/accuracy calculations: http://goo.gl/yH0ubM
static const uint16_t COMPANY_ID_APPLE = 0x004c;
static const uint8_t APPLE_TYPE_IBEACON = 0x02;
static const uint8_t APPLE_IBEACON_LENGTH = 0x15;

typedef struct __attribute__((__packed__)) {
  //! @see APPLE_AD_TYPE_IBEACON
  uint8_t type;

  //! @see APPLE_IBEACON_LENGTH
  uint8_t length;

  //! The application "proximityUUID" of the iBeacon. Generally, multiple
  //! iBeacons share one UUID and an (iOS) app scans for one particular UUID.
  uint8_t uuid[16];

  //! The most significant value in the beacon.
  uint16_t major;

  //! The least significant value in the beacon.
  uint16_t minor;

  //! The calibrated transmit power.
  int8_t calibrated_tx_power;
} AdDataManufacturerSpecificAppleiBeacon;

// -----------------------------------------------------------------------------
// Accessors

Uuid ble_ibeacon_get_uuid(const BLEiBeacon *ibeacon) {
  return ibeacon->uuid;
}

uint16_t ble_ibeacon_get_major(const BLEiBeacon *ibeacon) {
  return ibeacon->major;
}

uint16_t ble_ibeacon_get_minor(const BLEiBeacon *ibeacon) {
  return ibeacon->minor;
}

uint16_t ble_ibeacon_get_distance_cm(const BLEiBeacon *ibeacon) {
  return ibeacon->distance_cm;
}

BLEiBeacon *ble_ibeacon_create_from_ad_data(const BLEAdData *ad,
                                            int8_t rssi) {
  // Note, not yet exported to 3rd party apps so no padding necessary
  BLEiBeacon *ibeacon = applib_malloc(sizeof(BLEiBeacon));
  if (ibeacon && !ble_ibeacon_parse(ad, rssi, ibeacon)) {
    // Failed to parse.
    applib_free(ibeacon);
    ibeacon = NULL;
  }
  return ibeacon;
}

void ble_ibeacon_destroy(BLEiBeacon *ibeacon) {
  applib_free(ibeacon);
}

// -----------------------------------------------------------------------------
// Below is the iBeacon advertisement parsing code.

static uint16_t calculate_distance_cm(int8_t tx_power, int8_t rssi) {
  return 0; // TODO
}

// -----------------------------------------------------------------------------
//! iBeacon Advertisement Data parser
bool ble_ibeacon_parse(const BLEAdData *ad, int8_t rssi,
                       BLEiBeacon *ibeacon_out) {
  uint16_t company_id = 0;
  AdDataManufacturerSpecificAppleiBeacon raw_ibeacon;
  const size_t size_copied =
      ble_ad_copy_manufacturer_specific_data(ad, &company_id,
                                             (uint8_t *) &raw_ibeacon,
                                             sizeof(raw_ibeacon));
  if (size_copied != sizeof(raw_ibeacon)) {
    return false;
  }

  if (company_id == COMPANY_ID_APPLE &&
      raw_ibeacon.type == APPLE_TYPE_IBEACON &&
      raw_ibeacon.length == APPLE_IBEACON_LENGTH) {

    const int8_t tx_power = raw_ibeacon.calibrated_tx_power;
    *ibeacon_out = (const BLEiBeacon) {
      .uuid = UuidMakeFromBEBytes(raw_ibeacon.uuid),
      .major = ntohs(raw_ibeacon.major),
      .minor = ntohs(raw_ibeacon.minor),
      .distance_cm = calculate_distance_cm(tx_power, rssi),
      .rssi = rssi,
      .calibrated_tx_power = tx_power,
    };
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
//! iBeacon Advertisement Data composer
bool ble_ibeacon_compose(const BLEiBeacon *ibeacon_in, BLEAdData *ad_out) {
  AdDataManufacturerSpecificAppleiBeacon raw_ibeacon = {
    .type = APPLE_TYPE_IBEACON,
    .length = APPLE_IBEACON_LENGTH,
    // Major/Minor are part of Apple's iBeacon spec and are Big Endian!
    .major = htons(ibeacon_in->major),
    .minor = htons(ibeacon_in->minor),
    .calibrated_tx_power = ibeacon_in->calibrated_tx_power,
  };
  // Uuid is stored Big Endian on Pebble, so just copy over:
  memcpy(&raw_ibeacon.uuid, &ibeacon_in->uuid, sizeof(Uuid));

  return ble_ad_set_manufacturer_specific_data(ad_out, COMPANY_ID_APPLE,
                                               (uint8_t *) &raw_ibeacon,
                                               sizeof(raw_ibeacon));
}
